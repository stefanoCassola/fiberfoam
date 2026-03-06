import asyncio
import json
import os
import re

from fastapi import APIRouter, HTTPException, Query, WebSocket, WebSocketDisconnect
from schemas import SimulationRequest, SimulationResponse, JobStatus, JobStatusEnum
from services.executor import job_manager
from services.config_writer import write_simulation_config
from services.paths import RUN_BIN, SOLVER_BIN, OPENFOAM_BASHRC

router = APIRouter()


@router.post("/run", response_model=SimulationResponse)
async def run_simulation(req: SimulationRequest):
    """Launch an OpenFOAM simulation in *caseDir*.

    If ``fiberFoamRun`` is available it is used (it writes the config and
    calls the solver).  Otherwise the solver executable is invoked via
    bash with OpenFOAM environment sourced.
    """
    if not os.path.isdir(req.caseDir):
        raise HTTPException(
            status_code=400, detail=f"Case directory not found: {req.caseDir}"
        )

    config_path = os.path.join(req.caseDir, "fiberfoam_sim.yaml")
    write_simulation_config(
        config_path,
        case_dir=req.caseDir,
        solver=req.solver,
        max_iterations=req.maxIter,
        write_interval=req.writeInterval,
    )

    # Decide which executable to call
    if RUN_BIN:
        cmd = [RUN_BIN, "-config", config_path]
    elif SOLVER_BIN:
        # Run solver via bash to source OpenFOAM environment
        solver_cmd = f"source {OPENFOAM_BASHRC} && {SOLVER_BIN} -case {req.caseDir}"
        cmd = ["bash", "-c", solver_cmd]
    else:
        raise HTTPException(
            status_code=500,
            detail="Neither fiberFoamRun nor solver found. "
            "Set FIBERFOAM_RUN_BIN or FIBERFOAM_SOLVER_BIN.",
        )

    log_path = os.path.join(req.caseDir, "log.solver")
    job_id = await job_manager.run_command(cmd, cwd=req.caseDir, job_type="simulate")

    return SimulationResponse(
        jobId=job_id,
        status=JobStatusEnum.running,
        logPath=log_path,
    )


@router.get("/status/{job_id}", response_model=JobStatus)
async def simulation_status(job_id: str, since: int = Query(0, ge=0)):
    """Poll the status and recent log output of a simulation job.

    The *since* query parameter lets the client request only new lines
    (pass the length of previously received lines).
    """
    job = job_manager.get_job(job_id)
    log_lines = job_manager.get_log(job_id, since=since)

    progress = _estimate_progress(job, log_lines)

    return JobStatus(
        jobId=job_id,
        status=JobStatusEnum(job["status"]),
        progress=progress,
        returncode=job.get("returncode"),
        log=log_lines,
    )


@router.websocket("/ws/{job_id}")
async def simulation_log_ws(websocket: WebSocket, job_id: str):
    """WebSocket endpoint that streams solver log lines in real time.

    The client connects and receives JSON messages of the form::

        {"lines": ["Time = 100", "Ux: ..."], "status": "running"}

    The connection is kept alive until the job finishes or the client
    disconnects.
    """
    await websocket.accept()
    cursor = 0
    try:
        while True:
            job = job_manager.get_job(job_id)
            new_lines = job_manager.get_log(job_id, since=cursor)
            if new_lines:
                cursor += len(new_lines)
                await websocket.send_text(
                    json.dumps({"lines": new_lines, "status": job["status"]})
                )
            if job["status"] in ("completed", "failed", "not_found"):
                # Send final status and close
                await websocket.send_text(
                    json.dumps(
                        {
                            "lines": [],
                            "status": job["status"],
                            "returncode": job.get("returncode"),
                        }
                    )
                )
                await websocket.close()
                break
            await asyncio.sleep(0.5)
    except WebSocketDisconnect:
        pass  # Client disconnected, nothing to clean up


@router.post("/cancel/{job_id}")
async def cancel_simulation(job_id: str):
    """Cancel a running simulation."""
    cancelled = await job_manager.cancel_job(job_id)
    if not cancelled:
        raise HTTPException(
            status_code=400, detail="Job not found or not running"
        )
    return {"jobId": job_id, "status": "cancelled"}


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

_RE_TIME = re.compile(r"^Time\s*=\s*([\d.eE+-]+)")


def _estimate_progress(job: dict, recent_lines: list[str]) -> float | None:
    """Try to estimate solver progress from log output.

    OpenFOAM solvers print ``Time = <value>`` lines.  If we can find the
    current time and the max iterations from the job config we can compute
    a rough fraction.
    """
    if job.get("status") == "completed":
        return 1.0
    if job.get("status") != "running":
        return None

    # Walk the full log backwards to find latest Time
    all_lines = job.get("log", [])
    current_time = None
    for line in reversed(all_lines):
        m = _RE_TIME.match(line)
        if m:
            try:
                current_time = float(m.group(1))
            except ValueError:
                pass
            break

    if current_time is not None and current_time > 0:
        # We don't reliably know maxIter here, so return a bounded estimate.
        # The frontend can use iteration count directly if needed.
        return None  # Let the frontend compute from iteration / maxIter

    return None
