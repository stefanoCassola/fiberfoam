from fastapi import APIRouter, UploadFile, File, HTTPException
from schemas import GeometryUploadResponse
import os
import shutil

router = APIRouter()

UPLOAD_DIR = os.environ.get("FIBERFOAM_UPLOAD_DIR", "/tmp/fiberfoam/uploads")


@router.post("/upload", response_model=GeometryUploadResponse)
async def upload_geometry(file: UploadFile = File(...)):
    """Upload a voxelised geometry file (.dat or .npy).

    The file is saved to ``UPLOAD_DIR`` and a quick analysis is performed:
    resolution is estimated as the cube root of the total voxel count, and
    the fluid fraction is computed (voxel value ``0`` = fluid).
    """
    if not file.filename:
        raise HTTPException(status_code=400, detail="No filename provided")

    allowed_extensions = (".dat", ".npy")
    if not file.filename.lower().endswith(allowed_extensions):
        raise HTTPException(
            status_code=400,
            detail=f"Unsupported file type. Allowed: {allowed_extensions}",
        )

    os.makedirs(UPLOAD_DIR, exist_ok=True)
    filepath = os.path.join(UPLOAD_DIR, file.filename)

    try:
        with open(filepath, "wb") as f:
            shutil.copyfileobj(file.file, f)
    except OSError as exc:
        raise HTTPException(status_code=500, detail=f"Failed to save file: {exc}")

    # --- Quick analysis -------------------------------------------------------
    try:
        if filepath.endswith(".npy"):
            import numpy as np

            arr = np.load(filepath)
            shape = list(arr.shape)
            n = arr.size
            fluid = int((arr == 0).sum()) / n
            res = shape[0]
        else:
            with open(filepath) as f:
                values = f.read().split()
            n = len(values)
            if n == 0:
                raise HTTPException(status_code=400, detail="Geometry file is empty")
            res = round(n ** (1 / 3))
            fluid = sum(1 for v in values if v == "0") / n
            nz = n // (res * res) if res > 0 else 0
            shape = [res, res, nz]
    except HTTPException:
        raise
    except Exception as exc:
        raise HTTPException(
            status_code=400, detail=f"Failed to analyse geometry: {exc}"
        )

    return GeometryUploadResponse(
        filename=file.filename,
        resolution=res,
        fluidFraction=round(fluid, 6),
        shape=shape,
    )


@router.get("/list")
async def list_geometries():
    """Return a list of previously uploaded geometry files."""
    if not os.path.isdir(UPLOAD_DIR):
        return {"files": []}
    files = sorted(
        f for f in os.listdir(UPLOAD_DIR) if f.endswith((".dat", ".npy"))
    )
    return {"files": files}


@router.delete("/{filename}")
async def delete_geometry(filename: str):
    """Remove an uploaded geometry file."""
    filepath = os.path.join(UPLOAD_DIR, filename)
    if not os.path.isfile(filepath):
        raise HTTPException(status_code=404, detail="File not found")
    try:
        os.remove(filepath)
    except OSError as exc:
        raise HTTPException(status_code=500, detail=f"Failed to delete file: {exc}")
    return {"deleted": filename}
