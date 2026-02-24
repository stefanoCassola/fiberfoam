from fastapi import APIRouter, HTTPException, Query
from schemas import (
    PredictionRequest,
    PredictionResponse,
    DirectionPrediction,
    JobStatus,
    JobStatusEnum,
)
from services.executor import job_manager
from services.config_writer import write_prediction_config
import os

router = APIRouter()

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
FIBERFOAM_PREDICT = os.environ.get(
    "FIBERFOAM_PREDICT_BIN",
    os.path.join(_PROJECT_ROOT, "build", "bin", "fiberFoamPredict"),
)

WORK_DIR = os.environ.get("FIBERFOAM_WORK_DIR", "/tmp/fiberfoam/cases")


@router.post("/run", response_model=PredictionResponse)
async def run_prediction(req: PredictionRequest):
    """Run the ML velocity/pressure prediction on a geometry.

    Writes a YAML config and invokes ``fiberFoamPredict``.
    Returns immediately with a *jobId* for progress tracking.
    """
    if not os.path.isfile(req.inputPath):
        raise HTTPException(
            status_code=400, detail=f"Input geometry not found: {req.inputPath}"
        )

    case_name = os.path.splitext(os.path.basename(req.inputPath))[0]
    output_dir = os.path.join(WORK_DIR, f"{case_name}_predict")
    os.makedirs(output_dir, exist_ok=True)

    directions = [d.value for d in req.flowDirections]

    config_path = os.path.join(output_dir, "fiberfoam_predict.yaml")
    write_prediction_config(
        config_path,
        input_path=req.inputPath,
        voxel_resolution=req.voxelRes,
        model_resolution=req.modelRes,
        models_dir=req.modelsDir,
        flow_directions=directions,
    )

    if not os.path.isfile(FIBERFOAM_PREDICT):
        raise HTTPException(
            status_code=500,
            detail=f"fiberFoamPredict executable not found at {FIBERFOAM_PREDICT}. "
            "Set FIBERFOAM_PREDICT_BIN environment variable.",
        )

    cmd = [FIBERFOAM_PREDICT, config_path]
    job_id = await job_manager.run_command(cmd, cwd=output_dir)

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
