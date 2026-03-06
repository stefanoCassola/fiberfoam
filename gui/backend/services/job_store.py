"""File-based JSON storage for job persistence.

Each job/pipeline gets a JSON file: ``{job_id}.json`` in JOBS_DIR.
"""
import json
import os
from datetime import datetime, timezone
from typing import Any

from services.paths import JOBS_DIR


def _job_path(job_id: str) -> str:
    return os.path.join(JOBS_DIR, f"{job_id}.json")


def save_job(job_id: str, data: dict[str, Any]) -> None:
    """Write or update a job JSON file."""
    os.makedirs(JOBS_DIR, exist_ok=True)
    data.setdefault("id", job_id)
    data.setdefault("created_at", datetime.now(timezone.utc).isoformat())
    data["updated_at"] = datetime.now(timezone.utc).isoformat()
    with open(_job_path(job_id), "w") as f:
        json.dump(data, f, indent=2, default=str)


def load_job(job_id: str) -> dict[str, Any] | None:
    """Read a job JSON file.  Returns None if not found."""
    path = _job_path(job_id)
    if not os.path.isfile(path):
        return None
    with open(path) as f:
        return json.load(f)


def list_jobs() -> list[dict[str, Any]]:
    """List all stored jobs, sorted by creation time descending."""
    os.makedirs(JOBS_DIR, exist_ok=True)
    jobs: list[dict[str, Any]] = []
    for name in os.listdir(JOBS_DIR):
        if not name.endswith(".json"):
            continue
        path = os.path.join(JOBS_DIR, name)
        try:
            with open(path) as f:
                jobs.append(json.load(f))
        except (json.JSONDecodeError, OSError):
            continue
    jobs.sort(key=lambda j: j.get("created_at", ""), reverse=True)
    return jobs


def delete_job(job_id: str) -> bool:
    """Remove a job file.  Returns True if it existed."""
    path = _job_path(job_id)
    if os.path.isfile(path):
        os.remove(path)
        return True
    return False
