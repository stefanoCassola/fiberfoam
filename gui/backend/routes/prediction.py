from fastapi import APIRouter, HTTPException, Query
from pydantic import BaseModel
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


class VtkExportBody(BaseModel):
    destPath: str = ""


@router.post("/export-vtk/{pipeline_id}")
async def export_prediction_vtk(pipeline_id: str, body: VtkExportBody = VtkExportBody()):
    """Export prediction velocity fields as VTK structured grids.

    Writes VTK files to the user-chosen destination folder (on the host
    filesystem via /host mount) or falls back to a VTK/ subdirectory
    inside the prediction output.
    """
    from services import job_store
    from services.paths import OUTPUT_ROOT

    # Try in-memory pipeline state first (from pipeline router)
    predict_dir = ""
    try:
        from routes.pipeline import _pipelines
        state = _pipelines.get(pipeline_id)
        if state:
            predict_dir = state.get("predictDir", "")
    except Exception:
        pass

    # Fall back to persisted job data
    if not predict_dir:
        job = job_store.load_job(pipeline_id)
        if job:
            predict_dir = job.get("predictDir", "")

    if not predict_dir or not os.path.isdir(predict_dir):
        raise HTTPException(status_code=404, detail="No prediction output directory found. Run a prediction first.")

    import numpy as _np
    import glob as _glob

    # Find predicted .npy files in the predict dir
    npy_files = sorted(_glob.glob(os.path.join(predict_dir, "predicted_*.npy")))
    if not npy_files:
        npy_files = sorted(_glob.glob(os.path.join(predict_dir, "predicted_*.dat")))

    if not npy_files:
        raise HTTPException(status_code=404, detail="No prediction output files found")

    # Determine output directory
    if body.destPath:
        vtk_dir = os.path.normpath(os.path.join(OUTPUT_ROOT, body.destPath))
        if not vtk_dir.startswith(os.path.normpath(OUTPUT_ROOT)):
            raise HTTPException(status_code=400, detail="Invalid destination path")
    else:
        vtk_dir = os.path.join(predict_dir, "VTK")
    try:
        os.makedirs(vtk_dir, exist_ok=True)
    except PermissionError:
        raise HTTPException(
            status_code=403,
            detail=f"Permission denied creating directory. Choose a folder you have write access to.",
        )

    exported = []
    for fpath in npy_files:
        fname = os.path.basename(fpath)
        name = os.path.splitext(fname)[0]

        if fpath.endswith(".npy"):
            data = _np.load(fpath).astype(_np.float32)
        else:
            raw = _np.loadtxt(fpath, dtype=_np.float32)
            n = round(raw.size ** (1.0 / 3.0))
            data = raw.reshape(n, n, n) if n * n * n == raw.size else raw

        if data.ndim != 3:
            continue

        nz, ny, nx = data.shape
        vtk_path = os.path.join(vtk_dir, f"{name}.vtk")

        try:
            f_handle = open(vtk_path, "wb")
        except PermissionError:
            raise HTTPException(
                status_code=403,
                detail=f"Permission denied writing to {vtk_dir}. Choose a folder you have write access to.",
            )
        with f_handle as vf:
            header = (
                f"# vtk DataFile Version 3.0\n"
                f"{name}\n"
                f"BINARY\n"
                f"DATASET STRUCTURED_POINTS\n"
                f"DIMENSIONS {nx} {ny} {nz}\n"
                f"ORIGIN 0 0 0\n"
                f"SPACING 1 1 1\n"
                f"POINT_DATA {nx * ny * nz}\n"
                f"SCALARS velocity float 1\n"
                f"LOOKUP_TABLE default\n"
            )
            vf.write(header.encode("ascii"))
            vf.write(data.astype(">f4").tobytes())

        exported.append(f"{name}.vtk")

    return {"status": "ok", "outputDir": vtk_dir, "files": exported}


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

    # Also find FNO numpy weights (e.g. "x_80_fno.npz")
    for npz in _glob.glob(os.path.join(MODELS_DIR, "**/*_fno.npz"), recursive=True):
        rel = os.path.relpath(npz, MODELS_DIR)
        parts = rel.split("/")
        folder = parts[0] if len(parts) > 1 else ""
        basename = parts[-1]  # e.g. "x_80_fno.npz"
        m = _re.match(r"([xyz])_\d+_fno\.npz$", basename)
        if m:
            direction = m.group(1)
            if direction not in sets.get(folder, []):
                sets[folder].append(direction)
                all_files.append(rel)

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
