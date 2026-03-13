"""Filesystem browsing endpoints for output folder selection.

When running in Docker the host filesystem is bind-mounted at HOST_ROOT
(default /host).  The browse endpoint lets the user navigate the host
tree so they can pick any directory on their machine.  Paths sent to
and returned from these endpoints are **host-relative** (i.e. what the
user sees on their machine).  Internally the backend translates to the
container mount point.
"""
import os
import shutil

from fastapi import APIRouter, HTTPException, Query
from pydantic import BaseModel

from services.paths import HOST_ROOT, WORK_DIR

router = APIRouter()

# ---------------------------------------------------------------------------
# Path helpers
# ---------------------------------------------------------------------------

def _host_to_container(host_path: str) -> str:
    """Convert a host-absolute path to the container path via HOST_ROOT."""
    # Normalise to remove double slashes / trailing slashes
    hp = os.path.normpath(host_path) if host_path else "/"
    container = os.path.normpath(os.path.join(HOST_ROOT, hp.lstrip("/")))
    return container


def _container_to_host(container_path: str) -> str:
    """Convert a container path back to the host-absolute path."""
    norm = os.path.normpath(container_path)
    root = os.path.normpath(HOST_ROOT)
    if norm.startswith(root):
        rel = norm[len(root):]
        return rel if rel.startswith("/") else "/" + rel
    return container_path


def _is_under_host_root(container_path: str) -> bool:
    return os.path.normpath(container_path).startswith(os.path.normpath(HOST_ROOT))


# ---------------------------------------------------------------------------
# Browse
# ---------------------------------------------------------------------------

@router.get("/browse")
async def browse(path: str = Query("", description="Absolute path on the host filesystem")):
    """List directories and files at a given path on the host filesystem."""
    # When HOST_ROOT doesn't exist (not in Docker), browse OUTPUT_ROOT directly
    if not os.path.isdir(HOST_ROOT):
        return _browse_fallback(path)

    container_path = _host_to_container(path or "/")

    if not os.path.isdir(container_path):
        raise HTTPException(status_code=404, detail=f"Directory not found: {path or '/'}")

    dirs: list[dict] = []
    files: list[dict] = []

    try:
        for entry in sorted(os.scandir(container_path), key=lambda e: e.name.lower()):
            if entry.name.startswith("."):
                continue
            try:
                is_dir = entry.is_dir(follow_symlinks=True)
            except OSError:
                continue
            if is_dir:
                dirs.append({"name": entry.name, "type": "directory"})
            else:
                try:
                    size = entry.stat().st_size
                except OSError:
                    size = 0
                files.append({"name": entry.name, "type": "file", "size": size})
    except PermissionError:
        raise HTTPException(status_code=403, detail="Permission denied")

    host_path = _container_to_host(container_path)
    return {
        "path": host_path,
        "absPath": host_path,
        "root": "/",
        "dirs": dirs,
        "files": files,
    }


def _browse_fallback(path: str) -> dict:
    """Fallback browse for non-Docker environments (browse local filesystem)."""
    abs_path = os.path.normpath(path) if path else "/"
    if not os.path.isdir(abs_path):
        raise HTTPException(status_code=404, detail=f"Directory not found: {abs_path}")

    dirs: list[dict] = []
    files: list[dict] = []

    try:
        for entry in sorted(os.scandir(abs_path), key=lambda e: e.name.lower()):
            if entry.name.startswith("."):
                continue
            try:
                is_dir = entry.is_dir(follow_symlinks=True)
            except OSError:
                continue
            if is_dir:
                dirs.append({"name": entry.name, "type": "directory"})
            else:
                try:
                    size = entry.stat().st_size
                except OSError:
                    size = 0
                files.append({"name": entry.name, "type": "file", "size": size})
    except PermissionError:
        raise HTTPException(status_code=403, detail="Permission denied")

    return {
        "path": abs_path,
        "absPath": abs_path,
        "root": "/",
        "dirs": dirs,
        "files": files,
    }


# ---------------------------------------------------------------------------
# Mkdir
# ---------------------------------------------------------------------------

class MkdirRequest(BaseModel):
    path: str  # absolute host path


@router.post("/mkdir")
async def mkdir(req: MkdirRequest):
    """Create a new directory on the host filesystem."""
    if os.path.isdir(HOST_ROOT):
        container_path = _host_to_container(req.path)
        if not _is_under_host_root(container_path):
            raise HTTPException(status_code=400, detail="Path outside host mount")
    else:
        container_path = os.path.normpath(req.path)

    try:
        os.makedirs(container_path, exist_ok=True)
    except OSError as exc:
        raise HTTPException(status_code=400, detail=str(exc))
    return {"path": req.path, "absPath": req.path}


# ---------------------------------------------------------------------------
# Save results
# ---------------------------------------------------------------------------

class SaveResultsRequest(BaseModel):
    pipelineId: str
    destPath: str  # absolute host path
    force: bool = False


@router.post("/save-results")
async def save_results(req: SaveResultsRequest):
    """Copy pipeline result files to a user-chosen directory on the host."""
    from routes.pipeline import _pipelines

    state = _pipelines.get(req.pipelineId)
    if not state:
        raise HTTPException(status_code=404, detail="Pipeline not found")

    case_dirs = state.get("caseDirs", [])
    if not case_dirs:
        raise HTTPException(status_code=404, detail="No case directories to save")

    # Resolve destination: host path -> container path
    if os.path.isdir(HOST_ROOT):
        dest = _host_to_container(req.destPath)
        if not _is_under_host_root(dest):
            raise HTTPException(status_code=400, detail="Path outside host mount")
    else:
        dest = os.path.normpath(req.destPath)

    os.makedirs(dest, exist_ok=True)

    copied = []
    skipped = []
    errors = []

    for case_dir in case_dirs:
        if not os.path.isdir(case_dir):
            continue
        dir_name = os.path.basename(case_dir)
        target = os.path.join(dest, dir_name)

        if os.path.normpath(case_dir) == os.path.normpath(target):
            skipped.append(dir_name)
            continue

        if os.path.isdir(target) and not req.force:
            skipped.append(dir_name)
            continue

        try:
            shutil.copytree(case_dir, target, dirs_exist_ok=True)
            copied.append(dir_name)
        except PermissionError as exc:
            errors.append(f"Permission denied: {dir_name} ({exc.filename})")
        except OSError as exc:
            errors.append(f"{dir_name}: {exc}")

    if not copied and not skipped and errors:
        raise HTTPException(
            status_code=403,
            detail=f"Cannot write to destination: {'; '.join(errors)}",
        )

    # Delete successfully copied source case dirs from Docker work dir
    work_norm = os.path.normpath(WORK_DIR)
    deleted = []
    for case_dir in case_dirs:
        dir_name = os.path.basename(case_dir)
        if dir_name not in copied:
            continue
        if not os.path.normpath(case_dir).startswith(work_norm):
            continue
        try:
            shutil.rmtree(case_dir)
            deleted.append(dir_name)
        except OSError:
            pass

    result: dict = {
        "destPath": req.destPath,
        "absPath": req.destPath,
        "copiedDirs": copied,
    }
    if deleted:
        result["deletedDirs"] = deleted
    if skipped:
        result["skippedDirs"] = skipped
        result["alreadyExists"] = True
    if errors:
        result["warnings"] = errors
    return result
