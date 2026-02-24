# CLI Reference

FiberFoam provides six command-line executables. Four are always built; two additional tools require ONNX Runtime.

---

## fiberFoamMesh

Converts a voxelized geometry file into an OpenFOAM hexahedral mesh.

### Usage

```bash
fiberFoamMesh [options]
```

### Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-input` | string | (required) | Path to the geometry file (binary voxel array) |
| `-output` | string | (required) | Output directory for the OpenFOAM case(s) |
| `-voxelSize` | float | `0.5e-6` | Physical size of one voxel edge [m] |
| `-voxelRes` | int | `320` | Number of voxels per axis in the input geometry |
| `-flowDirection` | string | `x` | Flow direction: `x`, `y`, `z`, or `all` |
| `-inletLayers` | int | `0` | Number of fluid-only buffer layers at the inlet |
| `-outletLayers` | int | `0` | Number of fluid-only buffer layers at the outlet |
| `-connectivity` | flag | off | Enable connectivity check (remove disconnected cells) |
| `-periodic` | flag | off | Generate periodic boundary conditions |
| `-config` | string | -- | Load options from a YAML configuration file |

### Examples

Generate a mesh for X-direction flow with buffer zones:

```bash
fiberFoamMesh \
    -input geometry.dat \
    -output ./case \
    -voxelSize 0.5e-6 \
    -voxelRes 320 \
    -flowDirection x \
    -inletLayers 10 \
    -outletLayers 10 \
    -connectivity
```

Generate meshes for all three directions:

```bash
fiberFoamMesh \
    -input geometry.dat \
    -output ./case \
    -flowDirection all \
    -connectivity
```

### Output Structure

```
<output>/<dir>_dir/
    constant/
        polyMesh/
            points
            faces
            owner
            neighbour
            boundary
        transportProperties
    0/
        U
        p
    system/
        controlDict
        fvSchemes
        fvSolution
        blockMeshDict
```

---

## fiberFoamPredict

Uses trained ONNX models to predict initial velocity fields for a given geometry. The predicted fields are written as OpenFOAM initial conditions to accelerate solver convergence.

!!! note
    This tool is only available when built with `ENABLE_ONNX=ON`.

### Usage

```bash
fiberFoamPredict [options]
```

### Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-input` | string | (required) | Path to the geometry file |
| `-output` | string | (required) | Output directory (must contain case directories from `fiberFoamMesh`) |
| `-voxelRes` | int | `320` | Voxel resolution of the input geometry |
| `-modelRes` | int | `80` | Resolution of the ML model (geometry is resampled to this) |
| `-modelsDir` | string | `./models` | Directory containing ONNX model files |
| `-flowDirection` | string | `all` | Which direction(s) to predict: `x`, `y`, `z`, or `all` |
| `-config` | string | -- | Load options from a YAML configuration file |

### Examples

Predict all three directions:

```bash
fiberFoamPredict \
    -input geometry.dat \
    -output ./case \
    -voxelRes 320 \
    -modelRes 80 \
    -modelsDir ./models
```

Predict only the X direction:

```bash
fiberFoamPredict \
    -input geometry.dat \
    -output ./case \
    -flowDirection x \
    -modelsDir ./models
```

### Model Directory Layout

```
models/
    res80/
        x_80.onnx
        y_80.onnx
        z_80.onnx
```

Models can also be registered via a `models.yml` file (see [models/models.yml](https://github.com/stefanoCassola/fiberfoam/blob/main/models/models.yml)).

---

## fiberFoamRun

Orchestrates the full pipeline: mesh generation, optional prediction, solver execution, and post-processing. Reads all configuration from a YAML file.

### Usage

```bash
fiberFoamRun -config <config.yml> [options]
```

### Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-config` | string | (required) | Path to YAML configuration file |
| `-dryRun` | flag | off | Validate configuration without executing |
| `-skipMesh` | flag | off | Skip mesh generation (use existing mesh) |
| `-skipPredict` | flag | off | Skip ML prediction step |
| `-skipSolve` | flag | off | Skip solver execution |
| `-skipPostProcess` | flag | off | Skip post-processing |

### Examples

Run the full pipeline:

```bash
fiberFoamRun -config config.yml
```

Run only post-processing on existing results:

```bash
fiberFoamRun -config config.yml -skipMesh -skipPredict -skipSolve
```

---

## fiberFoamPostProcess

Reads completed simulation results and computes permeability values, fiber volume content, and convergence data.

### Usage

```bash
fiberFoamPostProcess [options]
```

### Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-case` | string | (required) | Path to the OpenFOAM case directory |
| `-method` | string | `both` | Permeability method: `volumeAveraged`, `flowRate`, or `both` |
| `-fibrousRegionOnly` | flag | off | Restrict calculation to the fibrous region (exclude buffer zones) |
| `-config` | string | -- | Load options from a YAML configuration file |

### Examples

Compute permeability using both methods on the fibrous region only:

```bash
fiberFoamPostProcess \
    -case ./case/x_dir \
    -method both \
    -fibrousRegionOnly
```

### Output

Results are written to `permeabilityInfo.csv` in the case directory. The CSV columns are documented in the [Quick Start](quickstart.md#step-6-review-results).

---

## fiberFoamInfo

Prints information about a geometry file: dimensions, solid/fluid voxel counts, solid fraction, and estimated fiber volume content.

### Usage

```bash
fiberFoamInfo [options]
```

### Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-input` | string | (required) | Path to the geometry file |
| `-voxelRes` | int | `320` | Voxel resolution (number of voxels per axis) |
| `-voxelSize` | float | `0.5e-6` | Physical size of one voxel edge [m] |
| `--version` | flag | -- | Print version information and exit |

### Examples

```bash
fiberFoamInfo -input geometry.dat -voxelRes 320
```

Sample output:

```
FiberFoam v0.1.0
Geometry file: geometry.dat
Voxel resolution: 320 x 320 x 320
Voxel size: 5e-07 m
Domain size: 1.6e-04 x 1.6e-04 x 1.6e-04 m
Total voxels: 32768000
Solid voxels: 11010048
Fluid voxels: 21757952
Solid fraction: 33.60%
```

---

## fiberFoamConvertModel

Converts or validates ONNX model files for use with FiberFoam.

!!! note
    This tool is only available when built with `ENABLE_ONNX=ON`.

### Usage

```bash
fiberFoamConvertModel [options]
```

### Options

| Flag | Type | Default | Description |
|---|---|---|---|
| `-input` | string | (required) | Path to the input model file |
| `-output` | string | (required) | Path for the output ONNX model |
| `-resolution` | int | `80` | Target model resolution |
| `-validate` | flag | off | Validate the model without converting |

### Examples

Validate an existing model:

```bash
fiberFoamConvertModel -input models/res80/x_80.onnx -validate
```

---

## simpleFoamMod

The modified OpenFOAM SIMPLE solver. This is an OpenFOAM application compiled with `wmake`, not a FiberFoam CMake target.

### Usage

```bash
simpleFoamMod -case <caseDir> [OpenFOAM options]
```

This solver inherits all standard OpenFOAM solver options (e.g., `-parallel`, `-postProcess`). It extends `simpleFoam` with:

- **Per-iteration permeability calculation** using both volume-averaged and flow-rate methods
- **Convergence monitoring** based on permeability slope (linear regression over a sliding window)
- **Region-of-interest filtering** to compute permeability only within the fibrous region
- **CSV output** of permeability history for post-processing

### Examples

Run the solver on a single case:

```bash
source /usr/lib/openfoam/openfoam2312/etc/bashrc
simpleFoamMod -case ./case/x_dir
```

Run in parallel with decomposed mesh:

```bash
mpirun -np 4 simpleFoamMod -case ./case/x_dir -parallel
```
