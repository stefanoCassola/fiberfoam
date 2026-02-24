from fastapi import APIRouter, HTTPException, Query
from schemas import MeshRequest, MeshResponse, JobStatus, JobStatusEnum
from services.executor import job_manager
from services.config_writer import write_mesh_config
import os
import re

router = APIRouter()

# Resolve path to the fiberFoamMesh executable.  The user can override via
# environment variable; otherwise we look relative to the project root.
_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
FIBERFOAM_MESH = os.environ.get(
    "FIBERFOAM_MESH_BIN",
    os.path.join(_PROJECT_ROOT, "build", "bin", "fiberFoamMesh"),
)

WORK_DIR = os.environ.get("FIBERFOAM_WORK_DIR", "/tmp/fiberfoam/cases")


@router.post("/generate", response_model=MeshResponse)
async def generate_mesh(req: MeshRequest):
    """Generate an OpenFOAM hex-mesh from a voxelised geometry file.

    Writes a temporary YAML config and invokes ``fiberFoamMesh``.
    Returns immediately with a *jobId* for progress tracking.
    """
    if not os.path.isfile(req.inputPath):
        raise HTTPException(
            status_code=400, detail=f"Input geometry not found: {req.inputPath}"
        )

    # Create a per-run case directory
    case_name = os.path.splitext(os.path.basename(req.inputPath))[0]
    case_dir = os.path.join(
        WORK_DIR,
        f"{case_name}_{req.flowDirection.value}",
    )
    os.makedirs(case_dir, exist_ok=True)

    config_path = os.path.join(case_dir, "fiberfoam_mesh.yaml")
    write_mesh_config(
        config_path,
        input_path=req.inputPath,
        voxel_size=req.voxelSize,
        voxel_resolution=req.voxelRes,
        flow_direction=req.flowDirection.value,
        inlet_buffer=req.inletBuffer,
        outlet_buffer=req.outletBuffer,
        connectivity=req.connectivity,
    )

    if not os.path.isfile(FIBERFOAM_MESH):
        raise HTTPException(
            status_code=500,
            detail=f"fiberFoamMesh executable not found at {FIBERFOAM_MESH}. "
            "Set FIBERFOAM_MESH_BIN environment variable.",
        )

    cmd = [FIBERFOAM_MESH, config_path]
    job_id = await job_manager.run_command(cmd, cwd=case_dir)

    return MeshResponse(jobId=job_id, caseDir=case_dir)


@router.get("/status/{job_id}", response_model=JobStatus)
async def mesh_status(job_id: str, since: int = Query(0, ge=0)):
    """Poll the status and recent log output of a mesh-generation job."""
    job = job_manager.get_job(job_id)
    log_lines = job_manager.get_log(job_id, since=since)

    # Try to extract mesh statistics from log output
    progress = None
    if job.get("status") == "completed":
        progress = 1.0

    return JobStatus(
        jobId=job_id,
        status=JobStatusEnum(job["status"]),
        progress=progress,
        returncode=job.get("returncode"),
        log=log_lines,
    )


@router.post("/cancel/{job_id}")
async def cancel_mesh(job_id: str):
    """Cancel a running mesh-generation job."""
    cancelled = await job_manager.cancel_job(job_id)
    if not cancelled:
        raise HTTPException(
            status_code=400, detail="Job not found or not running"
        )
    return {"jobId": job_id, "status": "cancelled"}


def _parse_mesh_stats(log_lines: list[str]) -> dict:
    """Extract nCells / nPoints / nFaces / nInternalFaces from log output.

    The C++ code prints lines like:
        nCells: 123456
        nPoints: 234567
    """
    stats: dict[str, int] = {}
    patterns = {
        "nCells": re.compile(r"nCells\s*[:=]\s*(\d+)"),
        "nPoints": re.compile(r"nPoints\s*[:=]\s*(\d+)"),
        "nFaces": re.compile(r"nFaces\s*[:=]\s*(\d+)"),
        "nInternalFaces": re.compile(r"nInternalFaces\s*[:=]\s*(\d+)"),
    }
    for line in log_lines:
        for key, pat in patterns.items():
            m = pat.search(line)
            if m:
                stats[key] = int(m.group(1))
    return stats
