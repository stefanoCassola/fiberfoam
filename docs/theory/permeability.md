# Permeability Calculation

This page describes the theory and formulas used by FiberFoam to compute permeability from CFD simulation results. The implementation is primarily in `src/solver/simpleFoamMod/permCalc.H` (runtime calculation within the solver) and `src/libfiberfoam/postprocessing/Permeability.cpp` (post-processing library).

---

## Darcy's Law

Permeability characterizes the resistance of a porous medium to fluid flow. For a Newtonian fluid flowing through a porous medium under a pressure gradient, Darcy's law states:

```
Q/A = -(K / mu) * (dP / L)
```

where:

- **Q** is the volumetric flow rate [m^3/s]
- **A** is the cross-sectional area perpendicular to the flow [m^2]
- **K** is the permeability [m^2]
- **mu** is the dynamic viscosity [Pa.s]
- **dP** is the pressure difference across the medium [Pa]
- **L** is the length of the porous medium in the flow direction [m]

Rearranging for permeability:

```
K = -(Q/A) * mu * L / dP
```

Since OpenFOAM solves for kinematic pressure p/rho and kinematic viscosity nu = mu/rho, the formulas below use nu and density explicitly.

---

## Volume-Averaged Velocity Method

This method computes permeability from the volume-averaged velocity within the region of interest (ROI).

### Formula

```
K_vol = -(U_avg * nu * density * L) / (p_out - p_in)
```

where:

- **U_avg** is the volume-averaged velocity in the main flow direction within the ROI [m/s]
- **nu** is the kinematic viscosity [m^2/s]
- **density** is the fluid density [kg/m^3]
- **L** is the length of the fibrous region in the flow direction [m]
- **p_in** is the inlet pressure [Pa]
- **p_out** is the outlet pressure [Pa]

### Implementation Details

The volume-averaged velocity is computed by summing the velocity over all cells in the ROI and dividing by the number of cells:

```
U_avg = (1/N) * sum(U_i)
```

where the sum runs over only those cells whose centers fall within the fibrous region bounding box (excluding inlet and outlet buffer zones when `fibrousRegionOnly` is enabled).

The solver also computes secondary and tertiary permeability components using the same formula with the velocity components in the other two directions. These cross-direction permeabilities quantify the off-diagonal terms of the permeability tensor.

### Corresponding Source Code

From `permCalc.H`:

```cpp
scalar permVolAvgUmain = -(avgUBox[mainFlowDir]*nu*density*flowLengthROI)/(pOut-pIn);
scalar permVolAvgUsec = -(avgUBox[secondaryFlowDir]*nu*density*flowLengthROI)/(pOut-pIn);
scalar permVolAvgUtert = -(avgUBox[tertiaryFlowDir]*nu*density*flowLengthROI)/(pOut-pIn);
```

---

## Flow-Rate Method

This method computes permeability from the volumetric flow rate through the outlet boundary.

### Formula

```
K_flow = -(Q/A * nu * density * L) / (p_out - p_in)
```

where:

- **Q** is the volumetric flow rate through the outlet boundary [m^3/s]
- **A** is the cross-sectional area of the domain perpendicular to the flow direction [m^2]
- **nu**, **density**, **L**, **p_in**, **p_out** are as defined above

### Implementation Details

The flow rate is obtained directly from the face flux field `phi` on the outlet boundary patch:

```
Q = sum(phi_outlet)
```

The cross-sectional area is computed from the bounding box of the mesh:

```
A = (max_sec - min_sec) * (max_tert - min_tert)
```

where `sec` and `tert` refer to the two axes perpendicular to the main flow direction.

### Corresponding Source Code

From `permCalc.H`:

```cpp
double permFlowRate = -((sum(phi.boundaryField()[indexOutlet])/flowCrossArea)
                        *nu*density*flowLengthROI)/(pOut-pIn);
```

---

## Comparison of Methods

| Aspect | Volume-Averaged | Flow-Rate |
|---|---|---|
| Input | Velocity field in ROI | Outlet boundary flux |
| Sensitivity | Affected by all cells in the ROI | Only depends on outlet flux |
| Off-diagonal terms | Yes (secondary, tertiary) | No (main direction only) |
| Robustness | Can be noisy if flow is not fully developed | More stable for converged flows |
| Typical use | Research, tensor characterization | Validation, engineering design |

Both methods should converge to similar values for a well-resolved, fully converged simulation. Discrepancies indicate insufficient mesh resolution, incomplete convergence, or boundary effects.

---

## Fiber Volume Content (FVC)

The fiber volume content quantifies the solid fraction of the porous medium within the computational domain.

### Formula

```
FVC = (1 - V_mesh / (L * A)) * 100%
```

where:

- **V_mesh** is the total volume of the fluid mesh (sum of all cell volumes) [m^3]
- **L** is the total flow length (including buffer zones) [m]
- **A** is the cross-sectional area [m^2]

The product `L * A` gives the total bounding box volume. Since solid voxels are not meshed (only fluid voxels become cells), the difference `L * A - V_mesh` represents the solid volume, and dividing by the total volume gives the solid fraction.

### Corresponding Source Code

From `permCalc.H`:

```cpp
meshVol = sum(mesh.V()).value();
fvc = (1 - (meshVol / (flowLength * flowCrossArea))) * 100;
```

---

## Region of Interest (ROI)

When buffer zones are present, the permeability calculation can optionally be restricted to the fibrous region of interest. The ROI is defined by the bounding box of the geometry after excluding the inlet and outlet buffer layers:

- **Main direction**: from `(min + inlet_length * scale)` to `(max - outlet_length * scale)`
- **Secondary direction**: full extent of the mesh
- **Tertiary direction**: full extent of the mesh

Only cells whose centers lie within this bounding box are included in the volume-averaged velocity calculation. The flow length `L` in the permeability formulas uses the ROI length, not the full domain length.

The flow-rate method uses the ROI length for `L` but computes the flow rate from the outlet boundary of the full domain, since the outlet flux must equal the inlet flux at steady state (mass conservation).

---

## Convergence Monitoring

The `simpleFoamMod` solver monitors permeability convergence at every iteration. It stores the permeability history and fits a linear regression over a sliding window. The simulation is considered converged when:

1. The slope of the permeability vs. iteration curve falls below the configured threshold (`convSlope`)
2. The relative change between consecutive permeability values is below the error bound (`errorBound`)

This automated convergence detection allows the solver to stop early rather than running for a fixed number of iterations.
