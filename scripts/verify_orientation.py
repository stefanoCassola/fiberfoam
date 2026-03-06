#!/usr/bin/env python3
"""
Generate a reference VTK file from the Python mesh convention
and compare with the C++ OpenFOAM output.

This produces a VTK file that can be opened in ParaView to visually
verify the geometry orientation and velocity field.
"""
import sys
import numpy as np
import onnxruntime as ort


def load_geometry(path, resolution):
    """Load and invert geometry, matching the Python convention."""
    raw = np.loadtxt(path)
    raw[raw == 0] = 3
    raw[raw == 1] = 0
    raw[raw == 3] = 1
    return raw.reshape(resolution, resolution, -1).astype(np.float32)


def write_vtk_geometry(geom, voxel_size, filename):
    """Write geometry as VTK structured points (for ParaView)."""
    nx, ny, nz = geom.shape
    with open(filename, 'w') as f:
        f.write("# vtk DataFile Version 3.0\n")
        f.write("FiberFoam geometry\n")
        f.write("ASCII\n")
        f.write("DATASET STRUCTURED_POINTS\n")
        f.write(f"DIMENSIONS {nx} {ny} {nz}\n")
        f.write(f"ORIGIN {voxel_size/2} {voxel_size/2} {voxel_size/2}\n")
        f.write(f"SPACING {voxel_size} {voxel_size} {voxel_size}\n")
        f.write(f"POINT_DATA {nx*ny*nz}\n")
        f.write("SCALARS geometry float 1\n")
        f.write("LOOKUP_TABLE default\n")
        # VTK structured points uses x-fastest ordering
        for z in range(nz):
            for y in range(ny):
                for x in range(nx):
                    f.write(f"{geom[x,y,z]:.0f}\n")


def write_vtk_velocity(geom, vel_field, voxel_size, direction, filename):
    """Write velocity field as VTK structured points."""
    nx, ny, nz = geom.shape
    with open(filename, 'w') as f:
        f.write("# vtk DataFile Version 3.0\n")
        f.write(f"FiberFoam velocity {direction}\n")
        f.write("ASCII\n")
        f.write("DATASET STRUCTURED_POINTS\n")
        f.write(f"DIMENSIONS {nx} {ny} {nz}\n")
        f.write(f"ORIGIN {voxel_size/2} {voxel_size/2} {voxel_size/2}\n")
        f.write(f"SPACING {voxel_size} {voxel_size} {voxel_size}\n")
        f.write(f"POINT_DATA {nx*ny*nz}\n")
        f.write(f"SCALARS velocity_{direction} float 1\n")
        f.write("LOOKUP_TABLE default\n")
        # VTK structured points uses x-fastest ordering
        for z in range(nz):
            for y in range(ny):
                for x in range(nx):
                    f.write(f"{vel_field[x,y,z]:.8e}\n")


def main():
    geom_file = "/media/cassola/SSD_860_2TB/FiberGeo_fvc-50_RS-10_fdia-6_fdir0_ds_res80.dat"
    resolution = 80
    voxel_size = 0.5e-6

    ONNX_MODELS = {
        "x": "/media/cassola/SSD_860_2TB/fiberFoam/models/res80/x_80.onnx",
        "y": "/media/cassola/SSD_860_2TB/fiberFoam/models/res80/y_80.onnx",
        "z": "/media/cassola/SSD_860_2TB/fiberFoam/models/res80/z_80.onnx",
    }

    SCALING = {
        "x": 1.0555797888840423e-05,
        "y": 2.8334309550529396e-06,
        "z": 1.5022073497650519e-06,
    }

    print("Loading geometry...")
    geom = load_geometry(geom_file, resolution)
    nx, ny, nz = geom.shape

    # Verify fiber orientation
    print(f"\nGeometry shape: {geom.shape}")
    print(f"Values: 0={np.mean(geom==0)*100:.1f}%, 1={np.mean(geom==1)*100:.1f}%, 2={np.mean(geom==2)*100:.1f}%")

    for ax_name, ax in [("x (axis 0)", 0), ("y (axis 1)", 1), ("z (axis 2)", 2)]:
        cells_per_slice = np.array([np.sum(np.take(geom, i, axis=ax) != 0)
                                     for i in range(geom.shape[ax])])
        print(f"  Fluid cells per {ax_name}-slice: std={cells_per_slice.std():.1f}")
    print("  (Fibers along axis with LOWEST std)")

    # Write geometry VTK
    outdir = "/media/cassola/SSD_860_2TB/fiberFoam/test_predict_80_fixed"
    vtk_geom = f"{outdir}/python_geometry.vtk"
    write_vtk_geometry(geom, voxel_size, vtk_geom)
    print(f"\nWrote geometry VTK: {vtk_geom}")

    # Run ONNX predictions and write velocity VTK
    for direction in ["x", "y", "z"]:
        print(f"\nPredicting direction {direction}...")
        session = ort.InferenceSession(ONNX_MODELS[direction])
        input_name = session.get_inputs()[0].name
        input_tensor = geom.reshape(1, nx, ny, nz, 1)
        result = session.run(None, {input_name: input_tensor})[0].squeeze()
        result_scaled = result * SCALING[direction]

        # Zero out solid cells
        result_scaled[geom == 0] = 0.0

        vtk_vel = f"{outdir}/python_velocity_{direction}.vtk"
        write_vtk_velocity(geom, result_scaled, voxel_size, direction, vtk_vel)
        print(f"  Wrote velocity VTK: {vtk_vel}")

        # Stats
        fluid_vel = result_scaled[geom != 0]
        print(f"  Max velocity: {result_scaled.max():.6e}")
        print(f"  Mean (fluid): {fluid_vel.mean():.6e}")
        print(f"  Non-zero: {np.sum(np.abs(fluid_vel) > 1e-15)}/{fluid_vel.size}")

    print("\n=== INSTRUCTIONS ===")
    print("Open these files in ParaView to verify:")
    print(f"  1. {vtk_geom} - geometry (threshold value > 0.5 to see fluid)")
    print(f"  2. {outdir}/python_velocity_x.vtk - x-velocity from Python ONNX")
    print(f"  3. {outdir}/x_dir/ - C++ OpenFOAM case")
    print("\nBoth should show fibers (value=0 = solid) running along the X-axis.")
    print("Compare velocity fields to verify C++ matches Python.")


if __name__ == "__main__":
    main()
