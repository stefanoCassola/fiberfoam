# FiberFoam

FiberFoam is a specialized computational tool for **permeability simulation of fibrous microstructures**. It targets directed, aligned fiber architectures — computing the full permeability tensor of Statistical Volume Elements (SVEs) through CFD simulation on voxelized geometry.

Built on top of **OpenFOAM** (specifically a modified `simpleFoam` solver — `simpleFoamMod`), FiberFoam provides an end-to-end pipeline: from raw voxel geometry to meshing, optional ML-accelerated initial conditions, steady-state Stokes/Navier-Stokes flow simulation, and permeability extraction.

## What It Does

- **Voxel-to-mesh conversion**: Converts 3D binary voxel arrays (`.dat`, `.npy`) of fiber microstructures into OpenFOAM hexahedral meshes with proper boundary conditions for permeability measurement
- **Buffer zone injection**: Adds configurable fiber-free inlet/outlet regions to ensure fully developed flow
- **Connectivity filtering**: Removes disconnected fluid pockets that would produce singular linear systems
- **ML velocity prediction**: Uses ONNX neural network models to predict velocity fields as initial conditions, significantly accelerating solver convergence
- **CFD simulation**: Runs a modified SIMPLE algorithm (`simpleFoamMod`) to compute steady-state flow through the microstructure for each flow direction (X, Y, Z)
- **Permeability extraction**: Computes permeability via both volume-averaged velocity and flow-rate (Darcy) methods, with optional restriction to the fibrous region only
- **Fiber orientation detection**: FFT-based automatic estimation and correction of fiber alignment
- **Batch processing**: Process multiple geometries with identical settings in sequence

## Target Application

FiberFoam is designed for researchers and engineers working with **fiber-reinforced composites** and **porous fibrous media**. Typical use cases include:

- Computing permeability tensors for resin transfer molding (RTM) process simulation
- Characterizing flow resistance of non-woven fabrics, felts, and fiber mats
- Generating permeability databases from micro-CT or synthetically generated SVEs
- Validating analytical permeability models against numerical results

The tool assumes the input is a **voxelized binary geometry** where solid voxels represent fibers and void voxels represent the pore space (or vice versa, with automatic remapping).

## Quick Start (Docker — recommended)

The easiest way to run FiberFoam is via Docker, which bundles OpenFOAM, the solver, ML models, and the web GUI.

```bash
cd docker
./launch.sh
```

Open your browser at **http://localhost:3000**.

### Configuration

| Environment Variable | Default | Description |
|---|---|---|
| `FIBERFOAM_PORT` | `3000` | Host port for the web GUI |
| `FIBERFOAM_INPUT_DIR` | `./input` | Host directory with geometry files (mounted read-only) |
| `FIBERFOAM_OUTPUT_DIR` | `./output` | Host directory for simulation results |

Place your `.dat` or `.npy` geometry files in the input directory before starting, or upload them through the GUI.

### Windows

```powershell
cd docker
.\launch.ps1
```

## Web GUI

The GUI provides two main workflows accessible at `http://localhost:3000`:

### Single Pipeline

1. **Upload & Preprocess** — Upload a voxel geometry file, inspect it with the interactive 3D viewer, remap values if needed, estimate and correct fiber orientation
2. **Configure** — Select pipeline mode, flow directions, voxel size/resolution, buffer zones, ML model, and connectivity filtering
3. **Run** — Execute the pipeline (mesh → predict → simulate → post-process) with live progress, convergence charts, and residual monitoring
4. **Results** — View permeability results per direction, download CSV, save case files to a chosen output folder

### Batch Processing

1. **Select Folder** — Use the native OS folder picker to browse for a directory containing geometry files
2. **Pick Files** — Check/uncheck individual geometry files from the discovered list (Select All supported)
3. **Configure** — Same settings as single pipeline (mode, flow directions, resolution, buffers, etc.)
4. **Run** — All selected geometries are processed sequentially with live per-file progress tracking
5. **Export** — Download CSV results for all completed runs

## Build from Source

### Prerequisites

- C++17 compiler (GCC 9+ or Clang 10+)
- CMake >= 3.20
- OpenFOAM v2312
- Eigen3, FFTW3, yaml-cpp, nlohmann/json
- ONNX Runtime (optional, for ML prediction)

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Command-Line Usage

```bash
# Generate mesh for all flow directions
fiberFoamMesh -input geometry.dat -output ./case \
    -voxelSize 0.5e-6 -voxelRes 320 -flowDirection all \
    -inletLayers 10 -outletLayers 10 -connectivity

# Predict initial velocity fields (optional)
fiberFoamPredict -input geometry.dat -output ./case \
    -voxelRes 320 -modelRes 80 -modelsDir ./models

# Run the CFD solver (requires OpenFOAM)
simpleFoamMod -case ./case/x_dir

# Extract permeability
fiberFoamPostProcess -case ./case/x_dir -method both -fibrousRegionOnly
```

## Project Structure

```
fiberfoam/
├── src/
│   ├── libfiberfoam/    # Core C++ library (mesh generation, I/O, FFT)
│   ├── apps/            # CLI executables
│   └── solver/          # OpenFOAM solver (simpleFoamMod)
├── gui/
│   ├── backend/         # FastAPI orchestration layer (Python)
│   └── frontend/        # React + Vite + TypeScript web interface
├── models/              # ML models (ONNX format) and scaling factors
├── tests/               # Unit and integration tests
├── docker/              # Dockerfile, docker-compose, launch scripts
├── docs/                # Documentation (MkDocs)
└── scripts/             # Utility scripts
```

## How It Works

1. **Geometry input**: A 3D binary voxel array where each voxel is either solid (fiber) or void (pore space)
2. **Meshing**: Each void voxel becomes a hexahedral cell in the OpenFOAM mesh. Buffer zones (fiber-free layers) are prepended/appended along the flow direction to allow flow development
3. **Boundary conditions**: Inlet (fixed pressure), outlet (fixed pressure), walls (no-slip on fiber surfaces), and symmetry (on domain boundaries perpendicular to flow)
4. **Solver**: `simpleFoamMod` — a modified version of OpenFOAM's `simpleFoam` (SIMPLE algorithm for steady-state incompressible flow). Modifications include built-in permeability monitoring and convergence-based stopping
5. **Permeability**: Computed from Darcy's law using either the volume-averaged velocity field or the outlet flow rate, normalized by the applied pressure gradient and fluid viscosity

## License

GPL-3.0 — see [LICENSE](LICENSE)
