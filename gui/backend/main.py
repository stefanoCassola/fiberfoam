import glob
import os

import json
from datetime import datetime, timezone

from fastapi import FastAPI, Query
from pydantic import BaseModel
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from routes import geometry, prediction, mesh, simulation, postprocess, pipeline, results, preprocess, filesystem
from services.executor import job_manager
from services.paths import (
    MESH_BIN,
    PREDICT_BIN,
    SOLVER_BIN,
    POSTPROCESS_BIN,
    MODELS_DIR,
    WORK_DIR,
    UPLOAD_DIR,
    JOBS_DIR,
    FEEDBACK_DIR,
)

app = FastAPI(
    title="FiberFoam",
    version="0.1.0",
    description="Web GUI for fiber foam flow simulation",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(geometry.router, prefix="/api/geometry", tags=["geometry"])
app.include_router(prediction.router, prefix="/api/prediction", tags=["prediction"])
app.include_router(mesh.router, prefix="/api/mesh", tags=["mesh"])
app.include_router(simulation.router, prefix="/api/simulation", tags=["simulation"])
app.include_router(postprocess.router, prefix="/api/postprocess", tags=["postprocess"])
app.include_router(pipeline.router, prefix="/api/pipeline", tags=["pipeline"])
app.include_router(results.router, prefix="/api/results", tags=["results"])
app.include_router(preprocess.router, prefix="/api/preprocess", tags=["preprocess"])
app.include_router(filesystem.router, prefix="/api/filesystem", tags=["filesystem"])


@app.on_event("startup")
async def startup():
    """Create required directories and reload persisted jobs."""
    for d in (WORK_DIR, UPLOAD_DIR, JOBS_DIR, FEEDBACK_DIR):
        os.makedirs(d, exist_ok=True)
    job_manager.load_persisted_jobs()


@app.get("/api/health")
async def health():
    """Health check with component availability."""
    models = (
        glob.glob(os.path.join(MODELS_DIR, "**/*.onnx"), recursive=True)
        if os.path.isdir(MODELS_DIR)
        else []
    )
    return {
        "status": "ok",
        "version": "0.1.0",
        "components": {
            "mesh": MESH_BIN is not None,
            "predict": PREDICT_BIN is not None,
            "solver": SOLVER_BIN is not None,
            "postprocess": POSTPROCESS_BIN is not None,
        },
        "models": [os.path.basename(m) for m in models],
    }


@app.get("/api/system/stats")
async def system_stats():
    """Return live system resource usage (memory)."""
    mem: dict = {"totalGb": 0, "usedGb": 0, "availableGb": 0, "percent": 0}
    try:
        with open("/proc/meminfo") as f:
            info = {}
            for line in f:
                parts = line.split()
                if len(parts) >= 2:
                    info[parts[0].rstrip(":")] = int(parts[1])  # kB
            total = info.get("MemTotal", 0)
            available = info.get("MemAvailable", 0)
            used = total - available
            mem = {
                "totalGb": round(total / 1048576, 1),
                "usedGb": round(used / 1048576, 1),
                "availableGb": round(available / 1048576, 1),
                "percent": round(used / total * 100, 1) if total > 0 else 0,
            }
    except Exception:
        pass
    return mem


@app.get("/api/jobs/{job_id}")
async def get_job_status(job_id: str, since: int = Query(0, ge=0)):
    """Generic job status endpoint for any job type."""
    job = job_manager.get_job(job_id)
    log_lines = job_manager.get_log(job_id, since=since)

    progress = None
    if job.get("status") == "completed":
        progress = 1.0

    return {
        "jobId": job_id,
        "status": job.get("status", "not_found"),
        "progress": progress,
        "returncode": job.get("returncode"),
        "log": log_lines,
    }


@app.get("/api/jobs")
async def list_jobs():
    """List all known jobs (in-memory + persisted)."""
    from services import job_store

    jobs = job_store.list_jobs()
    return {"jobs": jobs}


class FeedbackBody(BaseModel):
    category: str = "general"
    message: str
    contact: str = ""


@app.post("/api/feedback")
async def submit_feedback(body: FeedbackBody):
    """Save user feedback to a local JSON file."""
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    entry = {
        "timestamp": ts,
        "category": body.category,
        "message": body.message,
        "contact": body.contact,
    }
    filepath = os.path.join(FEEDBACK_DIR, f"feedback_{ts}.json")
    with open(filepath, "w") as f:
        json.dump(entry, f, indent=2)
    return {"status": "ok", "id": ts}


@app.get("/api/feedback")
async def list_feedback():
    """List all feedback entries (for admin review)."""
    entries = []
    if os.path.isdir(FEEDBACK_DIR):
        for fname in sorted(os.listdir(FEEDBACK_DIR), reverse=True):
            if fname.endswith(".json"):
                with open(os.path.join(FEEDBACK_DIR, fname)) as f:
                    entries.append(json.load(f))
    return {"feedback": entries}


# Serve static frontend files in production -- MUST be last (catch-all)
frontend_dir = os.path.join(os.path.dirname(__file__), "..", "frontend", "dist")
if os.path.isdir(frontend_dir):
    app.mount("/", StaticFiles(directory=frontend_dir, html=True), name="frontend")
