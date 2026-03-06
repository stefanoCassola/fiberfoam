import asyncio
import uuid
from typing import Optional

from services import job_store


class JobManager:
    """Manages long-running subprocess jobs (C++ executables) with log capture."""

    def __init__(self):
        self.jobs: dict[str, dict] = {}

    async def run_command(
        self,
        cmd: list[str],
        cwd: Optional[str] = None,
        env: Optional[dict] = None,
        job_type: str = "unknown",
    ) -> str:
        """Launch *cmd* as an async subprocess and return a job-ID for tracking.

        Output is captured line-by-line and stored in ``self.jobs[job_id]["log"]``.
        """
        job_id = str(uuid.uuid4())[:8]
        self.jobs[job_id] = {
            "status": "running",
            "cmd": cmd,
            "log": [],
            "process": None,
            "returncode": None,
            "type": job_type,
        }

        # Persist on creation
        job_store.save_job(job_id, {
            "id": job_id,
            "type": job_type,
            "status": "running",
            "cmd": [str(c) for c in cmd],
            "cwd": cwd,
            "log": [],
        })

        import os
        process = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            cwd=cwd,
            env=env,
            start_new_session=True,  # own process group for clean signal delivery
        )
        self.jobs[job_id]["process"] = process

        async def _read_output():
            while True:
                line = await process.stdout.readline()
                if not line:
                    break
                self.jobs[job_id]["log"].append(line.decode().rstrip())

            await process.wait()
            rc = process.returncode
            self.jobs[job_id]["status"] = "completed" if rc == 0 else "failed"
            self.jobs[job_id]["returncode"] = rc

            # Persist on completion/failure
            log_lines = self.jobs[job_id]["log"]
            job_store.save_job(job_id, {
                "id": job_id,
                "type": self.jobs[job_id].get("type", "unknown"),
                "status": self.jobs[job_id]["status"],
                "returncode": rc,
                "log": log_lines[-200:],  # persist last 200 lines
            })

        asyncio.create_task(_read_output())
        return job_id

    def get_job(self, job_id: str) -> dict:
        """Return full job dict or a sentinel ``{"status": "not_found"}``."""
        return self.jobs.get(job_id, {"status": "not_found"})

    def get_log(self, job_id: str, since: int = 0) -> list[str]:
        """Return log lines starting from index *since*."""
        job = self.jobs.get(job_id)
        if not job:
            return []
        return job["log"][since:]

    async def cancel_job(self, job_id: str) -> bool:
        """Send SIGTERM to a running process.  Returns True if signal was sent."""
        job = self.jobs.get(job_id)
        if not job or job["status"] != "running":
            return False
        proc = job.get("process")
        if proc and proc.returncode is None:
            # Kill the whole process group (bash + solver child)
            import signal
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            except (ProcessLookupError, PermissionError, OSError):
                proc.terminate()
            job["status"] = "failed"
            job["log"].append("--- job cancelled by user ---")
            job_store.save_job(job_id, {
                "id": job_id,
                "type": job.get("type", "unknown"),
                "status": "failed",
                "returncode": -1,
                "log": job["log"][-200:],
            })
            return True
        return False

    def load_persisted_jobs(self) -> None:
        """Reload completed/failed jobs from disk on startup.

        Running jobs cannot be resumed, so they are marked as failed.
        """
        for stored in job_store.list_jobs():
            jid = stored.get("id")
            if not jid or jid in self.jobs:
                continue
            status = stored.get("status", "unknown")
            if status == "running":
                status = "failed"  # cannot resume
                stored["status"] = status
                job_store.save_job(jid, stored)
            self.jobs[jid] = {
                "status": status,
                "cmd": stored.get("cmd", []),
                "log": stored.get("log", []),
                "process": None,
                "returncode": stored.get("returncode"),
                "type": stored.get("type", "unknown"),
            }


# Module-level singleton so all route modules share the same instance.
job_manager = JobManager()
