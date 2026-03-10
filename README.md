# FiberFoam

FiberFoam is a specialized computational tool for **permeability simulation of fibrous microstructures**. It targets directed, aligned fiber architectures — computing the full permeability tensor of Statistical Volume Elements (SVEs) through CFD simulation on voxelized geometry.

Built on top of **OpenFOAM** (specifically an extended `simpleFoam` solver — `simpleFoamMod`), FiberFoam provides an end-to-end pipeline: from raw voxel geometry to meshing, optional ML-accelerated initial conditions, steady-state Stokes/Navier-Stokes flow simulation, and permeability extraction.

## Three Ways to Use FiberFoam

| Method | What You Need | Best For |
|--------|--------------|----------|
| **Online** | A browser + Docker | Quickest start — guided setup, always up-to-date UI |
| **Docker** | Docker installed | Self-contained, no build step, full offline use |
| **Source** | C++ toolchain, OpenFOAM, Node.js | Developers, custom modifications |

### Option 1: Online (Vercel + Local Docker Backend)

Visit **[fiberfoam.vercel.app](https://fiberfoam.vercel.app)** — the web app guides you through installing Docker and starting the backend. All computation runs on your machine; the website is just the interface.

1. Open [fiberfoam.vercel.app](https://fiberfoam.vercel.app)
2. Follow the setup guide (installs Docker if needed)
3. Run the provided `docker run` command in your terminal
4. The page auto-detects the backend and proceeds to the app

### Option 2: Docker (Recommended for Offline Use)

```bash
cd docker
./launch.sh
```

Open your browser at **http://localhost:3000**.

#### Windows

```powershell
cd docker
.\launch.ps1
```

#### Configuration

| Environment Variable | Default | Description |
|---|---|---|
| `FIBERFOAM_PORT` | `3000` | Host port for the web GUI |
| `FIBERFOAM_INPUT_DIR` | `./input` | Host directory with geometry files (mounted read-only) |
| `FIBERFOAM_OUTPUT_DIR` | `./output` | Host directory for simulation results |

Place your `.dat` or `.npy` geometry files in the input directory before starting, or upload them through the GUI.

### Option 3: Build from Source

#### Prerequisites

- C++17 compiler (GCC 9+ or Clang 10+)
- CMake >= 3.20
- OpenFOAM v2312
- Eigen3, FFTW3, yaml-cpp, nlohmann/json
- ONNX Runtime (optional, for ML prediction)
- Node.js 18+ (for the web GUI)

#### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## What It Does

- **Voxel-to-mesh conversion**: Converts 3D binary voxel arrays (`.dat`, `.npy`) of fiber microstructures into OpenFOAM hexahedral meshes with proper boundary conditions for permeability measurement
- **Buffer zone injection**: Adds configurable fiber-free inlet/outlet regions to ensure fully developed flow
- **Connectivity filtering**: Removes disconnected fluid pockets that would produce singular linear systems
- **ML velocity prediction**: Uses neural network models to predict velocity fields as initial conditions, potentially accelerating solver convergence. Available models:
  - **3D U-Net** (~1.4M parameters, ONNX format)
  - **FNO — Fourier Neural Operator** (~28.3M parameters, TensorFlow SavedModel)
  - **Note:** Only low-resolution models (80 voxels) are currently available. Inaccurate predictions can lead to longer convergence times than starting from scratch — monitor residuals and consider disabling prediction if convergence stalls
- **CFD simulation**: Runs the OpenFOAM SIMPLE algorithm (`simpleFoamMod`) to compute steady-state flow through the microstructure for each flow direction (X, Y, Z)
- **Permeability extraction**: Computes permeability via both volume-averaged velocity and flow-rate (Darcy) methods, with optional restriction to the fibrous region only. The full permeability tensor (including off-diagonal components) requires CFD simulation in all three spatial directions. The ML-only mode computes only the diagonal components, as the models predict only the velocity component parallel to the applied pressure gradient
- **Fiber orientation detection**: FFT-based automatic estimation and correction of fiber alignment
- **Batch processing**: Process multiple geometries with identical settings in sequence
- **Live monitoring**: Real-time RAM usage, convergence charts, and per-step progress tracking
- **ParaView integration**: Launch ParaView directly from the GUI to visualize meshes, velocity fields, and simulation results

## Target Application

FiberFoam is designed for researchers and engineers working with **fiber-reinforced composites** and **porous fibrous media**. Typical use cases include:

- Computing permeability tensors for resin transfer molding (RTM) process simulation
- Characterizing flow resistance of non-woven fabrics, felts, and fiber mats
- Generating permeability databases from micro-CT or synthetically generated SVEs
- Validating analytical permeability models against numerical results

The tool assumes the input is a **voxelized binary geometry** where solid voxels represent fibers and void voxels represent the pore space (or vice versa, with automatic remapping).

## Web GUI

The GUI provides two main workflows accessible at `http://localhost:3000` (Docker) or [fiberfoam.vercel.app](https://fiberfoam.vercel.app) (online):

### Single Pipeline

1. **Upload & Preprocess** — Upload a voxel geometry file, inspect it with the interactive 3D viewer, remap values if needed, estimate and correct fiber orientation
2. **Configure** — Select pipeline mode, flow directions, voxel size/resolution, buffer zones, ML model, and connectivity filtering
3. **Run** — Execute the pipeline (mesh → predict → simulate → post-process) with live progress, convergence charts, residual monitoring, and RAM usage
4. **Results** — View permeability results per direction, download CSV, save case files, launch ParaView

### Batch Processing

1. **Select Folder** — Browse for a directory containing geometry files using the built-in file browser
2. **Pick Files** — Check/uncheck individual geometry files (Select All supported)
3. **Preprocess** — Optionally apply value remapping and auto-alignment to all selected files
4. **Configure** — Same settings as single pipeline (mode, flow directions, resolution, buffers, etc.)
5. **Run** — All selected geometries are processed sequentially with live per-file progress tracking
6. **Export** — Download CSV results for all completed runs

Batch processing continues in the background even if you navigate away from the page.

## How It Works

1. **Geometry input**: A 3D binary voxel array where each voxel is either solid (fiber) or void (pore space)
2. **Preprocessing** *(optional)*: Remap multi-valued arrays to binary; detect and correct fiber orientation via FFT analysis
3. **Meshing**: Each void voxel becomes a hexahedral cell in the OpenFOAM mesh. Buffer zones (fiber-free layers) are prepended/appended along the flow direction to allow flow development
4. **ML prediction** *(optional)*: Neural network predicts the velocity field as an initial condition, potentially reducing CFD iterations
5. **Boundary conditions**: Inlet (fixed pressure), outlet (fixed pressure), walls (no-slip on fiber surfaces), and periodic (on domain boundaries perpendicular to flow)
6. **Solver**: `simpleFoamMod` — an extended version of OpenFOAM's `simpleFoam` using the standard SIMPLE algorithm for steady-state incompressible flow. The extension adds permeability-based convergence monitoring and automatic stopping when the permeability value stabilizes
7. **Permeability**: Computed from Darcy's law using either the volume-averaged velocity field or the outlet flow rate, normalized by the applied pressure gradient and fluid viscosity

## Command-Line Usage

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
├── models/              # ML models (ONNX + TF SavedModel)
├── tests/               # Unit and integration tests
├── docker/              # Dockerfile, docker-compose, launch scripts
├── docs/                # Documentation
└── scripts/             # Utility scripts
```

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Browser (React/TypeScript)                             │
│  ├── Pipeline Page (single geometry workflow)           │
│  ├── Batch Page (multi-file processing)                 │
│  └── History Page (past job results)                    │
└────────────────────────┬────────────────────────────────┘
                         │ REST API
┌────────────────────────▼────────────────────────────────┐
│  FastAPI Backend (Python)                               │
│  ├── Job orchestration & async execution                │
│  ├── Preprocessing (remap, rotate, auto-align)          │
│  ├── Pipeline sequencing (mesh → predict → sim → post)  │
│  └── Results management & CSV export                    │
└────────────────────────┬────────────────────────────────┘
                         │ subprocess calls
┌────────────────────────▼────────────────────────────────┐
│  C++ Executables + OpenFOAM                             │
│  ├── fiberFoamMesh       (voxel → OpenFOAM hex mesh)    │
│  ├── fiberFoamPredict    (ML velocity prediction)       │
│  ├── simpleFoamMod       (OpenFOAM SIMPLE solver)       │
│  ├── fiberFoamPostProcess (permeability extraction)     │
│  └── ParaView            (visualization)                │
└─────────────────────────────────────────────────────────┘
```

## Feedback

Use the **Feedback** button in the sidebar of the GUI to submit bug reports, feature requests, or questions — no account required.

## License

GPL-3.0 — see [LICENSE](LICENSE)
