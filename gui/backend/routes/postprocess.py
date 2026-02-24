import json
import os
import re

from fastapi import APIRouter, HTTPException, Query
from schemas import (
    PostProcessRequest,
    PostProcessResponse,
    PermeabilityData,
    JobStatus,
    JobStatusEnum,
)
from services.executor import job_manager
from services.config_writer import write_postprocess_config

router = APIRouter()

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
FIBERFOAM_POSTPROCESS = os.environ.get(
    "FIBERFOAM_POSTPROCESS_BIN",
    os.path.join(_PROJECT_ROOT, "build", "bin", "fiberFoamPostProcess"),
)


@router.post("/run", response_model=PostProcessResponse)
async def run_postprocess(req: PostProcessRequest):
    """Run permeability post-processing on a completed simulation case.

    Invokes ``fiberFoamPostProcess`` which reads the latest time directory
    in the OpenFOAM case and computes permeability via volume-averaged
    and/or flow-rate methods.
    """
    if not os.path.isdir(req.caseDir):
        raise HTTPException(
            status_code=400, detail=f"Case directory not found: {req.caseDir}"
        )

    config_path = os.path.join(req.caseDir, "fiberfoam_post.yaml")
    write_postprocess_config(
        config_path,
        case_dir=req.caseDir,
        method=req.method.value,
        fibrous_region_only=req.fibrousRegionOnly,
    )

    if not os.path.isfile(FIBERFOAM_POSTPROCESS):
        raise HTTPException(
            status_code=500,
            detail=f"fiberFoamPostProcess executable not found at "
            f"{FIBERFOAM_POSTPROCESS}. "
            "Set FIBERFOAM_POSTPROCESS_BIN environment variable.",
        )

    cmd = [FIBERFOAM_POSTPROCESS, config_path]
    job_id = await job_manager.run_command(cmd, cwd=req.caseDir)

    return PostProcessResponse(jobId=job_id, caseDir=req.caseDir)


@router.get("/status/{job_id}", response_model=JobStatus)
async def postprocess_status(job_id: str, since: int = Query(0, ge=0)):
    """Poll the status of a post-processing job."""
    job = job_manager.get_job(job_id)
    log_lines = job_manager.get_log(job_id, since=since)

    progress = 1.0 if job.get("status") == "completed" else None

    return JobStatus(
        jobId=job_id,
        status=JobStatusEnum(job["status"]),
        progress=progress,
        returncode=job.get("returncode"),
        log=log_lines,
    )


@router.get("/results/{job_id}", response_model=PostProcessResponse)
async def postprocess_results(job_id: str):
    """Retrieve parsed permeability results once the job has completed.

    Results are extracted from the log output of ``fiberFoamPostProcess``.
    If the executable also writes a JSON results file we read that instead.
    """
    job = job_manager.get_job(job_id)
    if job.get("status") == "not_found":
        raise HTTPException(status_code=404, detail="Job not found")
    if job.get("status") == "running":
        raise HTTPException(
            status_code=409, detail="Job is still running; results not yet available"
        )
    if job.get("status") == "failed":
        raise HTTPException(
            status_code=500,
            detail="Post-processing failed. Check /status endpoint for logs.",
        )

    log_lines = job.get("log", [])

    # First try: look for a JSON results file written by the executable
    # (convention: <caseDir>/permeability_results.json)
    case_dir = _extract_case_dir(log_lines)
    results = _try_load_json_results(case_dir)
    if results is not None:
        return PostProcessResponse(
            jobId=job_id, caseDir=case_dir or "", results=results
        )

    # Fallback: parse permeability values from log output
    results = _parse_results_from_log(log_lines)

    return PostProcessResponse(
        jobId=job_id, caseDir=case_dir or "", results=results
    )


@router.post("/cancel/{job_id}")
async def cancel_postprocess(job_id: str):
    """Cancel a running post-processing job."""
    cancelled = await job_manager.cancel_job(job_id)
    if not cancelled:
        raise HTTPException(
            status_code=400, detail="Job not found or not running"
        )
    return {"jobId": job_id, "status": "cancelled"}


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

# Regex patterns matching typical C++ log output from PermeabilityCalculator.
_RE_DIRECTION = re.compile(r"Direction\s*[:=]\s*(\w)")
_RE_PERM_VOL = re.compile(
    r"[Pp]ermeability\s*\(vol(?:ume)?[\s-]*avg\w*\)\s*[:=]\s*([\d.eE+-]+)"
)
_RE_PERM_FLOW = re.compile(
    r"[Pp]ermeability\s*\(flow[\s-]*rate\)\s*[:=]\s*([\d.eE+-]+)"
)
_RE_FVC = re.compile(r"[Ff]iber\s*[Vv]olume\s*[Cc]ontent\s*[:=]\s*([\d.eE+-]+)")
_RE_FLOW_LEN = re.compile(r"[Ff]low\s*[Ll]ength\s*[:=]\s*([\d.eE+-]+)")
_RE_CROSS_AREA = re.compile(r"[Cc]ross[\s-]*[Ss]ection\s*[Aa]rea\s*[:=]\s*([\d.eE+-]+)")


def _parse_results_from_log(log_lines: list[str]) -> list[PermeabilityData]:
    """Best-effort extraction of permeability data from textual log output."""
    results: list[PermeabilityData] = []
    current: dict = {}

    def _flush():
        if current.get("direction"):
            results.append(PermeabilityData(**current))

    for line in log_lines:
        m = _RE_DIRECTION.search(line)
        if m:
            _flush()
            current = {"direction": m.group(1).lower()}
            continue

        m = _RE_PERM_VOL.search(line)
        if m:
            current["permVolAvgMain"] = float(m.group(1))

        m = _RE_PERM_FLOW.search(line)
        if m:
            current["permFlowRate"] = float(m.group(1))

        m = _RE_FVC.search(line)
        if m:
            current["fiberVolumeContent"] = float(m.group(1))

        m = _RE_FLOW_LEN.search(line)
        if m:
            current["flowLength"] = float(m.group(1))

        m = _RE_CROSS_AREA.search(line)
        if m:
            current["crossSectionArea"] = float(m.group(1))

    _flush()
    return results


def _extract_case_dir(log_lines: list[str]) -> str:
    """Try to find the case directory from log lines (e.g. 'Case: /path/to/case')."""
    for line in log_lines:
        if line.strip().startswith("Case:") or line.strip().startswith("caseDir:"):
            parts = line.split(":", 1)
            if len(parts) == 2:
                return parts[1].strip()
    return ""


def _try_load_json_results(case_dir: str | None) -> list[PermeabilityData] | None:
    """Load structured results from JSON if the executable wrote one."""
    if not case_dir:
        return None
    json_path = os.path.join(case_dir, "permeability_results.json")
    if not os.path.isfile(json_path):
        return None
    try:
        with open(json_path) as f:
            data = json.load(f)
        if isinstance(data, list):
            return [PermeabilityData(**entry) for entry in data]
        if isinstance(data, dict) and "results" in data:
            return [PermeabilityData(**entry) for entry in data["results"]]
    except Exception:
        return None
    return None
