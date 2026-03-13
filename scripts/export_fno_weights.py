#!/usr/bin/env python3
"""
Export FNO TensorFlow SavedModel weights to .npz format.

Usage:
    python scripts/export_fno_weights.py

Reads TF SavedModels from models/res80_fno/{x,y,z}_80_tf/
Writes numpy archives to models/res80_fno/{x,y,z}_80_fno.npz
"""
import os
import sys
import numpy as np

os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"


def export_model(tf_dir, npz_path):
    import tensorflow as tf

    tf.get_logger().setLevel("ERROR")
    model = tf.saved_model.load(tf_dir)

    weights = {}
    name_counts = {}
    for var in model.variables:
        # Strip model-name prefix and :0 suffix
        parts = var.name.split("/")
        short = "/".join(parts[1:])
        if short.endswith(":0"):
            short = short[:-2]

        # Handle duplicate names (e.g. mlp3d is shared between block 0
        # skip and output projection — same scope, different weights)
        if short in weights:
            count = name_counts.get(short, 1)
            name_counts[short] = count + 1
            short = f"{short}__dup{count}"

        weights[short] = var.numpy()

    np.savez_compressed(npz_path, **weights)
    size_mb = os.path.getsize(npz_path) / 1024 / 1024
    print(f"  {npz_path}: {len(weights)} arrays, {size_mb:.1f} MB")


def main():
    base = "models/res80_fno"
    for direction in ["x", "y", "z"]:
        tf_dir = os.path.join(base, f"{direction}_80_tf")
        npz_path = os.path.join(base, f"{direction}_80_fno.npz")
        if not os.path.isdir(tf_dir):
            print(f"  SKIP {tf_dir} (not found)")
            continue
        print(f"Exporting {tf_dir} ...")
        export_model(tf_dir, npz_path)

    print("Done.")


if __name__ == "__main__":
    main()
