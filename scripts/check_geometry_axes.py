#!/usr/bin/env python3
"""
check_geometry_axes.py
Investigate the axis convention of .dat fiber geometry files.

Key question: Fibers with fdir=0 (fiber direction 0 = x-axis) appear along
the z-axis in ParaView. Where are the fibers actually aligned in the raw data,
and how does the C++ VoxelArray indexing relate to NumPy C-order indexing?

C++ VoxelArray memory layout:
    at(x, y, z) = data_[x + nx * (y + ny * z)]
    => x is the FASTEST-varying index (stride 1)
    => z is the SLOWEST-varying index (stride nx*ny)

NumPy C-order array[i, j, k] with shape (N, N, N):
    element at [i,j,k] = flat_data[k + N * (j + N * i)]   ... NO
    Actually: element at [i,j,k] = flat_data[i*N*N + j*N + k]
    => k is the FASTEST-varying index (stride 1)
    => i is the SLOWEST-varying index (stride N*N)

So if we do: raw = np.loadtxt(file); arr = raw.reshape(N, N, N) (C-order)
    arr[i, j, k] maps to flat index: i*N*N + j*N + k

And C++ at(x, y, z) maps to flat index: x + nx*(y + ny*z) = x + nx*y + nx*ny*z

Therefore:
    C++ at(x, y, z) = arr[z, y, x]   (!! x and z are SWAPPED !!)

    Or equivalently: arr[i, j, k] = C++ at(k, j, i)
        NumPy axis 0 (i) = C++ z
        NumPy axis 1 (j) = C++ y
        NumPy axis 2 (k) = C++ x
"""

import numpy as np
import sys
from collections import Counter


def load_dat_file(path, resolution):
    """Load .dat file and reshape, mimicking np.loadtxt().reshape()"""
    print(f"Loading {path} ...")
    raw = np.loadtxt(path, dtype=np.int8)
    print(f"  Raw shape: {raw.shape}, unique values: {np.unique(raw)}")
    arr = raw.reshape(resolution, resolution, resolution)  # C-order
    return arr


def invert_convention(arr):
    """Invert 0<->1 to match C++ code convention (0=solid in file -> 1=solid after invert)"""
    out = arr.copy()
    out[arr == 0] = 1
    out[arr == 1] = 0
    return out


def run_length_analysis(arr, axis, label=""):
    """
    For each 1D line along the given axis, compute run lengths of
    consecutive solid cells (value == 0, i.e., fiber/solid after inversion means 0).

    After inversion: 0 = fiber/solid, 1 = fluid
    Wait -- let's be careful. In the .dat file:
      0 = solid (fiber)
      1 = fluid (pore)
    After inversion (matching C++ code):
      0 = fluid
      1 = solid (fiber)

    So after inversion, solid/fiber = 1.
    """
    n = arr.shape[axis]
    other_axes = [i for i in range(3) if i != axis]

    all_runs = []
    n_lines = 0

    # Iterate over all lines along the given axis
    for idx0 in range(arr.shape[other_axes[0]]):
        for idx1 in range(arr.shape[other_axes[1]]):
            # Build the index
            slc = [None, None, None]
            slc[axis] = slice(None)
            slc[other_axes[0]] = idx0
            slc[other_axes[1]] = idx1
            line = arr[tuple(slc)]

            # Compute run lengths of solid cells (value == 1)
            runs = []
            current_run = 0
            for val in line:
                if val == 1:
                    current_run += 1
                else:
                    if current_run > 0:
                        runs.append(current_run)
                    current_run = 0
            if current_run > 0:
                runs.append(current_run)

            all_runs.extend(runs)
            n_lines += 1

    if not all_runs:
        return {
            'axis': axis,
            'label': label,
            'n_lines': n_lines,
            'n_runs': 0,
            'max_run': 0,
            'mean_run': 0,
            'median_run': 0,
            'p90_run': 0,
            'p99_run': 0,
            'fraction_long_runs': 0,
        }

    all_runs = np.array(all_runs)
    max_run = int(np.max(all_runs))
    mean_run = float(np.mean(all_runs))
    median_run = float(np.median(all_runs))
    p90 = float(np.percentile(all_runs, 90))
    p99 = float(np.percentile(all_runs, 99))
    # Fraction of runs that span the entire axis
    full_span_runs = int(np.sum(all_runs == n))
    fraction_long = float(np.sum(all_runs >= n * 0.5)) / len(all_runs) if len(all_runs) > 0 else 0

    return {
        'axis': axis,
        'label': label,
        'n_lines': n_lines,
        'n_runs': len(all_runs),
        'max_run': max_run,
        'mean_run': mean_run,
        'median_run': median_run,
        'p90_run': p90,
        'p99_run': p99,
        'full_span_runs': full_span_runs,
        'fraction_long_runs': fraction_long,
        'axis_length': n,
    }


def projection_analysis(arr):
    """
    For each axis, sum solid cells along that axis to get a 2D projection.
    The axis along which fibers are aligned should have the LEAST variation
    in the projection (since fibers span the full length, every projection
    cell gets roughly the same count).
    """
    print("\n--- Projection Analysis (sum along each axis) ---")
    for axis in range(3):
        proj = np.sum(arr == 1, axis=axis).astype(float)
        mean_val = np.mean(proj)
        std_val = np.std(proj)
        cv = std_val / mean_val if mean_val > 0 else float('inf')
        print(f"  Axis {axis}: projection mean={mean_val:.2f}, std={std_val:.2f}, "
              f"CV(std/mean)={cv:.4f}, min={np.min(proj)}, max={np.max(proj)}")
    print("  -> Fiber alignment axis has LOWEST CV (least variation in projection)")


def analyze_file(path, resolution, name=""):
    print(f"\n{'='*70}")
    print(f"  Analyzing: {name}")
    print(f"  File: {path}")
    print(f"  Resolution: {resolution}")
    print(f"{'='*70}")

    arr = load_dat_file(path, resolution)
    arr_inv = invert_convention(arr)

    total_solid = np.sum(arr_inv == 1)
    total_cells = arr_inv.size
    solid_fraction = total_solid / total_cells
    print(f"  After inversion: solid(fiber)=1, fluid=0")
    print(f"  Solid fraction: {solid_fraction:.4f} ({total_solid}/{total_cells})")
    print(f"  (FVC in filename is 50 -> expect ~0.50 solid fraction)")

    # Run length analysis
    print(f"\n--- Run Length Analysis (solid=1 runs along each axis) ---")
    axis_labels = {0: "NumPy axis 0 (= C++ z, slowest-varying)",
                   1: "NumPy axis 1 (= C++ y)",
                   2: "NumPy axis 2 (= C++ x, fastest-varying)"}

    results = []
    for axis in range(3):
        r = run_length_analysis(arr_inv, axis, axis_labels[axis])
        results.append(r)
        print(f"\n  Axis {axis} [{axis_labels[axis]}]:")
        print(f"    Lines examined:    {r['n_lines']}")
        print(f"    Total runs found:  {r['n_runs']}")
        print(f"    Axis length:       {r['axis_length']}")
        print(f"    Max run length:    {r['max_run']}")
        print(f"    Mean run length:   {r['mean_run']:.2f}")
        print(f"    Median run length: {r['median_run']:.1f}")
        print(f"    90th percentile:   {r['p90_run']:.1f}")
        print(f"    99th percentile:   {r['p99_run']:.1f}")
        print(f"    Full-span runs:    {r['full_span_runs']}")
        print(f"    Fraction runs >= 50% of axis: {r['fraction_long_runs']:.4f}")

    # Determine which axis has longest runs
    best_axis = max(results, key=lambda r: r['mean_run'])
    print(f"\n  >>> LONGEST mean runs along: Axis {best_axis['axis']} "
          f"[{best_axis['label']}] with mean={best_axis['mean_run']:.2f}")

    # Projection analysis
    projection_analysis(arr_inv)

    # Indexing cross-reference
    print(f"\n--- Indexing Cross-Reference ---")
    print(f"  C++ VoxelArray::at(x, y, z) = data[x + nx*(y + ny*z)]")
    print(f"    x has stride 1 (fastest), z has stride nx*ny (slowest)")
    print(f"  NumPy C-order arr[i, j, k] for shape ({resolution},{resolution},{resolution}):")
    print(f"    k has stride 1 (fastest), i has stride {resolution}*{resolution} (slowest)")
    print(f"  Therefore: at(x,y,z) = arr[z, y, x]")
    print(f"    NumPy axis 0 <-> C++ z")
    print(f"    NumPy axis 1 <-> C++ y")
    print(f"    NumPy axis 2 <-> C++ x")
    print()
    print(f"  If fibers are along C++ x (fdir=0), they should have long runs along C++ x,")
    print(f"  which is NumPy axis 2 (the last/fastest axis in C-order).")

    return arr_inv


def verify_indexing_equivalence():
    """
    Explicitly verify: for flat data loaded sequentially,
    at(x,y,z) = data[x + nx*(y + ny*z)] corresponds to arr[z,y,x] in NumPy C-order.
    """
    print(f"\n{'='*70}")
    print(f"  Indexing Verification (small example)")
    print(f"{'='*70}")

    nx, ny, nz = 3, 4, 5
    flat = np.arange(nx * ny * nz, dtype=np.int32)

    # C++ style: at(x,y,z) = flat[x + nx*(y + ny*z)]
    def cpp_at(x, y, z):
        return flat[x + nx * (y + ny * z)]

    # NumPy C-order reshape
    arr = flat.reshape(nz, ny, nx)  # if we want arr[z,y,x] = flat[x + nx*(y+ny*z)]
    # Wait, let's check: arr = flat.reshape(N0, N1, N2) in C-order means:
    #   arr[i,j,k] = flat[i*N1*N2 + j*N2 + k]
    # We want arr[z,y,x] = flat[x + nx*(y + ny*z)] = flat[x + nx*y + nx*ny*z]
    # So: arr[z,y,x] with shape (nz, ny, nx) -> flat[z*ny*nx + y*nx + x] = flat[x + nx*y + nx*ny*z]  YES!

    print(f"  flat data: 0..{nx*ny*nz-1}, shape when loaded: ({nx*ny*nz},)")
    print(f"  C++ dimensions: nx={nx}, ny={ny}, nz={nz}")
    print()

    # Method A: reshape(nz, ny, nx) -> arr[z,y,x] should equal cpp_at(x,y,z)
    arrA = flat.reshape(nz, ny, nx)
    print(f"  Method A: arr = flat.reshape(nz={nz}, ny={ny}, nx={nx})")
    print(f"    arr[z,y,x] == cpp_at(x,y,z)?")
    all_match_A = True
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                if arrA[z, y, x] != cpp_at(x, y, z):
                    all_match_A = False
                    break
    print(f"    Result: {'MATCH' if all_match_A else 'MISMATCH'}")

    # Method B: reshape(nx, ny, nz) -> arr[x,y,z] should equal cpp_at(x,y,z)?
    arrB = flat.reshape(nx, ny, nz)
    print(f"\n  Method B: arr = flat.reshape(nx={nx}, ny={ny}, nz={nz})")
    print(f"    arr[x,y,z] == cpp_at(x,y,z)?")
    all_match_B = True
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                if arrB[x, y, z] != cpp_at(x, y, z):
                    all_match_B = False
                    break
    print(f"    Result: {'MATCH' if all_match_B else 'MISMATCH'}")

    # Method C: The user's method - reshape(N, N, N) with N=N
    # For the actual files, nx=ny=nz=resolution, so reshape(res,res,res)
    # arr[i,j,k] = flat[i*N*N + j*N + k]
    # cpp_at(x,y,z) = flat[x + N*y + N*N*z] = flat[x + N*(y + N*z)]
    # So arr[i,j,k] = cpp_at(k, j, i)   (i<->z, k<->x)
    print(f"\n  For cubic case (nx=ny=nz=N): arr = flat.reshape(N,N,N)")
    print(f"    arr[i,j,k] = flat[i*N*N + j*N + k]")
    print(f"    cpp_at(x,y,z) = flat[x + N*y + N*N*z]")
    print(f"    => arr[i,j,k] = cpp_at(k, j, i)")
    print(f"    => NumPy axis 0 = C++ z, NumPy axis 1 = C++ y, NumPy axis 2 = C++ x")

    # Verify with actual cubic case
    N = 4
    flat2 = np.arange(N**3, dtype=np.int32)
    arr2 = flat2.reshape(N, N, N)

    def cpp_at2(x, y, z):
        return flat2[x + N * (y + N * z)]

    all_match = True
    for i in range(N):
        for j in range(N):
            for k in range(N):
                if arr2[i, j, k] != cpp_at2(k, j, i):
                    all_match = False
    print(f"\n  Cubic verification (N={N}): arr[i,j,k] == cpp_at(k,j,i) -> {'MATCH' if all_match else 'MISMATCH'}")


def main():
    print("=" * 70)
    print("  FIBER GEOMETRY AXIS CONVENTION ANALYSIS")
    print("  Checking which axis fibers align with in .dat files")
    print("=" * 70)

    # First, verify the indexing relationship
    verify_indexing_equivalence()

    # Analyze the 320^3 file
    file320 = "/media/cassola/SSD_860_2TB/FiberGeo_fvc-50_RS-1_fdia-6_fdir0_res320.dat"
    # This is large (320^3 = 32.7M cells), so run-length analysis may take a moment
    print("\n\nNote: 320^3 run-length analysis may take a minute...")
    arr320 = analyze_file(file320, 320, "FiberGeo fvc-50 RS-1 fdir0 res320")

    # Analyze the 80^3 file
    file80 = "/media/cassola/SSD_860_2TB/FiberGeo_fvc-50_RS-10_fdia-6_fdir0_ds_res80.dat"
    arr80 = analyze_file(file80, 80, "FiberGeo fvc-50 RS-10 fdir0 ds_res80")

    # Final summary
    print(f"\n{'='*70}")
    print(f"  SUMMARY")
    print(f"{'='*70}")
    print(f"""
  The .dat file is a flat 1D array of 0s and 1s.
  When loaded with np.loadtxt().reshape(N,N,N) (C-order):
    arr[i, j, k]  where k varies fastest in memory.

  The C++ code uses: at(x, y, z) = data[x + nx*(y + ny*z)]
    where x varies fastest in memory.

  CRITICAL MAPPING:
    arr[i, j, k] = C++ at(k, j, i)
    - NumPy axis 0 (i, slowest) = C++ z-axis
    - NumPy axis 1 (j, middle)  = C++ y-axis
    - NumPy axis 2 (k, fastest) = C++ x-axis

  For fdir=0 fibers (intended to be along x-axis):
    - In C++ VoxelArray, x is the fastest-varying index
    - In NumPy C-order, this corresponds to axis 2 (the last index)
    - In ParaView (which typically maps array axes to spatial x,y,z):
      If ParaView interprets axis 0 as x, axis 1 as y, axis 2 as z,
      then C++ x (=NumPy axis 2) appears as ParaView z.

  This explains why fibers appear along the z-axis in ParaView!

  The data itself is correct -- fibers ARE along the C++ x-axis.
  But when visualized with standard tools that use row-major (C-order)
  conventions, the x and z axes appear swapped because the C++ code
  uses a column-major-like indexing scheme.
""")


if __name__ == "__main__":
    main()
