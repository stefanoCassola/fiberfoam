# Quick Start

This guide walks through a complete permeability calculation: from a voxelized geometry file to final permeability results.

---

## Prerequisites

- FiberFoam installed (see [Installation](installation.md))
- OpenFOAM v2312 environment sourced
- A geometry file (binary voxel array, typically `.dat` format)

---

## Step 1: Inspect the Geometry

Use `fiberFoamInfo` to check the geometry file before processing:

```bash
fiberFoamInfo -input geometry.dat -voxelRes 320
```

This prints the voxel dimensions, solid fraction, and basic statistics about the microstructure.

---

## Step 2: Generate the OpenFOAM Mesh

Convert the voxel geometry into an OpenFOAM hexahedral mesh. To generate meshes for all three flow directions with 10-layer inlet/outlet buffer zones:

```bash
fiberFoamMesh \
    -input geometry.dat \
    -output ./case \
    -voxelSize 0.5e-6 \
    -voxelRes 320 \
    -flowDirection all \
    -inletLayers 10 \
    -outletLayers 10 \
    -connectivity
```

This creates three case directories:

```
case/
  x_dir/     # Flow in X direction
  y_dir/     # Flow in Y direction
  z_dir/     # Flow in Z direction
```

Each directory contains a complete OpenFOAM case with `constant/polyMesh/`, `0/`, and `system/` directories.

---

## Step 3: (Optional) Predict Initial Velocity Fields

If you have trained ONNX models, use ML prediction to generate initial conditions that accelerate solver convergence:

```bash
fiberFoamPredict \
    -input geometry.dat \
    -output ./case \
    -voxelRes 320 \
    -modelRes 80 \
    -modelsDir ./models
```

The predicted velocity fields are written into each case's `0/U` file as initial conditions.

---

## Step 4: Run the CFD Solver

Run the modified SIMPLE solver for each flow direction:

```bash
# Make sure OpenFOAM is sourced
source /usr/lib/openfoam/openfoam2312/etc/bashrc

for dir in x y z; do
    echo "Solving ${dir} direction..."
    simpleFoamMod -case ./case/${dir}_dir
done
```

The solver computes permeability at every time step and monitors convergence automatically. It writes:

- Velocity and pressure fields at the configured write interval
- Permeability values (volume-averaged and flow-rate methods) to the log
- Convergence data for post-processing

---

## Step 5: Post-Process Results

Extract permeability values from the simulation results:

```bash
for dir in x y z; do
    fiberFoamPostProcess \
        -case ./case/${dir}_dir \
        -method both \
        -fibrousRegionOnly
done
```

The `both` method computes permeability using both the volume-averaged velocity method and the flow-rate (outlet flux) method. The `-fibrousRegionOnly` flag restricts the calculation to the fibrous region, excluding the buffer zones.

Results are written to `permeabilityInfo.csv` in each case directory.

---

## Step 6: Review Results

Check the output CSV files:

```bash
cat ./case/x_dir/permeabilityInfo.csv
cat ./case/y_dir/permeabilityInfo.csv
cat ./case/z_dir/permeabilityInfo.csv
```

Each file contains:

| Column | Description |
|---|---|
| `direction` | Flow direction (x, y, or z) |
| `K_volAvg_main` | Permeability from volume-averaged velocity (main direction) [m^2] |
| `K_volAvg_secondary` | Permeability from volume-averaged velocity (secondary direction) [m^2] |
| `K_volAvg_tertiary` | Permeability from volume-averaged velocity (tertiary direction) [m^2] |
| `K_flowRate` | Permeability from outlet flow rate [m^2] |
| `FVC` | Fiber volume content [%] |
| `flowLength` | Length of the fibrous region [m] |
| `crossSectionArea` | Cross-sectional area [m^2] |
| `iterations` | Number of solver iterations to convergence |

---

## Using a YAML Configuration File

Instead of passing all options as command-line flags, you can use a YAML configuration file:

```bash
fiberFoamRun -config config.yml
```

See [examples/config_example.yml](https://github.com/stefanoCassola/fiberfoam/blob/main/examples/config_example.yml) for a complete configuration example that drives the entire pipeline from mesh generation through post-processing.

---

## Next Steps

- Read the [CLI Reference](cli_reference.md) for complete documentation of all tools and flags
- Explore the [GUI Guide](gui_guide.md) for the web-based workflow
- Understand the [permeability theory](theory/permeability.md) behind the calculations
- Learn about [fiber-free buffer regions](theory/fiber_free_regions.md) and why they matter
