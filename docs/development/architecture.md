# Architecture

This page provides an overview of the FiberFoam C++ architecture: library structure, module responsibilities, data flow, and build system.

---

## High-Level Overview

FiberFoam follows a modular architecture with a central static library (`libfiberfoam`) and multiple thin application executables that consume it. A separate OpenFOAM solver (`simpleFoamMod`) is built using the standard OpenFOAM `wmake` system.

```
fiberfoam/
    CMakeLists.txt              # Top-level CMake configuration
    src/
        libfiberfoam/           # Core static library
            common/             # Types, logging, version, timer
            config/             # YAML configuration parsing
            geometry/           # Voxel arrays, fiber-free regions, region tracking
            mesh/               # Hex mesh generation, connectivity, face classification
            analysis/           # Fiber orientation analysis, velocity reconstruction
            io/                 # OpenFOAM file I/O (read/write), CSV, array I/O
            ml/                 # ONNX model registry, predictor, scaling
            postprocessing/     # Permeability calculation, convergence monitoring
        apps/                   # Application executables
            fiberFoamMesh/
            fiberFoamPostProcess/
            fiberFoamRun/
            fiberFoamInfo/
            fiberFoamPredict/       # (ONNX only)
            fiberFoamConvertModel/  # (ONNX only)
        solver/
            simpleFoamMod/      # Modified OpenFOAM SIMPLE solver
    gui/
        backend/                # FastAPI Python backend
        frontend/               # React + Vite + TypeScript frontend
    models/                     # ONNX model files
    tests/                      # Unit and integration tests
    docker/                     # Dockerfile
```

---

## Core Library: libfiberfoam

The `libfiberfoam` static library contains all domain logic. Applications are thin wrappers that parse command-line arguments and call library functions.

### Module Dependency Graph

```
common  <----  config
  ^              ^
  |              |
geometry  <--  mesh  <--  io
  ^              ^         ^
  |              |         |
  +---- analysis |         |
  |              |         |
  +------ ml ----+         |
  |                        |
  +-- postprocessing ------+
```

### Module Details

#### common

| File | Purpose |
|---|---|
| `Types.h` | Core type definitions: `FlowDirection`, `CellRegion`, `FluidProperties`, `Point3D`, `VoxelCoord`, `FaceVertices`, `CellData`, `MeshData`, `PermeabilityResult` |
| `Logger.h/.cpp` | Configurable logging with verbosity levels |
| `Timer.h` | RAII timer for profiling code sections |
| `Version.h.in` | Version template, configured by CMake at build time |

#### config

| File | Purpose |
|---|---|
| `SimulationConfig.h/.cpp` | Central configuration struct with YAML serialization/deserialization. Holds all parameters: geometry paths, fluid properties, buffer zones, mesh options, solver settings, post-processing options |
| `FluidProperties.h` | Re-exports `FluidProperties` from `Types.h` with documentation of default values |

#### geometry

| File | Purpose |
|---|---|
| `VoxelArray.h/.cpp` | 3D voxel array container. Reads binary geometry files, provides solid/fluid queries, supports resampling |
| `FiberFreeRegion.h/.cpp` | Pads geometry with inlet/outlet buffer layers along the flow axis. Produces a `PaddedGeometry` with geometry and region mask |
| `RegionTracker.h/.cpp` | Maps mesh cell indices to regions (fibrous, buffer inlet, buffer outlet). Used for ROI-filtered post-processing |

#### mesh

| File | Purpose |
|---|---|
| `HexMeshBuilder.h/.cpp` | Main mesh generation pipeline. Takes a `VoxelArray` and options, produces a complete `MeshData` (points, faces, owner/neighbour, boundary patches, cell map) |
| `Connectivity.h/.cpp` | Flood-fill connectivity check. Removes cells not connected to the main fluid region |
| `FaceGenerator.h/.cpp` | Generates internal and boundary faces from the cell map. Classifies faces into inlet, outlet, walls, and periodic patches |
| `MeshData.h` | (Types defined in `Types.h`) Struct holding the complete mesh: points, faces, owner, neighbour, boundary patches, cell map |

#### analysis

| File | Purpose |
|---|---|
| `FiberOrientation.h/.cpp` | Computes fiber orientation statistics from the voxelized geometry using FFT-based structure tensor analysis |
| `VelocityReconstruction.h/.cpp` | Upsamples predicted velocity fields from model resolution to mesh resolution using trilinear interpolation |

#### io

| File | Purpose |
|---|---|
| `FoamWriter.h/.cpp` | Writes OpenFOAM case files: `polyMesh/` (points, faces, owner, neighbour, boundary), `0/` (U, p), `system/` (controlDict, fvSchemes, fvSolution, blockMeshDict), `constant/transportProperties` |
| `FoamReader.h/.cpp` | Reads OpenFOAM result fields (velocity, pressure) from time directories |
| `ArrayIO.h/.cpp` | Binary I/O for voxel arrays and floating-point arrays |
| `CsvWriter.h/.cpp` | Writes permeability results and convergence data to CSV files |

#### ml

| File | Purpose |
|---|---|
| `ModelRegistry.h/.cpp` | Loads and manages ONNX model files. Supports loading from a YAML registry file or auto-detecting from a directory. Provides model lookup by direction and resolution |
| `OnnxPredictor.h/.cpp` | Runs ONNX inference to predict velocity fields. Resamples input geometry to model resolution, runs the model, and returns the predicted velocity array |
| `Scaling.h/.cpp` | Input/output normalization for ML models |

!!! note
    The `ml` module is conditionally compiled. When `ENABLE_ONNX=OFF`, these files are excluded and the `FIBERFOAM_HAS_ONNX` preprocessor macro is not defined.

#### postprocessing

| File | Purpose |
|---|---|
| `Permeability.h/.cpp` | Computes permeability from simulation results using volume-averaged and flow-rate methods. Supports ROI filtering via `RegionTracker` |
| `Convergence.h/.cpp` | Monitors permeability convergence over iterations using sliding-window linear regression |

---

## Application Executables

Each application is a single `main.cpp` file that:

1. Parses command-line arguments (or loads a YAML config)
2. Calls the appropriate `libfiberfoam` functions
3. Reports results

The `CMakeLists.txt` in `src/apps/` defines a helper function that creates each executable and links it against `libfiberfoam`:

```cmake
function(add_fiberfoam_app APP_NAME)
    add_executable(${APP_NAME} ${APP_NAME}/main.cpp)
    target_link_libraries(${APP_NAME} PRIVATE fiberfoam)
    install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)
endfunction()
```

---

## OpenFOAM Solver: simpleFoamMod

The modified solver lives in `src/solver/simpleFoamMod/` and is built with the OpenFOAM `wmake` system, not CMake. It consists of:

| File | Purpose |
|---|---|
| `simpleFoamMod.C` | Main solver loop. Extends `simpleFoam` with permeability calculation and convergence checking |
| `createFields.H` | Field initialization (velocity U, pressure p, flux phi) |
| `UEqn.H` | Momentum equation assembly and solution |
| `pEqn.H` | Pressure equation (SIMPLE correction) |
| `permCalc.H` | Per-iteration permeability calculation for both methods |
| `permConv.H` | Convergence monitoring with polynomial regression |

The solver uses the standard OpenFOAM `singlePhaseTransportModel` and reads fluid properties from `constant/transportProperties`.

---

## Data Flow

The typical data flow through the pipeline:

```
1. Read geometry file     -->  VoxelArray
2. Pad with buffers       -->  PaddedGeometry (geometry + regionMask)
3. Build hex mesh          -->  MeshData (points, faces, cells, patches)
4. Filter connectivity     -->  MeshData (pruned)
5. (Optional) ML predict   -->  velocity field at model resolution
6. (Optional) Upsample     -->  velocity field at mesh resolution
7. Write OpenFOAM case     -->  polyMesh/ + 0/U + 0/p + system/ + constant/
8. Run simpleFoamMod       -->  converged velocity + pressure fields
9. Read results            -->  per-cell velocities and pressures
10. Compute permeability   -->  PermeabilityResult (K_vol, K_flow, FVC)
11. Write CSV              -->  permeabilityInfo.csv
```

---

## Build System

### CMake Targets

| Target | Type | Dependencies |
|---|---|---|
| `fiberfoam` | Static library | Eigen3, FFTW3, yaml-cpp, nlohmann-json, (ONNX Runtime) |
| `fiberFoamMesh` | Executable | `fiberfoam` |
| `fiberFoamPostProcess` | Executable | `fiberfoam` |
| `fiberFoamRun` | Executable | `fiberfoam` |
| `fiberFoamInfo` | Executable | `fiberfoam` |
| `fiberFoamPredict` | Executable | `fiberfoam` (ONNX only) |
| `fiberFoamConvertModel` | Executable | `fiberfoam` (ONNX only) |

### Conditional Compilation

ONNX Runtime support is controlled by the `ENABLE_ONNX` CMake option:

- When enabled, CMake searches for ONNX Runtime via `find_package` or `pkg-config`
- If found, the `FIBERFOAM_HAS_ONNX` compile definition is added to `libfiberfoam`
- The `ml/` source files and the two ONNX-dependent executables are included in the build
- If not found, a warning is printed and ML prediction is disabled

### Version Configuration

The version is defined in the top-level `CMakeLists.txt` (`project(fiberfoam VERSION 0.1.0)`) and propagated to `Version.h` via `configure_file`:

```cmake
configure_file(
    ${CMAKE_SOURCE_DIR}/src/libfiberfoam/common/Version.h.in
    ${CMAKE_BINARY_DIR}/generated/Version.h
)
```
