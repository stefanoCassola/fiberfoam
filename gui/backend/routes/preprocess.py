"""Geometry preprocessing endpoints: value remapping, orientation estimation, rotation."""

import os
import math

import numpy as np
from fastapi import APIRouter, HTTPException

from schemas import (
    AnalyzeResponse,
    AutoAlignRequest,
    AutoAlignResponse,
    OrientationResponse,
    PreprocessResponse,
    RemapRequest,
    RotateRequest,
)
from services.paths import UPLOAD_DIR

router = APIRouter()


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _load_array(filename: str) -> np.ndarray:
    """Load a .dat or .npy geometry file from UPLOAD_DIR and return a 3-D uint8 array."""
    filepath = os.path.join(UPLOAD_DIR, filename)
    if not os.path.isfile(filepath):
        raise HTTPException(status_code=404, detail=f"File not found: {filename}")

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
            arr = np.array([int(v) for v in values], dtype=np.uint8).reshape(res, res, nz)
    except HTTPException:
        raise
    except Exception as exc:
        raise HTTPException(status_code=400, detail=f"Failed to load geometry: {exc}")

    return arr


def _save_dat(arr: np.ndarray, filepath: str) -> None:
    """Save a 3-D array as a flat space-separated .dat file."""
    with open(filepath, "w") as f:
        flat = arr.ravel()
        f.write(" ".join(str(v) for v in flat))


def _crop_to_content(arr: np.ndarray, margin: int = 1) -> np.ndarray:
    """Crop a 3-D binary array to the bounding box of non-zero voxels.

    After scipy.ndimage.rotate with reshape=True the array is padded with
    zeros (= pore space).  This removes that padding so it does not look
    like buffer zones were added.
    """
    nonzero = np.argwhere(arr != 0)
    if len(nonzero) == 0:
        return arr

    lo = nonzero.min(axis=0)
    hi = nonzero.max(axis=0)

    # Apply margin but clamp to array bounds
    lo = np.maximum(lo - margin, 0)
    hi = np.minimum(hi + margin, np.array(arr.shape) - 1)

    return arr[lo[0]:hi[0]+1, lo[1]:hi[1]+1, lo[2]:hi[2]+1].copy()


def _pca_angle_from_projection(projection: np.ndarray, sigma: float = 4.0):
    """Run Gaussian blur + threshold + PCA on a 2-D projection.

    Parameters
    ----------
    projection : 2-D array whose axes are (k_a, k_b) in frequency space.
    sigma : Gaussian blur sigma.

    Returns
    -------
    (norm_angle, raw_angle, rotation_needed) in degrees, where
    raw_angle is the fiber-direction angle from the a-axis measured
    towards the b-axis, and rotation_needed = -raw_angle.
    """
    from scipy.ndimage import gaussian_filter

    projection = gaussian_filter(projection, sigma=sigma)

    max_val = projection.max()
    if max_val <= 0:
        return (0.0, 0.0, 0.0)
    threshold = 0.5 * max_val

    coords = np.argwhere(projection > threshold).astype(np.float64)
    if len(coords) < 2:
        return (0.0, 0.0, 0.0)

    mean = coords.mean(axis=0)
    centred = coords - mean
    cov = (centred.T @ centred) / (len(coords) - 1)
    eigenvalues, eigenvectors = np.linalg.eigh(cov)

    # Dominant frequency direction = eigenvector with largest eigenvalue
    dominant_freq_dir = eigenvectors[:, -1]

    # Fiber direction is orthogonal to dominant frequency direction
    fiber_dir = np.array([-dominant_freq_dir[1], dominant_freq_dir[0]])

    raw_angle = math.degrees(math.atan2(fiber_dir[1], fiber_dir[0]))

    # Normalise to [0, 90]
    norm = raw_angle % 180.0
    if norm < 0:
        norm += 180.0
    norm = min(norm, 180.0 - norm)

    rotation_needed = -raw_angle
    return (norm, raw_angle, rotation_needed)


def _estimate_orientation(arr: np.ndarray, sigma: float = 4.0):
    """Estimate dominant fiber orientation via FFT + PCA in two planes.

    Returns a dict with keys:
        xy_angle, xy_raw, xy_rotation   -- angle in XY plane (rotation around Z)
        xz_angle, xz_raw, xz_rotation   -- angle in XZ plane (rotation around Y)

    The .dat flat format is read in C-order as arr[x, y, z] (axis 0 = x,
    axis 1 = y, axis 2 = z).

    XY plane: project FFT magnitude along kz (axis 2) → (kx, ky) image.
    XZ plane: project FFT magnitude along ky (axis 1) → (kx, kz) image.
    """

    # 1. 3-D FFT
    fft_data = np.fft.fftn(arr.astype(np.float64))

    # 2. Magnitude spectrum + fftshift
    mag = np.fft.fftshift(np.abs(fft_data))
    del fft_data

    # 3. Zero the DC component (central region carries no directional info)
    c = [s // 2 for s in mag.shape]
    mag[c[0] - 1 : c[0] + 2, c[1] - 1 : c[1] + 2, c[2] - 1 : c[2] + 2] = 0

    # 4a. XY plane: project along kz (axis 2) → shape (nx, ny)
    nz = mag.shape[2]
    cent_z = nz // 2
    lo = max(0, cent_z - 2)
    hi = min(nz - 1, cent_z + 2)
    proj_xy = mag[:, :, lo : hi + 1].mean(axis=2)

    # 4b. XZ plane: project along ky (axis 1) → shape (nx, nz)
    ny = mag.shape[1]
    cent_y = ny // 2
    lo_y = max(0, cent_y - 2)
    hi_y = min(ny - 1, cent_y + 2)
    proj_xz = mag[:, lo_y : hi_y + 1, :].mean(axis=1)

    del mag

    # 5. PCA on each projection
    xy_angle, xy_raw, xy_rotation = _pca_angle_from_projection(proj_xy, sigma)
    xz_angle, xz_raw, xz_rotation = _pca_angle_from_projection(proj_xz, sigma)

    return {
        "xy_angle": xy_angle,
        "xy_raw": xy_raw,
        "xy_rotation": xy_rotation,
        "xz_angle": xz_angle,
        "xz_raw": xz_raw,
        "xz_rotation": xz_rotation,
    }


# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------

@router.get("/analyze/{filename}", response_model=AnalyzeResponse)
async def analyze_geometry(filename: str):
    """Analyze raw value distribution of a geometry file."""
    arr = _load_array(filename)
    unique_vals, counts = np.unique(arr, return_counts=True)
    value_counts = {str(int(v)): int(c) for v, c in zip(unique_vals, counts)}
    return AnalyzeResponse(
        filename=filename,
        shape=list(arr.shape),
        uniqueValues=[int(v) for v in unique_vals],
        valueCounts=value_counts,
    )


@router.post("/remap", response_model=PreprocessResponse)
async def remap_values(req: RemapRequest):
    """Remap voxel values: poreValue becomes 0 (fluid), other values mapped by otherMapping."""
    arr = _load_array(req.filename)

    pore_val = req.poreValue
    # Determine the complementary binary value (the other 0-or-1 value)
    complement = 1 if pore_val == 0 else 0

    # Start: everything = solid (1) if otherMapping is solid, else pore (0)
    default = 1 if req.otherMapping == "solid" else 0
    output = np.full_like(arr, default, dtype=np.uint8)
    # Explicitly set the pore value
    output[arr == pore_val] = 0
    # Explicitly set the complement (always solid)
    output[arr == complement] = 1

    total = output.size
    fluid_count = int((output == 0).sum())
    fluid_fraction = fluid_count / total if total > 0 else 0.0

    # Save preprocessed file
    base, ext = os.path.splitext(req.filename)
    out_name = f"{base}_preprocessed.dat"
    out_path = os.path.join(UPLOAD_DIR, out_name)
    _save_dat(output, out_path)

    res = output.shape[0]
    unique_vals = [int(v) for v in np.unique(output)]

    return PreprocessResponse(
        filename=out_name,
        resolution=res,
        fluidFraction=round(fluid_fraction, 6),
        shape=list(output.shape),
        uniqueValues=unique_vals,
    )


@router.get("/orientation/{filename}", response_model=OrientationResponse)
async def estimate_orientation(filename: str):
    """Estimate dominant fiber orientation via FFT analysis."""
    arr = _load_array(filename)
    ori = _estimate_orientation(arr)
    return OrientationResponse(
        xyAngle=round(ori["xy_angle"], 2),
        xyRawAngle=round(ori["xy_raw"], 2),
        xyRotation=round(ori["xy_rotation"], 2),
        xzAngle=round(ori["xz_angle"], 2),
        xzRawAngle=round(ori["xz_raw"], 2),
        xzRotation=round(ori["xz_rotation"], 2),
    )


@router.post("/rotate", response_model=PreprocessResponse)
async def rotate_geometry(req: RotateRequest):
    """Rotate geometry around a specified axis by a given angle."""
    from scipy.ndimage import rotate as ndimage_rotate

    arr = _load_array(req.filename)

    # Axes mapping: rotation around axis means rotating in the plane of the other two
    axes_map = {"x": (1, 2), "y": (0, 2), "z": (0, 1)}
    axes = axes_map[req.axis.value]

    rotated = ndimage_rotate(arr.astype(np.float64), req.angle, axes=axes, order=0, reshape=True)
    # Ensure binary and crop padding introduced by reshape=True
    result = (rotated > 0.5).astype(np.uint8)
    result = _crop_to_content(result)

    total = result.size
    fluid_count = int((result == 0).sum())
    fluid_fraction = fluid_count / total if total > 0 else 0.0

    base, ext = os.path.splitext(req.filename)
    out_name = f"{base}_rotated.dat"
    out_path = os.path.join(UPLOAD_DIR, out_name)
    _save_dat(result, out_path)

    res = result.shape[0]

    return PreprocessResponse(
        filename=out_name,
        resolution=res,
        fluidFraction=round(fluid_fraction, 6),
        shape=list(result.shape),
        uniqueValues=[int(v) for v in np.unique(result)],
    )


@router.post("/auto-align", response_model=AutoAlignResponse)
async def auto_align_geometry(req: AutoAlignRequest):
    """Estimate fiber orientation and auto-rotate to align fibers with x-axis.

    Applies up to two rotations:
    1. XZ-plane correction (around Y-axis) -- if fibers are tilted out of the XY plane
    2. XY-plane correction (around Z-axis) -- if fibers are not aligned with X in the XY plane
    """
    from scipy.ndimage import rotate as ndimage_rotate

    arr = _load_array(req.filename)
    ori = _estimate_orientation(arr)

    xz_rot = ori["xz_rotation"]
    xy_rot = ori["xy_rotation"]

    need_xz = abs(xz_rot) >= 0.5
    need_xy = abs(xy_rot) >= 0.5

    if not need_xz and not need_xy:
        total = arr.size
        fluid_count = int((arr == 0).sum())
        fluid_fraction = fluid_count / total if total > 0 else 0.0
        return AutoAlignResponse(
            xyAngle=round(ori["xy_angle"], 2),
            xzAngle=round(ori["xz_angle"], 2),
            xyRotationApplied=0.0,
            xzRotationApplied=0.0,
            filename=req.filename,
            resolution=arr.shape[0],
            fluidFraction=round(fluid_fraction, 6),
            shape=list(arr.shape),
        )

    result = arr.astype(np.float64)

    # Step 1: correct XZ tilt (rotate around Y-axis, i.e. in the (x, z) = (0, 2) plane)
    xz_applied = 0.0
    if need_xz:
        result = ndimage_rotate(result, xz_rot, axes=(0, 2), order=0, reshape=True)
        xz_applied = xz_rot

    # Step 2: correct XY angle (rotate around Z-axis, i.e. in the (x, y) = (0, 1) plane)
    xy_applied = 0.0
    if need_xy:
        # Re-estimate XY angle after XZ correction if we rotated
        if need_xz:
            tmp = (result > 0.5).astype(np.uint8)
            ori2 = _estimate_orientation(tmp)
            xy_rot2 = ori2["xy_rotation"]
            if abs(xy_rot2) >= 0.5:
                result = ndimage_rotate(result, xy_rot2, axes=(0, 1), order=0, reshape=True)
                xy_applied = xy_rot2
        else:
            result = ndimage_rotate(result, xy_rot, axes=(0, 1), order=0, reshape=True)
            xy_applied = xy_rot

    result = (result > 0.5).astype(np.uint8)
    result = _crop_to_content(result)

    total = result.size
    fluid_count = int((result == 0).sum())
    fluid_fraction = fluid_count / total if total > 0 else 0.0

    base, ext = os.path.splitext(req.filename)
    out_name = f"{base}_aligned.dat"
    out_path = os.path.join(UPLOAD_DIR, out_name)
    _save_dat(result, out_path)

    return AutoAlignResponse(
        xyAngle=round(ori["xy_angle"], 2),
        xzAngle=round(ori["xz_angle"], 2),
        xyRotationApplied=round(xy_applied, 2),
        xzRotationApplied=round(xz_applied, 2),
        filename=out_name,
        resolution=result.shape[0],
        fluidFraction=round(fluid_fraction, 6),
        shape=list(result.shape),
    )
