"""ParaView integration endpoints.

Provides VTK export and ParaView launch capabilities for visualizing
meshes, predictions, and simulation results.
"""
import asyncio
import logging
import os
import subprocess

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from services.paths import WORK_DIR, OPENFOAM_BASHRC

logger = logging.getLogger(__name__)
router = APIRouter()


class ParaViewRequest(BaseModel):
    caseDir: str
    mode: str = "case"  # "case", "mesh", "prediction", "vtk_export"


class VtkExportRequest(BaseModel):
    caseDir: str
    latestTime: bool = True


def _resolve_case_dir(case_dir: str) -> str:
    """Resolve and validate case directory path."""
    # Allow absolute paths or relative to WORK_DIR
    if os.path.isabs(case_dir):
        resolved = case_dir
    else:
        resolved = os.path.join(WORK_DIR, case_dir)

    if not os.path.isdir(resolved):
        raise HTTPException(status_code=404, detail=f"Case directory not found: {resolved}")

    return resolved


@router.post("/export-vtk")
async def export_vtk(req: VtkExportRequest):
    """Convert OpenFOAM case to VTK format using foamToVTK."""
    case_dir = _resolve_case_dir(req.caseDir)

    # Check if it's an OpenFOAM case (has system/controlDict)
    if not os.path.isfile(os.path.join(case_dir, "system", "controlDict")):
        raise HTTPException(status_code=400, detail="Not a valid OpenFOAM case directory")

    cmd = f"source {OPENFOAM_BASHRC} && foamToVTK -case {case_dir}"
    if req.latestTime:
        cmd += " -latestTime"

    try:
        proc = await asyncio.create_subprocess_shell(
            f"bash -c '{cmd}'",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=120)

        vtk_dir = os.path.join(case_dir, "VTK")
        vtk_files = []
        if os.path.isdir(vtk_dir):
            vtk_files = [f for f in os.listdir(vtk_dir) if f.endswith((".vtk", ".vtu", ".vtp"))]

        return {
            "status": "ok" if proc.returncode == 0 else "error",
            "returncode": proc.returncode,
            "vtkDir": vtk_dir,
            "vtkFiles": vtk_files,
            "log": (stdout.decode() + stderr.decode()).strip()[-2000:],
        }
    except asyncio.TimeoutError:
        raise HTTPException(status_code=504, detail="VTK export timed out")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/open")
async def open_paraview(req: ParaViewRequest):
    """Launch ParaView for the given case.

    In Docker, this creates a .foam file and returns the path so the user
    can open it from their host. On a local install, it attempts to launch
    paraFoam/paraview directly.
    """
    case_dir = _resolve_case_dir(req.caseDir)

    # Create a .foam touch file (ParaView uses this to detect OF cases)
    foam_file = os.path.join(case_dir, "case.foam")
    if not os.path.exists(foam_file):
        open(foam_file, "w").close()

    # Check if we're in Docker
    in_docker = os.path.exists("/.dockerenv") or os.path.isfile("/proc/1/cgroup")

    if in_docker:
        # In Docker: can't launch GUI. Return the path for the user.
        # Map container path to host path if possible
        host_path = case_dir
        # /data/cases/... maps to the host output dir
        return {
            "status": "path_only",
            "message": "ParaView cannot run inside Docker. Open the case on your host machine.",
            "caseDir": case_dir,
            "foamFile": foam_file,
            "hint": f"Run: paraview {foam_file}",
        }

    # Local install: try to launch paraFoam or paraview
    try:
        if req.mode == "vtk_export":
            # Export to VTK first, then open
            await export_vtk(VtkExportRequest(caseDir=req.caseDir))
            vtk_dir = os.path.join(case_dir, "VTK")
            subprocess.Popen(["paraview", "--data", vtk_dir], start_new_session=True)
        else:
            # Open the .foam file directly
            subprocess.Popen(["paraview", foam_file], start_new_session=True)

        return {"status": "launched", "caseDir": case_dir, "foamFile": foam_file}
    except FileNotFoundError:
        return {
            "status": "not_found",
            "message": "ParaView not found on PATH. Install ParaView or open the case manually.",
            "foamFile": foam_file,
            "hint": f"Run: paraview {foam_file}",
        }


@router.get("/available")
async def check_paraview():
    """Check if ParaView/foamToVTK are available."""
    in_docker = os.path.exists("/.dockerenv")

    # Check foamToVTK
    foam_to_vtk = False
    try:
        proc = await asyncio.create_subprocess_shell(
            f"bash -c 'source {OPENFOAM_BASHRC} && which foamToVTK'",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, _ = await proc.communicate()
        foam_to_vtk = proc.returncode == 0
    except Exception:
        pass

    # Check paraview
    paraview_available = False
    if not in_docker:
        try:
            proc = subprocess.run(["which", "paraview"], capture_output=True)
            paraview_available = proc.returncode == 0
        except Exception:
            pass

    return {
        "inDocker": in_docker,
        "foamToVTK": foam_to_vtk,
        "paraview": paraview_available,
    }


@router.get("/case-dirs/{pipeline_id}")
async def list_case_dirs(pipeline_id: str):
    """List available case directories for a pipeline (e.g. x_dir, y_dir, z_dir)."""
    from services.job_store import get_pipeline

    pipeline = get_pipeline(pipeline_id)
    if not pipeline:
        raise HTTPException(status_code=404, detail="Pipeline not found")

    output_dir = pipeline.get("outputDir", "")
    if not output_dir or not os.path.isdir(output_dir):
        # Try WORK_DIR
        output_dir = os.path.join(WORK_DIR, pipeline_id)

    if not os.path.isdir(output_dir):
        raise HTTPException(status_code=404, detail="Output directory not found")

    # Find case directories (those with system/controlDict)
    cases = []
    for entry in sorted(os.listdir(output_dir)):
        full = os.path.join(output_dir, entry)
        if os.path.isdir(full) and os.path.isfile(os.path.join(full, "system", "controlDict")):
            # Check what time steps exist
            time_dirs = [
                d for d in os.listdir(full)
                if os.path.isdir(os.path.join(full, d)) and d.replace(".", "").isdigit()
            ]
            has_mesh = os.path.isdir(os.path.join(full, "constant", "polyMesh"))
            has_results = any(float(t) > 0 for t in time_dirs if t != "0")

            cases.append({
                "name": entry,
                "path": full,
                "hasMesh": has_mesh,
                "hasResults": has_results,
                "timeSteps": sorted(time_dirs, key=lambda x: float(x)),
            })

    return {"pipelineId": pipeline_id, "outputDir": output_dir, "cases": cases}
