import asyncio
import uuid
from typing import Optional


class JobManager:
    """Manages long-running subprocess jobs (C++ executables) with log capture."""

    def __init__(self):
        self.jobs: dict[str, dict] = {}

    async def run_command(
        self, cmd: list[str], cwd: Optional[str] = None, env: Optional[dict] = None
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
        }

        process = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            cwd=cwd,
            env=env,
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
            proc.terminate()
            job["status"] = "failed"
            job["log"].append("--- job cancelled by user ---")
            return True
        return False


# Module-level singleton so all route modules share the same instance.
job_manager = JobManager()
