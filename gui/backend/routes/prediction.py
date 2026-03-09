from fastapi import APIRouter, HTTPException, Query
from schemas import (
    PredictionRequest,
    PredictionResponse,
    DirectionPrediction,
    QuickPredictionRequest,
    QuickPredictionResponse,
    QuickPredictionData,
    JobStatus,
    JobStatusEnum,
)
from services.executor import job_manager
from services.config_writer import write_prediction_config
from services.paths import PREDICT_BIN, WORK_DIR, MODELS_DIR, UPLOAD_DIR
import os

router = APIRouter()


@router.post("/run", response_model=PredictionResponse)
async def run_prediction(req: PredictionRequest):
    """Run the ML velocity/pressure prediction on a geometry.

    Writes a YAML config and invokes ``fiberFoamPredict``.
    Returns immediately with a *jobId* for progress tracking.
    """
    if not os.path.isabs(req.inputPath):
        req.inputPath = os.path.join(UPLOAD_DIR, req.inputPath)

    if not os.path.isfile(req.inputPath):
        raise HTTPException(
            status_code=400, detail=f"Input geometry not found: {req.inputPath}"
        )

    case_name = os.path.splitext(os.path.basename(req.inputPath))[0]
    output_dir = os.path.join(WORK_DIR, f"{case_name}_predict")
    os.makedirs(output_dir, exist_ok=True)

    directions = [d.value for d in req.flowDirections]

    # Use centralized MODELS_DIR if the request doesn't specify one
    models_dir = req.modelsDir if req.modelsDir else MODELS_DIR

    config_path = os.path.join(output_dir, "fiberfoam_predict.yaml")
    write_prediction_config(
        config_path,
        input_path=req.inputPath,
        voxel_resolution=req.voxelRes,
        model_resolution=req.modelRes,
        models_dir=models_dir,
        flow_directions=directions,
    )

    if not PREDICT_BIN:
        raise HTTPException(
            status_code=500,
            detail="fiberFoamPredict executable not found. "
            "Set FIBERFOAM_PREDICT_BIN environment variable.",
        )

    cmd = [PREDICT_BIN, "-config", config_path]
    job_id = await job_manager.run_command(cmd, cwd=output_dir, job_type="predict")

    # Build expected output files (one per direction)
    dir_predictions = [
        DirectionPrediction(
            direction=d,
            outputFile=os.path.join(output_dir, f"predicted_{d}.dat"),
        )
        for d in directions
    ]

    return PredictionResponse(
        jobId=job_id,
        outputDir=output_dir,
        directions=dir_predictions,
    )


@router.get("/status/{job_id}", response_model=JobStatus)
async def prediction_status(job_id: str, since: int = Query(0, ge=0)):
    """Poll the status and recent log output of a prediction job."""
    job = job_manager.get_job(job_id)
    log_lines = job_manager.get_log(job_id, since=since)

    progress = None
    if job.get("status") == "completed":
        progress = 1.0
    elif job.get("status") == "running":
        # Attempt to estimate progress from log lines
        for line in reversed(log_lines):
            if "%" in line:
                try:
                    pct = float(line.split("%")[0].split()[-1])
                    progress = pct / 100.0
                except (ValueError, IndexError):
                    pass
                break

    return JobStatus(
        jobId=job_id,
        status=JobStatusEnum(job["status"]),
        progress=progress,
        returncode=job.get("returncode"),
        log=log_lines,
    )


@router.post("/cancel/{job_id}")
async def cancel_prediction(job_id: str):
    """Cancel a running prediction job."""
    cancelled = await job_manager.cancel_job(job_id)
    if not cancelled:
        raise HTTPException(
            status_code=400, detail="Job not found or not running"
        )
    return {"jobId": job_id, "status": "cancelled"}


@router.post("/quick", response_model=QuickPredictionResponse)
async def quick_prediction(req: QuickPredictionRequest):
    """Run pure-Python ONNX prediction + Darcy permeability (no C++ needed).

    Returns permeability results directly — fast enough for synchronous use.
    """
    from services.predictor import predict_permeability

    if not os.path.isabs(req.inputPath):
        req.inputPath = os.path.join(UPLOAD_DIR, req.inputPath)

    if not os.path.isfile(req.inputPath):
        raise HTTPException(
            status_code=400, detail=f"Input geometry not found: {req.inputPath}"
        )

    directions = [d.value for d in req.flowDirections]

    try:
        results = predict_permeability(
            geometry_path=req.inputPath,
            directions=directions,
            voxel_size=req.voxelSize,
            voxel_res=req.voxelRes,
            model_res=req.modelRes,
            inlet_buffer=req.inletBuffer,
            outlet_buffer=req.outletBuffer,
            nu=req.viscosity,
            density=req.density,
            delta_p=req.deltaP,
        )
    except FileNotFoundError as e:
        raise HTTPException(status_code=404, detail=str(e))
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Prediction failed: {e}")

    return QuickPredictionResponse(
        results=[
            QuickPredictionData(
                direction=r.direction,
                permeability=r.permeability,
                fiberVolumeContent=r.fiberVolumeContent,
                meanVelocity=r.meanVelocity,
                flowLength=r.flowLength,
            )
            for r in results
        ]
    )


@router.get("/models")
async def list_models():
    """Scan MODELS_DIR for available model sets, grouped by resolution.

    Returns a list of model sets, each with resolution, folder name,
    and available directions.  The frontend uses this to populate the
    model selector dropdown.
    """
    import glob as _glob
    import re as _re
    from collections import defaultdict

    if not os.path.isdir(MODELS_DIR):
        return {"models": [], "modelSets": [], "modelsDir": MODELS_DIR}

    # Find ONNX files
    all_files = sorted(
        os.path.relpath(p, MODELS_DIR)
        for p in _glob.glob(os.path.join(MODELS_DIR, "**/*.onnx"), recursive=True)
    )

    # Group by parent folder (e.g. "res80")
    sets: dict[str, list[str]] = defaultdict(list)
    for f in all_files:
        parts = f.split("/")
        folder = parts[0] if len(parts) > 1 else ""
        basename = parts[-1]
        # Extract direction from filename like "x_80.onnx"
        m = _re.match(r"([xyz])_\d+\.onnx$", basename)
        if m:
            sets[folder].append(m.group(1))

    # Also find TF SavedModel directories (e.g. "x_80_tf/saved_model.pb")
    for tf_pb in _glob.glob(os.path.join(MODELS_DIR, "**/*_tf/saved_model.pb"), recursive=True):
        tf_dir = os.path.dirname(tf_pb)
        rel = os.path.relpath(tf_dir, MODELS_DIR)
        parts = rel.split("/")
        folder = parts[0] if len(parts) > 1 else ""
        basename = parts[-1]  # e.g. "x_80_tf"
        m = _re.match(r"([xyz])_\d+_tf$", basename)
        if m:
            direction = m.group(1)
            if direction not in sets.get(folder, []):
                sets[folder].append(direction)
                all_files.append(os.path.relpath(tf_dir, MODELS_DIR))

    model_sets = []
    for folder, directions in sorted(sets.items()):
        # Extract resolution from folder name like "res80" or "res80_fno"
        m = _re.match(r"res(\d+)", folder)
        resolution = int(m.group(1)) if m else 0
        model_sets.append({
            "folder": folder,
            "resolution": resolution,
            "directions": sorted(set(directions)),
        })

    return {"models": all_files, "modelSets": model_sets, "modelsDir": MODELS_DIR}
