# Fiber-Free Regions (Buffer Zones)

This page explains the purpose, implementation, and impact of fiber-free buffer regions in FiberFoam simulations.

---

## Why Buffer Zones Are Needed

When simulating flow through a porous microstructure, the computational domain typically consists only of the fibrous material. This creates an issue at the inlet and outlet boundaries: the flow profile at the boundary is directly influenced by the fiber structure immediately adjacent to it. This leads to two problems:

1. **Entrance effects**: The velocity profile at the inlet is forced by the boundary condition (e.g., uniform pressure) and does not represent a fully developed flow through the porous medium. The first few layers of the fibrous region experience unrealistic flow patterns.

2. **Exit effects**: Similarly, the outlet boundary condition artificially constrains the flow at the exit of the fibrous region. The pressure field near the outlet does not represent the internal porous flow.

These boundary artifacts corrupt the volume-averaged velocity in the near-boundary cells and can introduce significant errors in the computed permeability.

---

## How Buffer Zones Work

FiberFoam addresses boundary effects by adding **fiber-free buffer layers** -- layers of pure fluid (no solid voxels) -- at the inlet and outlet of the flow domain. These layers act as settling chambers where the flow can develop naturally before entering the fibrous region and recover after exiting it.

### Padding Operation

The `FiberFreeRegion::pad()` function extends the voxelized geometry along the flow axis:

1. **Inlet buffer**: `inletLayers` layers of all-fluid voxels are prepended at the start of the flow axis
2. **Outlet buffer**: `outletLayers` layers of all-fluid voxels are appended at the end of the flow axis
3. The original geometry occupies the central portion of the padded domain

For example, with a 320^3 geometry, X-direction flow, and 10 inlet + 10 outlet layers:

```
Original:  320 x 320 x 320
Padded:    340 x 320 x 320
           ^^^              (10 inlet + 320 fibrous + 10 outlet along X)
```

### Region Mask

The padding operation produces a `regionMask` array of the same dimensions as the padded geometry. Each voxel is assigned one of three region labels:

| Value | Enum | Meaning |
|---|---|---|
| 0 | `CellRegion::Fibrous` | Original fibrous geometry (may be solid or fluid) |
| 1 | `CellRegion::BufferInlet` | Inlet buffer zone (always fluid) |
| 2 | `CellRegion::BufferOutlet` | Outlet buffer zone (always fluid) |

This mask is carried through the entire pipeline and used during post-processing to identify which cells belong to the region of interest.

---

## Region Tracking

The `RegionTracker` class maps each mesh cell to its region. After mesh generation, it provides:

- `regionForCell(cellIndex)` -- returns the region for any cell
- `countFibrousCells()` -- number of cells in the fibrous region
- `countBufferInletCells()` -- number of cells in the inlet buffer
- `countBufferOutletCells()` -- number of cells in the outlet buffer

This information is used by the `PermeabilityCalculator` to restrict the volume-averaged velocity computation to only the fibrous cells, and by the `simpleFoamMod` solver to define the bounding box of the region of interest.

---

## Impact on Permeability Calculation

### Volume-Averaged Method

When `fibrousRegionOnly` is enabled:

- The bounding box of the ROI excludes the buffer layers
- Only cells whose centers fall within the fibrous bounding box are included in the velocity average
- The flow length `L` in the permeability formula uses the fibrous region length (not the total domain length)

This eliminates the entrance and exit effects from the permeability calculation.

### Flow-Rate Method

The flow-rate method computes permeability from the outlet flux. Since mass is conserved at steady state, the outlet flux equals the flux through any cross-section of the domain, including at the fibrous/buffer boundary. The flow length `L` still uses the fibrous region length, ensuring consistency.

### Fiber Volume Content

FVC is computed using the full domain dimensions (including buffer zones) for the total volume. Since buffer layers contain no solid, they reduce the apparent solid fraction. If you need FVC for just the fibrous region, the region tracker cell counts can be used to compute it:

```
FVC_fibrous = (1 - N_fluid_fibrous / N_total_fibrous) * 100%
```

---

## Choosing Buffer Size

The number of buffer layers depends on the voxel size and flow conditions:

- **Typical choice**: 10 layers on each side (inlet and outlet)
- **Rule of thumb**: Buffer length should be at least 5-10% of the fibrous region length
- **Validation**: Run the same geometry with different buffer sizes and check that permeability converges

Too few layers may not fully eliminate boundary effects. Too many layers increase the mesh size and solver runtime without improving accuracy.

---

## Implementation Reference

| Component | File | Purpose |
|---|---|---|
| `FiberFreeRegion` | `src/libfiberfoam/geometry/FiberFreeRegion.h` | Pad geometry with buffer layers |
| `RegionTracker` | `src/libfiberfoam/geometry/RegionTracker.h` | Map cells to regions |
| `PermeabilityCalculator` | `src/libfiberfoam/postprocessing/Permeability.h` | ROI-filtered permeability |
| `permCalc.H` | `src/solver/simpleFoamMod/permCalc.H` | Solver-side ROI calculation |
