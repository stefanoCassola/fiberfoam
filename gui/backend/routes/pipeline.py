"""Pipeline orchestration: single-geometry pipelines and batch processing."""
import asyncio
import os
import re
import uuid
from datetime import datetime, timezone

from fastapi import APIRouter, HTTPException

from schemas import (
    PipelineRequest,
    PipelineStatus,
    PipelineStepStatus,
    PipelineModeEnum,
    BatchRequest,
    BatchStatus,
    JobStatusEnum,
)
from services.executor import job_manager
from services.config_writer import (
    write_mesh_config,
    write_prediction_config,
    write_simulation_config,
)
from services.paths import (
    MESH_BIN,
    PREDICT_BIN,
    SOLVER_BIN,
    WORK_DIR,
    BATCH_DIR,
    UPLOAD_DIR,
    OPENFOAM_BASHRC,
    MODELS_DIR,
    OUTPUT_ROOT,
)
from services import job_store

router = APIRouter()

# In-memory pipeline and batch state
_pipelines: dict[str, dict] = {}
_batches: dict[str, dict] = {}


# ---------------------------------------------------------------------------
# Pipeline endpoints
# ---------------------------------------------------------------------------


@router.post("/run")
async def run_pipeline(req: PipelineRequest):
    """Launch an async pipeline.  Returns immediately with a pipelineId."""
    # Resolve relative paths against the upload directory
    if not os.path.isabs(req.inputPath):
        req.inputPath = os.path.join(UPLOAD_DIR, req.inputPath)

    if not os.path.isfile(req.inputPath):
        raise HTTPException(
            status_code=400, detail=f"Input geometry not found: {req.inputPath}"
        )

    pipeline_id = str(uuid.uuid4())[:8]
    directions = [d.value for d in req.flowDirections]

    # Build step list based on mode
    steps = _build_step_list(req.mode, directions)

    state = {
        "pipelineId": pipeline_id,
        "mode": req.mode.value,
        "steps": [
            {"name": s, "status": "pending", "jobId": None, "log": []}
            for s in steps
        ],
        "currentStep": None,
        "status": "running",
        "results": {},
        "caseDirs": [],
        "createdAt": datetime.now(timezone.utc).isoformat(),
        "config": req.model_dump(),
    }
    _pipelines[pipeline_id] = state

    job_store.save_job(pipeline_id, {
        "id": pipeline_id,
        "type": "pipeline",
        "status": "running",
        "mode": req.mode.value,
        "config": req.model_dump(),
    })

    asyncio.create_task(_execute_pipeline(pipeline_id, req, directions))

    return _to_pipeline_status(state)


@router.get("/status/{pipeline_id}")
async def pipeline_status(pipeline_id: str):
    """Return the current pipeline status with per-step details."""
    state = _pipelines.get(pipeline_id)
    if not state:
        raise HTTPException(status_code=404, detail="Pipeline not found")
    return _to_pipeline_status(state)


@router.post("/cancel/{pipeline_id}")
async def cancel_pipeline(pipeline_id: str):
    """Cancel the currently running step of a pipeline."""
    state = _pipelines.get(pipeline_id)
    if not state:
        raise HTTPException(status_code=404, detail="Pipeline not found")
    if state["status"] != "running":
        raise HTTPException(status_code=400, detail="Pipeline is not running")

    # Find the current running step and cancel its job
    for step in state["steps"]:
        if step["status"] == "running" and step["jobId"]:
            await job_manager.cancel_job(step["jobId"])
            step["status"] = "failed"
            step["log"].append("--- cancelled by user ---")

    state["status"] = "failed"
    state["currentStep"] = None
    _persist_pipeline(state)

    return {"pipelineId": pipeline_id, "status": "cancelled"}


@router.post("/stop-solver/{pipeline_id}")
async def stop_solver(pipeline_id: str):
    """Gracefully stop the running solver: write the current timestep and exit.

    Modifies the OpenFOAM controlDict to set ``stopAt writeNow;`` so the
    solver finishes the current iteration, writes all fields, and exits
    cleanly.  The pipeline then continues with result extraction as usual.
    """
    state = _pipelines.get(pipeline_id)
    if not state:
        raise HTTPException(status_code=404, detail="Pipeline not found")
    if state["status"] != "running":
        raise HTTPException(status_code=400, detail="Pipeline is not running")

    # Find the currently running simulate step
    sim_step = None
    for step in state["steps"]:
        if step["status"] == "running" and step["name"].startswith("simulate"):
            sim_step = step
            break

    if not sim_step:
        raise HTTPException(status_code=400, detail="No simulation step is currently running")

    # Determine the OpenFOAM case directory
    step_name = sim_step["name"]
    last_us = step_name.rfind("_")
    direction = step_name[last_us + 1:]
    case_name = os.path.splitext(os.path.basename(state["config"]["inputPath"]))[0]

    base_dir = WORK_DIR
    cfg = state.get("config", {})
    if cfg.get("outputDir"):
        candidate = os.path.normpath(os.path.join(OUTPUT_ROOT, cfg["outputDir"]))
        if candidate.startswith(os.path.normpath(OUTPUT_ROOT)):
            base_dir = candidate

    case_dir = os.path.join(base_dir, f"{case_name}_{direction}")
    foam_case_dir = os.path.join(case_dir, f"{direction}_dir")
    if not os.path.isdir(os.path.join(foam_case_dir, "system")):
        foam_case_dir = case_dir

    control_dict = os.path.join(foam_case_dir, "system", "controlDict")
    if not os.path.isfile(control_dict):
        raise HTTPException(status_code=404, detail="controlDict not found")

    # Patch stopAt to writeNow
    text = open(control_dict).read()
    text = re.sub(
        r"(stopAt\s+)\w+(\s*;)",
        r"\1writeNow\2",
        text,
    )
    # If no stopAt entry exists, add one inside the top-level block
    if "stopAt" not in text:
        text = text.replace("endTime", "stopAt        writeNow;\n\n    endTime", 1)
    with open(control_dict, "w") as f:
        f.write(text)

    # OpenFOAM only re-reads controlDict at writeInterval boundaries when
    # runTimeModifiable is true.  To make the solver notice immediately, send
    # SIGUSR2 to the actual solver process (child of the bash wrapper).
    # SIGUSR2 tells OpenFOAM to write current results and stop.
    job_id = sim_step.get("jobId")
    if job_id:
        job = job_manager.get_job(job_id)
        proc = job.get("process")
        if proc and proc.returncode is None:
            import signal
            try:
                # The job runs "bash -c 'source ... && simpleFoamMod ...'"
                # so we need to signal the whole process group.
                os.killpg(os.getpgid(proc.pid), signal.SIGUSR2)
            except (ProcessLookupError, PermissionError, OSError):
                # Fallback: just signal the direct process
                try:
                    proc.send_signal(signal.SIGUSR2)
                except (ProcessLookupError, OSError):
                    pass

    sim_step["log"].append("--- stop requested: writing current timestep and exiting ---")

    return {"pipelineId": pipeline_id, "status": "stopping"}


# ---------------------------------------------------------------------------
# Batch endpoints
# ---------------------------------------------------------------------------


@router.post("/batch")
async def run_batch(req: BatchRequest):
    """Launch a batch pipeline across multiple geometry files."""
    # Determine input files
    if req.inputFiles:
        input_files = req.inputFiles
    else:
        if not os.path.isdir(BATCH_DIR):
            raise HTTPException(
                status_code=400,
                detail=f"Batch input directory not found: {BATCH_DIR}",
            )
        input_files = sorted(
            os.path.join(BATCH_DIR, f)
            for f in os.listdir(BATCH_DIR)
            if f.endswith((".dat", ".npy"))
        )
        if not input_files:
            raise HTTPException(
                status_code=400, detail="No geometry files found in batch directory"
            )

    batch_id = str(uuid.uuid4())[:8]
    batch_state = {
        "batchId": batch_id,
        "status": "running",
        "totalFiles": len(input_files),
        "completedFiles": 0,
        "pipelines": [],
        "config": req.model_dump(),
        "inputFiles": input_files,
    }
    _batches[batch_id] = batch_state

    job_store.save_job(batch_id, {
        "id": batch_id,
        "type": "batch",
        "status": "running",
        "totalFiles": len(input_files),
    })

    asyncio.create_task(_execute_batch(batch_id, req, input_files))

    return _to_batch_status(batch_state)


@router.get("/batch/{batch_id}")
async def batch_status(batch_id: str):
    """Return the current batch status with per-geometry pipeline progress."""
    state = _batches.get(batch_id)
    if not state:
        raise HTTPException(status_code=404, detail="Batch not found")
    return _to_batch_status(state)


@router.get("/batch/{batch_id}/results")
async def batch_results(batch_id: str):
    """Return combined results from all completed pipelines in the batch."""
    state = _batches.get(batch_id)
    if not state:
        raise HTTPException(status_code=404, detail="Batch not found")

    all_results = []
    for ps in state["pipelines"]:
        pipeline_state = _pipelines.get(ps["pipelineId"])
        if pipeline_state and pipeline_state.get("results"):
            all_results.append({
                "pipelineId": ps["pipelineId"],
                "inputFile": pipeline_state["config"].get("inputPath", ""),
                "results": pipeline_state["results"],
                "caseDirs": pipeline_state["caseDirs"],
            })

    return {"batchId": batch_id, "results": all_results}


@router.get("/input-files")
async def list_input_files():
    """List .dat/.npy files available for batch processing.

    Returns files from both the batch input directory (BATCH_DIR, mounted
    read-only from the host) and the uploads directory (UPLOAD_DIR, files
    uploaded via the GUI).  Each entry includes the source so the frontend
    can display them grouped.
    """
    results: list[dict] = []

    if os.path.isdir(BATCH_DIR):
        for f in sorted(os.listdir(BATCH_DIR)):
            if f.endswith((".dat", ".npy")):
                results.append({
                    "filename": f,
                    "source": "input",
                    "path": os.path.join(BATCH_DIR, f),
                })

    if os.path.isdir(UPLOAD_DIR):
        for f in sorted(os.listdir(UPLOAD_DIR)):
            if f.endswith((".dat", ".npy")):
                results.append({
                    "filename": f,
                    "source": "uploaded",
                    "path": os.path.join(UPLOAD_DIR, f),
                })

    return {"files": results}


# ---------------------------------------------------------------------------
# Pipeline execution logic
# ---------------------------------------------------------------------------


def _build_step_list(mode: PipelineModeEnum, directions: list[str]) -> list[str]:
    """Return ordered step names based on pipeline mode and directions."""
    steps: list[str] = []
    if mode == PipelineModeEnum.mesh_only:
        for d in directions:
            steps.append(f"mesh_{d}")
    elif mode == PipelineModeEnum.predict_only:
        for d in directions:
            steps.append(f"quick_predict_{d}")
    elif mode == PipelineModeEnum.mesh_predict:
        for d in directions:
            steps.append(f"predict_{d}")
    elif mode == PipelineModeEnum.full:
        for d in directions:
            steps.append(f"predict_{d}")
            steps.append(f"simulate_{d}")
    return steps


async def _execute_pipeline(
    pipeline_id: str,
    req: PipelineRequest,
    directions: list[str],
) -> None:
    """Run all pipeline steps sequentially as a background task."""
    state = _pipelines[pipeline_id]
    case_name = os.path.splitext(os.path.basename(req.inputPath))[0]

    # Determine base output directory
    # outputDir is relative to OUTPUT_ROOT (the browsable host mount)
    base_dir = WORK_DIR
    if req.outputDir:
        candidate = os.path.normpath(os.path.join(OUTPUT_ROOT, req.outputDir))
        # Guard against path traversal outside the browsable root
        if candidate.startswith(os.path.normpath(OUTPUT_ROOT)):
            base_dir = candidate

    try:
        for step_info in state["steps"]:
            if state["status"] != "running":
                break

            step_name = step_info["name"]
            state["currentStep"] = step_name
            step_info["status"] = "running"

            # Parse step name: "mesh_x", "predict_x", "quick_predict_x", "simulate_x"
            # Direction is always the last part after the final underscore
            last_us = step_name.rfind("_")
            step_type = step_name[:last_us]
            direction = step_name[last_us + 1:]

            case_dir = os.path.join(base_dir, f"{case_name}_{direction}")
            # quick_predict runs purely in memory — no case directory needed
            if step_type != "quick_predict" and case_dir not in state["caseDirs"]:
                state["caseDirs"].append(case_dir)

            success = await _run_step(
                step_type, direction, req, case_dir, step_info
            )

            if not success:
                step_info["status"] = "failed"
                state["status"] = "failed"
                state["currentStep"] = None
                _persist_pipeline(state)
                return

            step_info["status"] = "completed"

            # After simulate step completes, parse permeability from CSV
            if step_type == "simulate":
                foam_case_dir = os.path.join(case_dir, f"{direction}_dir")
                if not os.path.isdir(foam_case_dir):
                    foam_case_dir = case_dir
                result = _parse_permeability_csv(foam_case_dir, direction)
                if result:
                    state["results"][direction] = result

            # After quick_predict step, extract stored result
            if step_type == "quick_predict" and "_result" in step_info:
                state["results"][direction] = step_info.pop("_result")

            _persist_pipeline(state)

        # All steps completed
        if state["status"] == "running":
            state["status"] = "completed"
            state["currentStep"] = None
            _persist_pipeline(state)

    except Exception as exc:
        state["status"] = "failed"
        state["currentStep"] = None
        # Record the error on the last running step
        for si in state["steps"]:
            if si["status"] == "running":
                si["status"] = "failed"
                si["log"].append(f"Unexpected error: {exc}")
        _persist_pipeline(state)


async def _run_step(
    step_type: str,
    direction: str,
    req: PipelineRequest,
    case_dir: str,
    step_info: dict,
) -> bool:
    """Execute a single pipeline step.  Returns True on success."""
    if step_type != "quick_predict":
        os.makedirs(case_dir, exist_ok=True)

    if step_type == "mesh":
        return await _run_mesh_step(direction, req, case_dir, step_info)
    elif step_type == "predict":
        return await _run_predict_step(direction, req, case_dir, step_info)
    elif step_type == "quick_predict":
        return await _run_quick_predict_step(direction, req, case_dir, step_info)
    elif step_type == "simulate":
        return await _run_simulate_step(direction, req, case_dir, step_info)
    else:
        step_info["log"].append(f"Unknown step type: {step_type}")
        return False


async def _run_mesh_step(
    direction: str, req: PipelineRequest, case_dir: str, step_info: dict
) -> bool:
    if not MESH_BIN:
        step_info["log"].append("fiberFoamMesh executable not found")
        return False

    config_path = os.path.join(case_dir, "fiberfoam_mesh.yaml")
    write_mesh_config(
        config_path,
        input_path=req.inputPath,
        voxel_size=req.voxelSize,
        voxel_resolution=req.voxelRes,
        flow_direction=direction,
        inlet_buffer=req.inletBuffer,
        outlet_buffer=req.outletBuffer,
        connectivity=req.connectivity,
    )

    cmd = [MESH_BIN, "-config", config_path]
    return await _run_and_wait(cmd, case_dir, step_info, "mesh")


async def _run_predict_step(
    direction: str, req: PipelineRequest, case_dir: str, step_info: dict
) -> bool:
    if not PREDICT_BIN:
        step_info["log"].append("fiberFoamPredict executable not found")
        return False

    config_path = os.path.join(case_dir, "fiberfoam_predict.yaml")
    write_prediction_config(
        config_path,
        input_path=req.inputPath,
        voxel_size=req.voxelSize,
        voxel_resolution=req.voxelRes,
        model_resolution=req.modelRes,
        models_dir=MODELS_DIR,
        flow_directions=[direction],
        inlet_buffer=req.inletBuffer,
        outlet_buffer=req.outletBuffer,
        connectivity=req.connectivity,
    )

    cmd = [PREDICT_BIN, "-config", config_path]
    return await _run_and_wait(cmd, case_dir, step_info, "predict")


async def _run_quick_predict_step(
    direction: str, req: PipelineRequest, case_dir: str, step_info: dict
) -> bool:
    """Run pure-Python ONNX prediction + Darcy permeability for one direction."""
    from services.predictor import predict_permeability

    step_info["log"].append(f"Running quick prediction for direction {direction}...")
    try:
        results = await asyncio.get_event_loop().run_in_executor(
            None,
            lambda: predict_permeability(
                geometry_path=req.inputPath,
                directions=[direction],
                voxel_size=req.voxelSize,
                voxel_res=req.voxelRes,
                model_res=req.modelRes,
                inlet_buffer=req.inletBuffer,
                outlet_buffer=req.outletBuffer,
                nu=req.viscosity,
                density=req.density,
                delta_p=req.deltaP,
            ),
        )
        r = results[0]
        step_info["log"].append(
            f"Permeability ({direction}): {r.permeability:.6e} m^2"
        )
        step_info["log"].append(f"FVC: {r.fiberVolumeContent:.4f}")
        step_info["log"].append(f"Mean velocity: {r.meanVelocity:.6e} m/s")
        # Store result in step_info for later extraction
        step_info["_result"] = {
            "direction": r.direction,
            "permVolAvgMain": r.permeability,
            "fiberVolumeContent": r.fiberVolumeContent,
            "flowLength": r.flowLength,
        }
        return True
    except Exception as e:
        step_info["log"].append(f"Quick prediction failed: {e}")
        return False


async def _run_simulate_step(
    direction: str, req: PipelineRequest, case_dir: str, step_info: dict
) -> bool:
    if not SOLVER_BIN:
        step_info["log"].append("Solver executable not found")
        return False

    # The predict/mesh step writes the OpenFOAM case into a subdirectory
    foam_case_dir = os.path.join(case_dir, f"{direction}_dir")
    if not os.path.isdir(os.path.join(foam_case_dir, "system")):
        foam_case_dir = case_dir  # fallback if no subdirectory

    # Patch controlDict and fvSolution with user settings before running
    _patch_controlDict(foam_case_dir, req.maxIter, req.writeInterval)
    _patch_fvSolution(
        foam_case_dir,
        conv_window=req.convWindow,
        conv_slope=req.convSlope,
        conv_error_bound=req.convErrorBound,
    )

    solver_cmd = (
        f"source {OPENFOAM_BASHRC} && {SOLVER_BIN} -case {foam_case_dir}"
    )
    cmd = ["bash", "-c", solver_cmd]

    config_path = os.path.join(case_dir, "fiberfoam_sim.yaml")
    write_simulation_config(
        config_path,
        case_dir=foam_case_dir,
        solver=req.solver,
        max_iterations=req.maxIter,
        write_interval=req.writeInterval,
    )

    return await _run_and_wait(cmd, foam_case_dir, step_info, "simulate")


def _patch_controlDict(
    foam_case_dir: str, max_iter: int, write_interval: int
) -> None:
    """Patch controlDict with user-specified maxIter and writeInterval.

    The controlDict is generated by the C++ mesh/predict tools with defaults
    (e.g. endTime 1000000, writeInterval 50000).  This overwrites those with
    the values chosen by the user in the pipeline configuration.
    """
    path = os.path.join(foam_case_dir, "system", "controlDict")
    if not os.path.isfile(path):
        return

    text = open(path).read()

    # Replace endTime value
    text = re.sub(
        r"(endTime\s+)\d+(\s*;)",
        rf"\g<1>{max_iter}\2",
        text,
    )
    # Replace writeInterval value
    text = re.sub(
        r"(writeInterval\s+)\d+(\s*;)",
        rf"\g<1>{write_interval}\2",
        text,
    )

    with open(path, "w") as f:
        f.write(text)


def _patch_fvSolution(
    foam_case_dir: str,
    conv_window: int = 10,
    conv_slope: float = 0.01,
    conv_error_bound: float = 0.01,
) -> None:
    """Ensure fvSolution SIMPLE dict has permeabilityControl with user values.

    The permeability-based convergence criteria (custom simpleFoamMod
    feature) should always be used.  If the C++ tool already wrote a
    permeabilityControl block, we update its values.  Otherwise we add one.

    We intentionally do NOT add residualControl — the permeability-based
    criterion is the correct stopping condition for these simulations.
    """
    path = os.path.join(foam_case_dir, "system", "fvSolution")
    if not os.path.isfile(path):
        return

    text = open(path).read()

    perm_block = (
        "permeabilityControl\n"
        "    {{\n"
        "        convPermeability    true;\n"
        "        convSlope           {slope};\n"
        "        convWindow          {window};\n"
        "        errorBound          {error};\n"
        "    }}"
    ).format(slope=conv_slope, window=conv_window, error=conv_error_bound)

    if "permeabilityControl" in text:
        # Replace existing block with updated values
        text = re.sub(
            r"permeabilityControl\s*\{[^}]*\}",
            perm_block,
            text,
            count=1,
        )
    else:
        # Insert after SIMPLE {
        pattern = r"(SIMPLE\s*\{)"
        replacement = r"\1\n    " + perm_block + "\n"
        text = re.sub(pattern, replacement, text, count=1)

    with open(path, "w") as f:
        f.write(text)


async def _run_and_wait(
    cmd: list[str], cwd: str, step_info: dict, job_type: str
) -> bool:
    """Run a command via job_manager and wait for it to complete."""
    job_id = await job_manager.run_command(cmd, cwd=cwd, job_type=job_type)
    step_info["jobId"] = job_id

    # Poll until the job finishes
    while True:
        job = job_manager.get_job(job_id)
        status = job.get("status", "not_found")
        if status in ("completed", "failed", "not_found"):
            full_log = job_manager.get_log(job_id)
            # For simulate jobs, parse residuals from the full log before
            # truncating so the convergence chart survives after completion.
            if job_type == "simulate":
                step_info["_residuals"] = _parse_residuals(full_log)
            step_info["log"] = full_log[-50:]
            return status == "completed"
        await asyncio.sleep(1.0)


# ---------------------------------------------------------------------------
# Batch execution logic
# ---------------------------------------------------------------------------


async def _execute_batch(
    batch_id: str,
    req: BatchRequest,
    input_files: list[str],
) -> None:
    """Run pipelines for each geometry file sequentially."""
    batch = _batches[batch_id]

    for filepath in input_files:
        if batch["status"] != "running":
            break

        # Resolve relative paths against UPLOAD_DIR (same as run_pipeline)
        if not os.path.isabs(filepath):
            filepath = os.path.join(UPLOAD_DIR, filepath)

        # Create a pipeline request for this file
        pipeline_req = PipelineRequest(
            mode=req.mode,
            inputPath=filepath,
            flowDirections=req.flowDirections,
            voxelSize=req.voxelSize,
            voxelRes=req.voxelRes,
            modelRes=req.modelRes,
            inletBuffer=req.inletBuffer,
            outletBuffer=req.outletBuffer,
            connectivity=req.connectivity,
            solver=req.solver,
            maxIter=req.maxIter,
            writeInterval=req.writeInterval,
            convWindow=req.convWindow,
            convSlope=req.convSlope,
            convErrorBound=req.convErrorBound,
            outputDir=req.outputDir,
            viscosity=req.viscosity,
            density=req.density,
            deltaP=req.deltaP,
        )

        # Run pipeline synchronously (within this background task)
        pipeline_id = str(uuid.uuid4())[:8]
        directions = [d.value for d in req.flowDirections]
        steps = _build_step_list(req.mode, directions)

        state = {
            "pipelineId": pipeline_id,
            "mode": req.mode.value,
            "steps": [
                {"name": s, "status": "pending", "jobId": None, "log": []}
                for s in steps
            ],
            "currentStep": None,
            "status": "running",
            "results": {},
            "caseDirs": [],
            "createdAt": datetime.now(timezone.utc).isoformat(),
            "config": pipeline_req.model_dump(),
        }
        _pipelines[pipeline_id] = state
        batch["pipelines"].append({"pipelineId": pipeline_id})

        await _execute_pipeline(pipeline_id, pipeline_req, directions)

        if _pipelines[pipeline_id]["status"] == "completed":
            batch["completedFiles"] += 1

    # Update batch final status
    if batch["status"] == "running":
        if batch["completedFiles"] == batch["totalFiles"]:
            batch["status"] = "completed"
        else:
            batch["status"] = "failed"

    job_store.save_job(batch_id, {
        "id": batch_id,
        "type": "batch",
        "status": batch["status"],
        "totalFiles": batch["totalFiles"],
        "completedFiles": batch["completedFiles"],
    })


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


_RE_TIME = re.compile(r"^Time\s*=\s*(\d+)")
_RE_RESIDUAL = re.compile(
    r"Solving for (\w+),\s*Initial residual\s*=\s*([\d.eE+-]+)"
)
_RE_PERM_MAIN = re.compile(
    r"Main flow direction:\s*([\d.eE+-]+)\s*m"
)
_RE_PERM_FLOW = re.compile(
    r"permeability calculated with the volumetric flow rate.*?[\n\r]*.*?-\s*([\d.eE+-]+)\s*m",
    re.IGNORECASE,
)
_RE_SLOPE = re.compile(
    r"Normalized slope of linear regression:\s*([\d.eE+-]+)"
)
_RE_PRED_PERM = re.compile(
    r"Predicted permeability:\s*([\d.eE+-]+)\s*m"
)
_RE_PERM_ERROR = re.compile(
    r"Error predicted vs\. calculated permeability:\s*([\d.eE+-]+)"
)


def _parse_residuals(log_lines: list[str]) -> list[dict]:
    """Extract per-iteration residuals, permeability and convergence info from solver log."""
    residuals: list[dict] = []
    current_iter: int | None = None
    current: dict = {}

    for line in log_lines:
        m = _RE_TIME.search(line)
        if m:
            if current_iter is not None and current:
                residuals.append({"iteration": current_iter, **current})
            current_iter = int(m.group(1))
            current = {}
            continue

        m = _RE_RESIDUAL.search(line)
        if m:
            current[m.group(1)] = float(m.group(2))

        m = _RE_PERM_MAIN.search(line)
        if m:
            current["permVolAvg"] = float(m.group(1))

        m = _RE_SLOPE.search(line)
        if m:
            current["slope"] = float(m.group(1))

        m = _RE_PRED_PERM.search(line)
        if m:
            current["predPerm"] = float(m.group(1))

        m = _RE_PERM_ERROR.search(line)
        if m:
            current["permError"] = float(m.group(1))

    if current_iter is not None and current:
        residuals.append({"iteration": current_iter, **current})

    return residuals


def _parse_permeability_csv(case_dir: str, direction: str) -> dict | None:
    """Parse the [x|y|z]PermeabilityInfo.csv written by simpleFoamMod."""
    csv_name = f"{direction}PermeabilityInfo.csv"
    csv_path = os.path.join(case_dir, csv_name)
    if not os.path.isfile(csv_path):
        return None

    data: dict = {"direction": direction}
    key_map = {
        "flowLength": "flowLength",
        "flowCrossArea": "crossSectionArea",
        "permVolAvgUmain": "permVolAvgMain",
        "permVolAvgUsec": "permVolAvgSecondary",
        "permVolAvgUtert": "permVolAvgTertiary",
        "permFlowRate": "permFlowRate",
        "fvc": "fiberVolumeContent",
    }
    try:
        with open(csv_path) as f:
            for line in f:
                parts = line.strip().split(";")
                if len(parts) >= 2 and parts[0] in key_map:
                    data[key_map[parts[0]]] = float(parts[1])
    except Exception:
        return None

    return data if len(data) > 1 else None


def _to_pipeline_status(state: dict) -> dict:
    """Convert internal state dict to PipelineStatus response.

    For the currently running step, pull live log lines and progress
    from the job manager so the frontend gets real-time feedback.
    """
    enriched_steps = []
    for s in state["steps"]:
        step = dict(s)
        # Enrich running steps with live data from the job manager
        if step["status"] == "running" and step.get("jobId"):
            job = job_manager.get_job(step["jobId"])
            live_log = job_manager.get_log(step["jobId"])
            step["log"] = live_log[-100:]  # last 100 lines
            # Estimate progress from log line count (rough heuristic)
            if job.get("status") == "completed":
                step["progress"] = 100.0
            # Parse residuals for simulate steps
            if step.get("name", "").startswith("simulate"):
                step["residuals"] = _parse_residuals(live_log)
        elif step.get("name", "").startswith("simulate"):
            # Use cached residuals (parsed from full log before truncation)
            # so the convergence chart persists after the solver finishes.
            if step.get("_residuals"):
                step["residuals"] = step["_residuals"]
            elif step.get("log"):
                step["residuals"] = _parse_residuals(step["log"])
        enriched_steps.append(step)

    return PipelineStatus(
        pipelineId=state["pipelineId"],
        mode=PipelineModeEnum(state["mode"]),
        steps=[PipelineStepStatus(**s) for s in enriched_steps],
        currentStep=state.get("currentStep"),
        status=JobStatusEnum(state["status"]),
        results=state.get("results", {}),
        caseDirs=state.get("caseDirs", []),
        createdAt=state.get("createdAt", ""),
    ).model_dump()


def _to_batch_status(state: dict) -> dict:
    """Convert internal batch state dict to BatchStatus response."""
    pipeline_statuses = []
    for ps in state.get("pipelines", []):
        pid = ps["pipelineId"]
        pstate = _pipelines.get(pid)
        if pstate:
            pipeline_statuses.append(_to_pipeline_status(pstate))

    return BatchStatus(
        batchId=state["batchId"],
        status=JobStatusEnum(state["status"]),
        totalFiles=state.get("totalFiles", 0),
        completedFiles=state.get("completedFiles", 0),
        pipelines=[PipelineStatus(**p) for p in pipeline_statuses],
    ).model_dump()


def _persist_pipeline(state: dict) -> None:
    """Persist pipeline state to job store."""
    job_store.save_job(state["pipelineId"], {
        "id": state["pipelineId"],
        "type": "pipeline",
        "status": state["status"],
        "mode": state.get("mode"),
        "steps": [
            {"name": s["name"], "status": s["status"], "jobId": s.get("jobId")}
            for s in state["steps"]
        ],
        "caseDirs": state.get("caseDirs", []),
        "results": state.get("results", {}),
    })
