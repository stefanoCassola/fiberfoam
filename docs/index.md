# FiberFoam

**High-performance voxel-to-OpenFOAM mesh conversion with ML-accelerated velocity prediction for fiber-reinforced composite flow simulation.**

FiberFoam is a complete pipeline for computing permeability of fibrous microstructures. It converts voxelized geometry representations into OpenFOAM-compatible hexahedral meshes, optionally predicts initial velocity fields using ONNX machine learning models, runs steady-state CFD simulations with a modified SIMPLE solver, and post-processes the results to extract permeability tensors.

---

## Key Features

- **Voxel-to-Mesh Conversion** -- Converts binary voxel arrays (geometry files) into OpenFOAM hexahedral meshes with automatic boundary patch classification, connectivity filtering, and fiber-free buffer regions for inlet/outlet zones.

- **ML Velocity Prediction** -- Uses trained ONNX neural network models to predict initial velocity fields at coarse resolution (e.g., 80^3), which are then upsampled and mapped onto the full-resolution mesh as initial conditions, significantly accelerating solver convergence.

- **Modified SIMPLE Solver** (`simpleFoamMod`) -- A custom OpenFOAM solver based on `simpleFoam` that computes permeability at every time step using both volume-averaged and flow-rate methods, with built-in convergence monitoring.

- **Automated Post-Processing** -- Extracts permeability values for the fibrous region of interest (excluding buffer zones), computes fiber volume content, and writes results to CSV for downstream analysis.

- **Web GUI** -- A FastAPI + React web interface for uploading geometry, configuring simulations, launching jobs, and viewing results interactively.

- **Docker Support** -- Multi-stage Dockerfile bundles OpenFOAM 2312, the C++ tools, and the web GUI into a single deployable image.

---

## Quick Links

| Topic | Link |
|---|---|
| Install from source or Docker | [Installation](installation.md) |
| Run your first simulation | [Quick Start](quickstart.md) |
| Command-line tools reference | [CLI Reference](cli_reference.md) |
| Web interface walkthrough | [GUI Guide](gui_guide.md) |
| Permeability theory and formulas | [Permeability Calculation](theory/permeability.md) |
| Buffer region design | [Fiber-Free Regions](theory/fiber_free_regions.md) |
| C++ architecture overview | [Architecture](development/architecture.md) |
| How to contribute | [Contributing](development/contributing.md) |

---

## Pipeline Overview

```
geometry.dat
     |
     v
 fiberFoamMesh          Generate OpenFOAM hex mesh for each flow direction
     |
     v
 fiberFoamPredict       (optional) Predict velocity field with ML model
     |
     v
 simpleFoamMod          Run steady-state SIMPLE solver with permeability monitoring
     |
     v
 fiberFoamPostProcess   Extract permeability, FVC, convergence data
     |
     v
 permeabilityInfo.csv   Final results
```

---

## System Requirements

- **C++17** compiler (GCC >= 9, Clang >= 10)
- **CMake** >= 3.20
- **OpenFOAM** v2312 (for `simpleFoamMod` and mesh utilities)
- **Eigen3**, **FFTW3**, **yaml-cpp**, **nlohmann-json**
- (Optional) **ONNX Runtime** for ML prediction
- (Optional) **Python 3.10+** and **Node.js 20+** for the web GUI

---

## License

FiberFoam is released under the GNU General Public License v3.0. See `LICENSE` for details. The modified OpenFOAM solver retains the original OpenFOAM Foundation copyright and GPLv3 license.
