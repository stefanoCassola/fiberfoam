"""Centralized executable and directory path resolution."""
import os
import shutil


def find_executable(name: str, env_var: str) -> str | None:
    """Resolve executable: check env var, then shutil.which(), then /usr/local/bin/."""
    path = os.environ.get(env_var)
    if path and os.path.isfile(path):
        return path
    path = shutil.which(name)
    if path:
        return path
    fallback = f"/usr/local/bin/{name}"
    if os.path.isfile(fallback):
        return fallback
    return None


# Module-level constants
MESH_BIN = find_executable("fiberFoamMesh", "FIBERFOAM_MESH_BIN")
PREDICT_BIN = find_executable("fiberFoamPredict", "FIBERFOAM_PREDICT_BIN")
SOLVER_BIN = find_executable("simpleFoamMod", "FIBERFOAM_SOLVER_BIN")
RUN_BIN = find_executable("fiberFoamRun", "FIBERFOAM_RUN_BIN")
POSTPROCESS_BIN = find_executable("fiberFoamPostProcess", "FIBERFOAM_POSTPROCESS_BIN")

MODELS_DIR = os.environ.get("FIBERFOAM_MODELS_DIR", "/app/models")
SCALING_FACTORS = os.environ.get(
    "FIBERFOAM_SCALING_FACTORS", os.path.join(MODELS_DIR, "scaling_factors.json")
)
WORK_DIR = os.environ.get("FIBERFOAM_WORK_DIR", "/tmp/fiberfoam/cases")
UPLOAD_DIR = os.environ.get("FIBERFOAM_UPLOAD_DIR", "/tmp/fiberfoam/uploads")
BATCH_DIR = os.environ.get("FIBERFOAM_BATCH_DIR", "/data/input")
JOBS_DIR = os.environ.get("FIBERFOAM_JOBS_DIR", "/data/jobs")
OUTPUT_ROOT = os.environ.get("FIBERFOAM_OUTPUT_ROOT", WORK_DIR)

# OpenFOAM bashrc path for sourcing before running the solver
OPENFOAM_BASHRC = os.environ.get(
    "OPENFOAM_BASHRC", "/usr/lib/openfoam/openfoam2312/etc/bashrc"
)
