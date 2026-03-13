import glob
import os

import json
import logging
import urllib.request
from datetime import datetime, timezone

from fastapi import FastAPI, Query
from pydantic import BaseModel
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

logger = logging.getLogger(__name__)

from routes import geometry, prediction, mesh, simulation, postprocess, pipeline, results, preprocess, filesystem, paraview
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
    version=os.environ.get("FIBERFOAM_VERSION", "dev"),
    description="Web GUI for fiber foam flow simulation",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
    allow_private_network=True,  # Allow HTTPS (Vercel) → localhost requests
    max_age=0,  # Never cache preflight responses to avoid stale CORS errors
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
app.include_router(paraview.router, prefix="/api/paraview", tags=["paraview"])


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
        "version": os.environ.get("FIBERFOAM_VERSION", "dev"),
        "components": {
            "mesh": MESH_BIN is not None,
            "predict": PREDICT_BIN is not None,
            "solver": SOLVER_BIN is not None,
            "postprocess": POSTPROCESS_BIN is not None,
        },
        "models": [os.path.basename(m) for m in models],
    }


DOCKER_IMAGE = "ghcr.io/stefanocassola/fiberfoam"


def _parse_version(tag: str) -> tuple[int, ...] | None:
    """Parse a semver-like tag (e.g. 'v0.2.0' or '0.2.0') into a tuple."""
    import re
    m = re.match(r"v?(\d+(?:\.\d+)*)", tag)
    if not m:
        return None
    return tuple(int(x) for x in m.group(1).split("."))


@app.get("/api/updates/check")
async def check_updates():
    """Check if a newer Docker image is available on ghcr.io."""
    current = os.environ.get("FIBERFOAM_VERSION", "dev")
    try:
        # Get an anonymous token for the public package
        token_url = f"https://ghcr.io/token?scope=repository:stefanocassola/fiberfoam:pull"
        req = urllib.request.Request(token_url)
        with urllib.request.urlopen(req, timeout=10) as resp:
            token = json.loads(resp.read())["token"]

        # List tags to find semver tags
        tags_url = f"https://ghcr.io/v2/stefanocassola/fiberfoam/tags/list"
        req = urllib.request.Request(tags_url, headers={
            "Authorization": f"Bearer {token}",
        })
        with urllib.request.urlopen(req, timeout=10) as resp:
            tags_data = json.loads(resp.read())
            tags = tags_data.get("tags", [])

        # Find the latest semver tag
        semver_tags = [(t, _parse_version(t)) for t in tags]
        semver_tags = [(t, v) for t, v in semver_tags if v is not None]
        semver_tags.sort(key=lambda x: x[1], reverse=True)
        latest_tag = semver_tags[0][0] if semver_tags else None
        latest_ver = semver_tags[0][1] if semver_tags else None

        current_ver = _parse_version(current)
        update_available = False
        if current_ver and latest_ver:
            update_available = latest_ver > current_ver
        elif current == "dev" and latest_tag:
            update_available = True

        return {
            "currentVersion": current,
            "latestVersion": latest_tag,
            "availableTags": tags,
            "updateAvailable": update_available,
            "image": DOCKER_IMAGE,
        }
    except Exception as exc:
        logger.warning("Update check failed: %s", exc)
        return {
            "currentVersion": current,
            "latestVersion": None,
            "availableTags": [],
            "updateAvailable": None,
            "error": str(exc),
            "image": DOCKER_IMAGE,
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
