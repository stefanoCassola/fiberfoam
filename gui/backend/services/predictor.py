"""Pure Python ONNX prediction + Darcy permeability calculation.

No C++ binaries needed — runs entirely in Python using onnxruntime and scipy.
"""
import json
import os
from dataclasses import dataclass

import numpy as np

from services.paths import MODELS_DIR, SCALING_FACTORS


@dataclass
class QuickPredictionResult:
    direction: str
    permeability: float
    fiberVolumeContent: float
    meanVelocity: float
    flowLength: float


def load_geometry(path: str) -> np.ndarray:
    """Load a voxelized geometry from .dat or .npy file."""
    if path.endswith(".npy"):
        return np.load(path).astype(np.float32)

    # .dat file: flat text, infer cube resolution from line count
    raw = np.loadtxt(path, dtype=np.float32)
    n = round(raw.size ** (1.0 / 3.0))
    if n * n * n != raw.size:
        raise ValueError(
            f"Cannot reshape {raw.size} values into a cube — "
            f"closest cube root is {n}"
        )
    return raw.reshape(n, n, n)


def _invert_geometry(geom: np.ndarray) -> np.ndarray:
    """Invert convention: 0->1, 1->0 (matching training convention)."""
    out = geom.copy()
    out[geom == 0] = 1
    out[geom == 1] = 0
    return out


def _downsample(geom: np.ndarray, target_res: int) -> np.ndarray:
    """Downsample geometry to model resolution using cubic interpolation."""
    from scipy.ndimage import zoom

    current_res = geom.shape[0]
    if current_res == target_res:
        return geom
    factor = target_res / current_res
    return zoom(geom, factor, order=3).astype(np.float32)


def _load_scaling_factors() -> dict:
    """Load velocity scaling factors from JSON.

    Uses "max velocity" to match the C++ ScalingFactors::fromJson which
    extracts the same field for denormalising raw ONNX model output.
    """
    with open(SCALING_FACTORS) as f:
        data = json.load(f)
    # The file has a nested structure: {"res80": [{"x": {...}}, ...], ...}
    # Extract "max velocity" for each direction — matches C++ Scaling.cpp
    factors = {}
    for res_key, directions in data.items():
        factors[res_key] = {}
        for entry in directions:
            for direction, stats in entry.items():
                factors[res_key][direction] = stats["max velocity"]
    return factors


def _find_model(direction: str, model_res: int, model_folder: str = "") -> tuple[str, str]:
    """Find a model file and return (path, format).

    format is 'onnx' or 'tf'.
    """
    if model_folder:
        folder = os.path.join(MODELS_DIR, model_folder)
    else:
        folder = os.path.join(MODELS_DIR, f"res{model_res}")

    # Check ONNX first
    onnx_path = os.path.join(folder, f"{direction}_{model_res}.onnx")
    if os.path.isfile(onnx_path):
        return onnx_path, "onnx"

    # Check TF SavedModel directory
    tf_path = os.path.join(folder, f"{direction}_{model_res}_tf")
    if os.path.isdir(tf_path) and os.path.isfile(os.path.join(tf_path, "saved_model.pb")):
        return tf_path, "tf"

    raise FileNotFoundError(
        f"No model found for direction={direction}, res={model_res} in {folder}"
    )


def _run_onnx_inference(
    geom: np.ndarray, direction: str, model_res: int, model_folder: str = ""
) -> np.ndarray:
    """Run model inference for a single direction (ONNX or TF SavedModel)."""
    model_path, fmt = _find_model(direction, model_res, model_folder)

    # Model expects (batch, X, Y, Z, channels) — channels-last
    input_tensor = geom.reshape(1, *geom.shape, 1)

    if fmt == "onnx":
        import onnxruntime as ort
        session = ort.InferenceSession(model_path)
        input_name = session.get_inputs()[0].name
        result = session.run(None, {input_name: input_tensor})
        return result[0].squeeze()

    else:  # TF SavedModel
        import tensorflow as tf
        model = tf.saved_model.load(model_path)
        sig = model.signatures["serving_default"]
        tf_input = tf.constant(input_tensor)
        output = sig(input_1=tf_input)
        # Get the first (and only) output tensor
        result = list(output.values())[0].numpy()
        return result.squeeze()


def predict_permeability(
    geometry_path: str,
    directions: list[str],
    voxel_size: float = 0.5e-6,
    voxel_res: int = 320,
    model_res: int = 80,
    model_folder: str = "",
    inlet_buffer: int = 0,
    outlet_buffer: int = 0,
    nu: float = 7.934782609e-05,
    density: float = 920.0,
    delta_p: float = 1.0,
    save_velocity_dir: str = "",
) -> list[QuickPredictionResult]:
    """Run ONNX prediction and compute permeability via Darcy's law.

    Parameters
    ----------
    geometry_path : path to .dat or .npy geometry file
    directions : list of flow directions ("x", "y", "z")
    voxel_size : physical size of one voxel in meters
    voxel_res : resolution of the input geometry
    model_res : resolution the ML model was trained at
    inlet_buffer : number of buffer voxels at inlet
    outlet_buffer : number of buffer voxels at outlet
    nu : kinematic viscosity in m^2/s
    density : fluid density in kg/m^3
    delta_p : kinematic pressure drop (p/rho), typically 1.0

    Returns
    -------
    List of QuickPredictionResult, one per direction.
    """
    # Load and prepare geometry
    geom_raw = load_geometry(geometry_path)
    geom = _invert_geometry(geom_raw)

    # Downsample to model resolution
    geom_model = _downsample(geom, model_res)

    # Load scaling factors
    scaling = _load_scaling_factors()
    res_key = f"res{model_res}"
    if res_key not in scaling:
        raise ValueError(f"No scaling factors for resolution {model_res}")

    # Compute FVC from the original-resolution geometry
    # After inversion: 0 = fluid, >0 = solid (fibers)
    # But the convention is: 1 = fluid (pore), 0 = solid (fiber) in the inverted geometry
    # Actually in the inverted geometry: original 0 (pore) -> 1 (fluid), original 1 (fiber) -> 0 (solid)
    # Fluid voxels are where geom > 0.5
    fluid_mask_full = geom > 0.5
    total_voxels = geom.size
    fluid_voxels = fluid_mask_full.sum()
    fvc = 1.0 - (fluid_voxels / total_voxels)

    # Flow length excluding buffer zones (at model resolution for the prediction)
    flow_voxels = model_res - inlet_buffer - outlet_buffer
    # Scale flow length to physical units using the original voxel size and resolution
    flow_length = flow_voxels * (voxel_size * voxel_res / model_res)

    results = []
    for direction in directions:
        scale_factor = scaling[res_key].get(direction)
        if scale_factor is None:
            raise ValueError(f"No scaling factor for direction {direction} at {res_key}")

        # Run inference
        raw_output = _run_onnx_inference(geom_model, direction, model_res, model_folder=model_folder)

        # Scale output to physical velocity
        velocity = raw_output * scale_factor

        # Optionally save velocity field to disk
        if save_velocity_dir:
            os.makedirs(save_velocity_dir, exist_ok=True)
            np.save(os.path.join(save_velocity_dir, f"predicted_{direction}.npy"), velocity)

        # Build fluid mask at model resolution (after inversion: fluid = 1)
        fluid_mask = geom_model > 0.5

        # Exclude buffer zones from mean velocity calculation
        # Buffer zones are at the inlet/outlet along the flow direction axis
        dir_axis = {"x": 0, "y": 1, "z": 2}[direction]
        buf_mask = np.ones_like(fluid_mask)
        if inlet_buffer > 0:
            slices = [slice(None)] * 3
            slices[dir_axis] = slice(0, inlet_buffer)
            buf_mask[tuple(slices)] = False
        if outlet_buffer > 0:
            slices = [slice(None)] * 3
            slices[dir_axis] = slice(model_res - outlet_buffer, model_res)
            buf_mask[tuple(slices)] = False

        # Mean velocity over fluid voxels in the fibrous region
        active_mask = fluid_mask & buf_mask
        if active_mask.sum() == 0:
            raise ValueError(f"No active fluid voxels for direction {direction}")

        # For permeability, use only the main flow component
        mean_vel = float(np.mean(velocity[active_mask]))

        # Darcy's law (matching C++ Permeability.cpp):
        #   K = -(avgU * nu * density * flowLength) / (pOut - pIn)
        # With kinematic pressure pIn=1, pOut=0 → dP = -1:
        #   K = avgU * nu * density * flowLength
        permeability = abs(mean_vel) * nu * density * flow_length / delta_p

        results.append(QuickPredictionResult(
            direction=direction,
            permeability=permeability,
            fiberVolumeContent=float(fvc),
            meanVelocity=mean_vel,
            flowLength=flow_length,
        ))

    return results
