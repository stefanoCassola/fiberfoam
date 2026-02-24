# FiberFoam

High-performance voxel-to-OpenFOAM mesh conversion with ML-predicted velocity fields for fiber foam flow simulation.

## Features

- Convert 3D voxel geometry arrays to OpenFOAM hex meshes
- ML-based velocity field prediction using ONNX Runtime
- FFT-based fiber orientation estimation
- Fiber-free buffer region support
- Dual-method permeability post-processing
- Web-based GUI for interactive workflow
- Docker support for reproducible environments

## Quick Start

### Build from source

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Run

```bash
# Generate mesh
fiberFoamMesh -input geometry.dat -output ./case -voxelSize 0.5e-6 -voxelRes 320 -flowDirection all

# Predict velocity field
fiberFoamPredict -input geometry.dat -output ./predictions -voxelRes 320 -modelRes 80

# Post-process permeability
fiberFoamPostProcess -case ./case/x_dir -method both -fibrousRegionOnly
```

### Docker

```bash
docker-compose up gui
# Access GUI at http://localhost:3000
```

## Dependencies

- C++17 compiler (GCC 9+ or Clang 10+)
- CMake >= 3.20
- Eigen3
- FFTW3
- yaml-cpp
- nlohmann/json
- ONNX Runtime (optional, for ML prediction)
- Google Test (for tests)
- OpenFOAM 2312 (for solver, optional)

## Project Structure

```
fiberfoam/
├── src/
│   ├── libfiberfoam/    # Core C++ library
│   ├── apps/            # CLI executables
│   └── solver/          # OpenFOAM solver (simpleFoamMod)
├── gui/
│   ├── backend/         # FastAPI orchestration layer
│   └── frontend/        # React + Vite web interface
├── models/              # ML models (ONNX format)
├── tests/               # Unit and integration tests
├── docker/              # Docker configuration
├── docs/                # MkDocs documentation
└── examples/            # Example scripts and configs
```

## License

GPL-3.0 - see [LICENSE](LICENSE)
