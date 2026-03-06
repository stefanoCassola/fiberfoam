"""Filesystem browsing endpoints for output folder selection."""
import os
import shutil

from fastapi import APIRouter, HTTPException, Query
from pydantic import BaseModel

from services.paths import OUTPUT_ROOT, WORK_DIR

router = APIRouter()

_OUTPUT_ROOT = OUTPUT_ROOT


def _safe_path(rel: str) -> str:
    """Resolve a relative path against _OUTPUT_ROOT, guarding against traversal."""
    joined = os.path.normpath(os.path.join(_OUTPUT_ROOT, rel))
    if not joined.startswith(os.path.normpath(_OUTPUT_ROOT)):
        raise HTTPException(status_code=400, detail="Path outside output root")
    return joined


@router.get("/browse")
async def browse(path: str = Query("", description="Relative path within the output root")):
    """List directories and files at a given path under the output root."""
    abs_path = _safe_path(path)

    if not os.path.isdir(abs_path):
        # If root doesn't exist yet, create it and return empty
        if abs_path == os.path.normpath(_OUTPUT_ROOT):
            os.makedirs(abs_path, exist_ok=True)
        else:
            raise HTTPException(status_code=404, detail="Directory not found")

    dirs: list[dict] = []
    files: list[dict] = []

    try:
        for entry in sorted(os.scandir(abs_path), key=lambda e: e.name.lower()):
            if entry.name.startswith("."):
                continue
            if entry.is_dir(follow_symlinks=False):
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
        "path": path or "",
        "absPath": abs_path,
        "root": _OUTPUT_ROOT,
        "dirs": dirs,
        "files": files,
    }


class MkdirRequest(BaseModel):
    path: str


@router.post("/mkdir")
async def mkdir(req: MkdirRequest):
    """Create a new directory under the output root."""
    abs_path = _safe_path(req.path)
    try:
        os.makedirs(abs_path, exist_ok=True)
    except OSError as exc:
        raise HTTPException(status_code=400, detail=str(exc))
    return {"path": req.path, "absPath": abs_path}


class SaveResultsRequest(BaseModel):
    pipelineId: str
    destPath: str  # relative to output root
    force: bool = False  # overwrite existing directories


@router.post("/save-results")
async def save_results(req: SaveResultsRequest):
    """Copy pipeline result files to a user-chosen output directory."""
    from routes.pipeline import _pipelines

    state = _pipelines.get(req.pipelineId)
    if not state:
        raise HTTPException(status_code=404, detail="Pipeline not found")

    case_dirs = state.get("caseDirs", [])
    if not case_dirs:
        raise HTTPException(status_code=404, detail="No case directories to save")

    dest = _safe_path(req.destPath)
    os.makedirs(dest, exist_ok=True)

    copied = []
    skipped = []
    errors = []
    force = getattr(req, "force", False)

    for case_dir in case_dirs:
        if not os.path.isdir(case_dir):
            continue
        dir_name = os.path.basename(case_dir)
        target = os.path.join(dest, dir_name)

        # Source and destination are the same directory
        if os.path.normpath(case_dir) == os.path.normpath(target):
            skipped.append(dir_name)
            continue

        # Target already exists — need force flag to overwrite
        if os.path.isdir(target) and not force:
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
    # Only delete from WORK_DIR (internal), never from OUTPUT_ROOT (host mount)
    work_norm = os.path.normpath(WORK_DIR)
    deleted = []
    for case_dir in case_dirs:
        dir_name = os.path.basename(case_dir)
        if dir_name not in copied:
            continue
        # Only delete if the source is inside WORK_DIR (not on host mount)
        if not os.path.normpath(case_dir).startswith(work_norm):
            continue
        try:
            shutil.rmtree(case_dir)
            deleted.append(dir_name)
        except OSError:
            pass  # best-effort cleanup

    result: dict = {
        "destPath": req.destPath,
        "absPath": dest,
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
