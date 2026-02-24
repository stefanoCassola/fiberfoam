"""Utility for writing YAML config files consumed by the C++ executables."""

import os
import yaml
from typing import Optional


def write_mesh_config(
    output_path: str,
    *,
    input_path: str,
    voxel_size: float = 0.5e-6,
    voxel_resolution: int = 320,
    flow_direction: str = "x",
    inlet_buffer: int = 0,
    outlet_buffer: int = 0,
    connectivity: bool = True,
) -> str:
    """Write a YAML config suitable for ``fiberFoamMesh`` and return the path."""
    cfg = {
        "geometry": {
            "input": input_path,
            "voxelResolution": voxel_resolution,
            "voxelSize": voxel_size,
        },
        "flow": {
            "directions": [flow_direction],
        },
        "bufferZones": {
            "inletLayers": inlet_buffer,
            "outletLayers": outlet_buffer,
        },
        "mesh": {
            "connectivityCheck": connectivity,
            "autoBoundaryFaceSets": True,
            "periodic": False,
        },
        "output": {
            "path": os.path.dirname(output_path),
        },
    }
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w") as f:
        yaml.dump(cfg, f, default_flow_style=False)
    return output_path


def write_prediction_config(
    output_path: str,
    *,
    input_path: str,
    voxel_resolution: int = 320,
    model_resolution: int = 80,
    models_dir: str = "",
    flow_directions: Optional[list[str]] = None,
) -> str:
    """Write a YAML config suitable for ``fiberFoamPredict``."""
    cfg = {
        "geometry": {
            "input": input_path,
            "voxelResolution": voxel_resolution,
        },
        "flow": {
            "directions": flow_directions or ["x"],
        },
        "prediction": {
            "enabled": True,
            "modelsDir": models_dir,
            "modelResolution": model_resolution,
        },
        "output": {
            "path": os.path.dirname(output_path),
        },
    }
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w") as f:
        yaml.dump(cfg, f, default_flow_style=False)
    return output_path


def write_simulation_config(
    output_path: str,
    *,
    case_dir: str,
    solver: str = "simpleFoamMod",
    max_iterations: int = 1000000,
    write_interval: int = 50000,
) -> str:
    """Write a YAML config suitable for ``fiberFoamRun`` / solver."""
    cfg = {
        "solver": {
            "name": solver,
            "maxIterations": max_iterations,
            "writeInterval": write_interval,
            "convergence": {
                "enabled": True,
                "slope": 0.01,
                "window": 10,
                "errorBound": 0.01,
            },
        },
        "output": {
            "path": case_dir,
        },
    }
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w") as f:
        yaml.dump(cfg, f, default_flow_style=False)
    return output_path


def write_postprocess_config(
    output_path: str,
    *,
    case_dir: str,
    method: str = "both",
    fibrous_region_only: bool = True,
) -> str:
    """Write a YAML config suitable for ``fiberFoamPostProcess``."""
    cfg = {
        "postProcessing": {
            "fibrousRegionOnly": fibrous_region_only,
            "method": method,
        },
        "output": {
            "path": case_dir,
        },
    }
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w") as f:
        yaml.dump(cfg, f, default_flow_style=False)
    return output_path
