#!/usr/bin/env python3
"""
Compare velocity predictions across three sources:
  1. TensorFlow SavedModel (reference)
  2. ONNX Runtime (should match TF exactly)
  3. C++ FiberFoam output (OpenFOAM 0/U file)

Generates comparison statistics and optional VTK files for ParaView.

Usage:
    python visualize_predictions.py \
        --geom <geometry.dat> \
        --resolution 80 \
        --direction x \
        --tf-model <path_to_saved_model> \
        --onnx-model <path_to_model.onnx> \
        --cpp-case <path_to_openfoam_case> \
        [--scaling-factor 0.000011] \
        [--vtk-output <output_dir>]
"""
import argparse
import os
import re
import sys

import numpy as np


def load_geometry(path, resolution):
    """Load and invert geometry, matching the Python training convention."""
    raw = np.loadtxt(path).reshape(resolution, resolution, resolution)
    geom = raw.copy()
    geom[raw == 0] = 1
    geom[raw == 1] = 0
    return geom.astype(np.float32)


def predict_tf(geom, model_path):
    """Run prediction with TensorFlow SavedModel."""
    import tensorflow as tf
    tf.get_logger().setLevel("ERROR")
    model = tf.saved_model.load(model_path)
    if hasattr(model, "signatures"):
        infer = model.signatures.get("serving_default")
        if infer is None:
            infer = list(model.signatures.values())[0]
    else:
        infer = model.__call__

    input_tensor = tf.constant(geom.reshape(1, *geom.shape, 1))
    try:
        result = infer(input_tensor)
        if isinstance(result, dict):
            key = list(result.keys())[0]
            output = result[key].numpy()
        else:
            output = result.numpy()
    except TypeError:
        sig = model.signatures.get("serving_default")
        input_name = list(sig.structured_input_signature[1].keys())[0]
        result = sig(**{input_name: input_tensor})
        key = list(result.keys())[0]
        output = result[key].numpy()

    return output.squeeze()


def predict_onnx(geom, model_path):
    """Run prediction with ONNX Runtime."""
    import onnxruntime as ort
    session = ort.InferenceSession(model_path)
    input_name = session.get_inputs()[0].name
    input_tensor = geom.reshape(1, *geom.shape, 1)
    result = session.run(None, {input_name: input_tensor})
    return result[0].squeeze()


def load_openfoam_velocity(case_dir, direction):
    """Load velocity field from OpenFOAM 0/U file.

    Returns a flat array of the velocity component in the flow direction,
    ordered by cell index.
    """
    u_path = os.path.join(case_dir, "0", "U")
    if not os.path.exists(u_path):
        raise FileNotFoundError(f"Velocity file not found: {u_path}")

    with open(u_path) as f:
        content = f.read()

    # Find internalField data
    if "nonuniform" in content:
        # Extract the vector list
        match = re.search(
            r"internalField\s+nonuniform\s+List<vector>\s*\n(\d+)\s*\n\(\n(.*?)\n\)\s*;",
            content,
            re.DOTALL,
        )
        if not match:
            raise ValueError("Could not parse nonuniform velocity field")

        n_cells = int(match.group(1))
        lines = match.group(2).strip().split("\n")
        velocities = []
        for line in lines:
            line = line.strip().strip("()")
            parts = line.split()
            u, v, w = float(parts[0]), float(parts[1]), float(parts[2])
            velocities.append((u, v, w))

        component_idx = {"x": 0, "y": 1, "z": 2}[direction]
        return np.array([vel[component_idx] for vel in velocities])
    else:
        # uniform field
        match = re.search(r"internalField\s+uniform\s+\(([^)]+)\)", content)
        if match:
            parts = match.group(1).split()
            component_idx = {"x": 0, "y": 1, "z": 2}[direction]
            val = float(parts[component_idx])
            # Count cells from owner file
            owner_path = os.path.join(case_dir, "constant", "polyMesh", "owner")
            with open(owner_path) as f:
                owner_content = f.read()
            n_cells_match = re.search(r"nCells:(\d+)", owner_content)
            n_cells = int(n_cells_match.group(1))
            return np.full(n_cells, val)

    raise ValueError("Could not parse velocity field")


def write_vtk_structured(data, resolution, filename, field_name="velocity"):
    """Write a 3D array as VTK structured points for ParaView."""
    with open(filename, "w") as f:
        f.write("# vtk DataFile Version 3.0\n")
        f.write(f"{field_name}\n")
        f.write("ASCII\n")
        f.write("DATASET STRUCTURED_POINTS\n")
        f.write(f"DIMENSIONS {resolution} {resolution} {resolution}\n")
        f.write("ORIGIN 0 0 0\n")
        f.write("SPACING 1 1 1\n")
        f.write(f"POINT_DATA {resolution**3}\n")
        f.write(f"SCALARS {field_name} float 1\n")
        f.write("LOOKUP_TABLE default\n")
        for val in data.flatten():
            f.write(f"{val:.6e}\n")


def compare_arrays(name_a, arr_a, name_b, arr_b, geom=None):
    """Compare two prediction arrays and print statistics."""
    print(f"\n  --- {name_a} vs {name_b} ---")

    if arr_a.shape != arr_b.shape:
        print(f"  Shape mismatch: {arr_a.shape} vs {arr_b.shape}")
        return False

    diff = np.abs(arr_a - arr_b)
    print(f"  {name_a}: min={arr_a.min():.6e}, max={arr_a.max():.6e}, "
          f"mean={arr_a.mean():.6e}")
    print(f"  {name_b}: min={arr_b.min():.6e}, max={arr_b.max():.6e}, "
          f"mean={arr_b.mean():.6e}")
    print(f"  Abs diff: max={diff.max():.6e}, mean={diff.mean():.6e}")

    nonzero_a = np.count_nonzero(np.abs(arr_a) > 1e-10)
    if nonzero_a > 0:
        rel_diff = np.where(np.abs(arr_a) > 1e-10, diff / np.abs(arr_a), 0)
        print(f"  Rel diff: max={rel_diff.max():.6e}, mean={rel_diff.mean():.6e}")
        corr = np.corrcoef(arr_a.flatten(), arr_b.flatten())[0, 1]
        print(f"  Correlation: {corr:.10f}")
        passed = diff.max() < 1e-4 and corr > 0.999
    else:
        passed = diff.max() < 1e-10

    # Per-slice analysis along axis 0
    n_slices = min(arr_a.shape[0] if arr_a.ndim == 3 else 5, 5)
    if arr_a.ndim == 3:
        print(f"  Per-slice max diff (first {n_slices} slices along axis 0):")
        for i in range(n_slices):
            s_diff = np.abs(arr_a[i] - arr_b[i]).max()
            print(f"    Slice {i}: {s_diff:.6e}")

    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    return passed


def main():
    parser = argparse.ArgumentParser(
        description="Compare velocity predictions: TF vs ONNX vs C++ FiberFoam"
    )
    parser.add_argument("--geom", required=True, help="Path to geometry .dat file")
    parser.add_argument("--resolution", type=int, default=80,
                        help="Geometry resolution (default: 80)")
    parser.add_argument("--direction", required=True, choices=["x", "y", "z"],
                        help="Flow direction")
    parser.add_argument("--tf-model", help="Path to TF SavedModel directory")
    parser.add_argument("--onnx-model", help="Path to ONNX model file")
    parser.add_argument("--cpp-case", help="Path to C++ FiberFoam OpenFOAM case dir")
    parser.add_argument("--scaling-factor", type=float, default=1.0,
                        help="Scaling factor to apply to predictions")
    parser.add_argument("--vtk-output", help="Directory for VTK output files")
    args = parser.parse_args()

    print(f"Loading geometry: {args.geom} ({args.resolution}^3)")
    geom = load_geometry(args.geom, args.resolution)
    fluid_frac = np.sum(geom > 0.5) / geom.size
    print(f"  Fluid fraction: {fluid_frac*100:.1f}%")

    results = {}
    all_pass = True

    # TensorFlow prediction
    if args.tf_model:
        print(f"\nRunning TF prediction: {args.tf_model}")
        try:
            tf_out = predict_tf(geom, args.tf_model)
            results["TF"] = tf_out
            print(f"  Shape: {tf_out.shape}, range: [{tf_out.min():.6e}, {tf_out.max():.6e}]")
        except Exception as e:
            print(f"  FAILED: {e}")
            all_pass = False

    # ONNX prediction
    if args.onnx_model:
        print(f"\nRunning ONNX prediction: {args.onnx_model}")
        try:
            onnx_out = predict_onnx(geom, args.onnx_model)
            results["ONNX"] = onnx_out
            print(f"  Shape: {onnx_out.shape}, range: [{onnx_out.min():.6e}, {onnx_out.max():.6e}]")
        except Exception as e:
            print(f"  FAILED: {e}")
            all_pass = False

    # C++ FiberFoam output
    if args.cpp_case:
        print(f"\nLoading C++ FiberFoam output: {args.cpp_case}")
        try:
            cpp_vel = load_openfoam_velocity(args.cpp_case, args.direction)
            results["C++"] = cpp_vel
            print(f"  Cells: {len(cpp_vel)}, range: [{cpp_vel.min():.6e}, {cpp_vel.max():.6e}]")
        except Exception as e:
            print(f"  FAILED: {e}")
            all_pass = False

    # Comparisons
    print(f"\n{'='*60}")
    print(f"Comparisons (direction: {args.direction})")
    print(f"{'='*60}")

    if "TF" in results and "ONNX" in results:
        passed = compare_arrays("TF", results["TF"], "ONNX", results["ONNX"], geom)
        if not passed:
            all_pass = False

    if "TF" in results and "C++" in results:
        # C++ output is per-cell, TF/ONNX is per-voxel.
        # For comparison, we can check overall statistics.
        tf_scaled = results["TF"] * args.scaling_factor
        print(f"\n  --- TF (scaled) vs C++ (cell values) ---")
        print(f"  Note: TF is per-voxel ({results['TF'].size} values), "
              f"C++ is per-cell ({len(results['C++'])} values)")
        print(f"  TF scaled:  range=[{tf_scaled.min():.6e}, {tf_scaled.max():.6e}], "
              f"mean={tf_scaled.mean():.6e}")
        print(f"  C++ output: range=[{results['C++'].min():.6e}, {results['C++'].max():.6e}], "
              f"mean={results['C++'].mean():.6e}")

    if "ONNX" in results and "C++" in results:
        onnx_scaled = results["ONNX"] * args.scaling_factor
        print(f"\n  --- ONNX (scaled) vs C++ (cell values) ---")
        print(f"  ONNX scaled: range=[{onnx_scaled.min():.6e}, {onnx_scaled.max():.6e}], "
              f"mean={onnx_scaled.mean():.6e}")
        print(f"  C++ output:  range=[{results['C++'].min():.6e}, {results['C++'].max():.6e}], "
              f"mean={results['C++'].mean():.6e}")

    # VTK output
    if args.vtk_output:
        os.makedirs(args.vtk_output, exist_ok=True)
        for name, data in results.items():
            if data.ndim == 3:
                vtk_path = os.path.join(args.vtk_output,
                                        f"{name}_{args.direction}.vtk")
                write_vtk_structured(data * args.scaling_factor,
                                     args.resolution, vtk_path,
                                     f"velocity_{args.direction}")
                print(f"\nWrote VTK: {vtk_path}")

    print(f"\n{'='*60}")
    print(f"Overall: {'ALL PASS' if all_pass else 'SOME FAILED'}")
    print(f"{'='*60}")

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
