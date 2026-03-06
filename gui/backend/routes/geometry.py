from fastapi import APIRouter, UploadFile, File, HTTPException
from schemas import GeometryUploadResponse
from services.paths import UPLOAD_DIR, BATCH_DIR
import os
import shutil

router = APIRouter()


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


@router.get("/voxels/{filename}")
async def get_geometry_voxels(filename: str):
    """Return downsampled voxel data for 3D visualization (max 64^3)."""
    filepath = os.path.join(UPLOAD_DIR, filename)
    if not os.path.isfile(filepath):
        raise HTTPException(status_code=404, detail="File not found")

    import numpy as np

    try:
        if filepath.endswith(".npy"):
            arr = np.load(filepath)
        else:
            with open(filepath) as f:
                values = f.read().split()
            n = len(values)
            if n == 0:
                raise HTTPException(status_code=400, detail="Geometry file is empty")
            res = round(n ** (1 / 3))
            nz = n // (res * res) if res > 0 else 0
            arr = np.array([int(v) for v in values], dtype=np.uint8).reshape(
                res, res, nz
            )
    except HTTPException:
        raise
    except Exception as exc:
        raise HTTPException(
            status_code=400, detail=f"Failed to load geometry: {exc}"
        )

    # Downsample to max 64^3
    max_dim = 64
    if any(s > max_dim for s in arr.shape):
        factors = tuple(max(1, s // max_dim) for s in arr.shape)
        arr = arr[:: factors[0], :: factors[1], :: factors[2]]

    # Extract surface-only voxels: solid voxels with at least one fluid neighbor
    solid = arr > 0
    # Pad with zeros (fluid) on all sides, then check 6-connectivity neighbors
    padded = np.pad(solid, 1, mode="constant", constant_values=False)
    has_fluid_neighbor = (
        ~padded[:-2, 1:-1, 1:-1]  # -x
        | ~padded[2:, 1:-1, 1:-1]  # +x
        | ~padded[1:-1, :-2, 1:-1]  # -y
        | ~padded[1:-1, 2:, 1:-1]  # +y
        | ~padded[1:-1, 1:-1, :-2]  # -z
        | ~padded[1:-1, 1:-1, 2:]  # +z
    )
    surface = solid & has_fluid_neighbor
    positions = np.argwhere(surface).tolist()

    return {"positions": positions, "dimensions": list(arr.shape)}


@router.get("/list")
async def list_geometries():
    """Return a list of previously uploaded geometry files."""
    if not os.path.isdir(UPLOAD_DIR):
        return {"files": []}
    files = sorted(
        f for f in os.listdir(UPLOAD_DIR) if f.endswith((".dat", ".npy"))
    )
    return {"files": files}


@router.get("/batch-files")
async def list_batch_files():
    """Return a list of geometry files available in the batch input directory."""
    if not os.path.isdir(BATCH_DIR):
        return {"files": [], "batchDir": BATCH_DIR}
    files = sorted(
        f for f in os.listdir(BATCH_DIR) if f.endswith((".dat", ".npy"))
    )
    return {"files": files, "batchDir": BATCH_DIR}


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
