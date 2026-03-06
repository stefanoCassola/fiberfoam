#!/usr/bin/env python3
"""
Compare TensorFlow SavedModel predictions vs ONNX model predictions.
Ensures the ONNX conversion preserves model behavior exactly.
"""
import sys
import numpy as np

# Paths
GEOM_80 = "/media/cassola/SSD_860_2TB/FiberGeo_fvc-50_RS-10_fdia-6_fdir0_ds_res80.dat"
GEOM_320 = "/media/cassola/SSD_860_2TB/FiberGeo_fvc-50_RS-1_fdia-6_fdir0_res320.dat"

TF_MODELS = {
    "x": "/media/cassola/SSD_860_2TB/fiberFoam/Machine_learning_model/x_80_base_spectral_model_relu_more_fil_opt_2/x_80_base_spectral_model_relu_more_fil_opt_2",
    "y": "/media/cassola/SSD_860_2TB/fiberFoam/Machine_learning_model/y_80_base_spectral_model_relu_more_fil_opt_1/y_80_base_spectral_model_relu_more_fil_opt_1",
    "z": "/media/cassola/SSD_860_2TB/fiberFoam/Machine_learning_model/z_80_base_spectral_model_relu_more_fil_opt_2/z_80_base_spectral_model_relu_more_fil_opt_2",
}

ONNX_MODELS = {
    "x": "/media/cassola/SSD_860_2TB/fiberFoam/models/res80/x_80.onnx",
    "y": "/media/cassola/SSD_860_2TB/fiberFoam/models/res80/y_80.onnx",
    "z": "/media/cassola/SSD_860_2TB/fiberFoam/models/res80/z_80.onnx",
}

SCALING = {
    "x": 0.000011,
    "y": 0.000003,
    "z": 0.000002,
}


def load_geometry(path, resolution):
    """Load and invert geometry, matching the Python training convention."""
    raw = np.loadtxt(path).reshape(resolution, resolution, resolution)
    # Invert convention: 0->1, 1->0 (swap), leave other values as-is
    geom = raw.copy()
    geom[raw == 0] = 1
    geom[raw == 1] = 0
    return geom.astype(np.float32)


def predict_tf(geom, model_path):
    """Run prediction with TensorFlow SavedModel."""
    import tensorflow as tf
    tf.get_logger().setLevel('ERROR')
    model = tf.saved_model.load(model_path)
    # Try to find the inference function
    if hasattr(model, 'signatures'):
        infer = model.signatures.get('serving_default')
        if infer is None:
            infer = list(model.signatures.values())[0]
    else:
        infer = model.__call__

    input_tensor = tf.constant(geom.reshape(1, *geom.shape, 1))

    # Try calling the model
    try:
        result = infer(input_tensor)
        if isinstance(result, dict):
            # Get the first output
            key = list(result.keys())[0]
            output = result[key].numpy()
        else:
            output = result.numpy()
    except TypeError:
        # Try with keyword argument
        sig = model.signatures.get('serving_default')
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


def compare_predictions(tf_out, onnx_out, direction, geom):
    """Compare TF and ONNX predictions in detail."""
    print(f"\n{'='*60}")
    print(f"Direction: {direction}")
    print(f"{'='*60}")

    # Basic shape check
    print(f"  TF output shape:   {tf_out.shape}")
    print(f"  ONNX output shape: {onnx_out.shape}")
    assert tf_out.shape == onnx_out.shape, "Shape mismatch!"

    # Compute differences
    diff = np.abs(tf_out - onnx_out)
    rel_diff = np.where(np.abs(tf_out) > 1e-10,
                        diff / np.abs(tf_out), 0)

    print(f"\n  TF output:   min={tf_out.min():.6e}, max={tf_out.max():.6e}, "
          f"mean={tf_out.mean():.6e}, std={tf_out.std():.6e}")
    print(f"  ONNX output: min={onnx_out.min():.6e}, max={onnx_out.max():.6e}, "
          f"mean={onnx_out.mean():.6e}, std={onnx_out.std():.6e}")

    print(f"\n  Absolute diff: max={diff.max():.6e}, mean={diff.mean():.6e}")
    print(f"  Relative diff: max={rel_diff.max():.6e}, mean={rel_diff.mean():.6e}")

    # Check non-zero values
    tf_nonzero = np.count_nonzero(np.abs(tf_out) > 1e-10)
    onnx_nonzero = np.count_nonzero(np.abs(onnx_out) > 1e-10)
    total = tf_out.size
    print(f"\n  TF non-zero:   {tf_nonzero}/{total} ({100*tf_nonzero/total:.1f}%)")
    print(f"  ONNX non-zero: {onnx_nonzero}/{total} ({100*onnx_nonzero/total:.1f}%)")

    # Correlation
    if tf_nonzero > 0:
        correlation = np.corrcoef(tf_out.flatten(), onnx_out.flatten())[0, 1]
        print(f"  Correlation: {correlation:.10f}")

    # Check fluid vs solid predictions
    fluid_mask = (geom > 0.5)  # fluid cells (value 1 or 2 after inversion)
    solid_mask = (geom < 0.5)  # solid cells (value 0 after inversion)

    tf_fluid = tf_out[fluid_mask]
    onnx_fluid = onnx_out[fluid_mask]
    tf_solid = tf_out[solid_mask]
    onnx_solid = onnx_out[solid_mask]

    print(f"\n  Fluid voxels ({fluid_mask.sum()}):")
    print(f"    TF:   mean={tf_fluid.mean():.6e}, max={tf_fluid.max():.6e}")
    print(f"    ONNX: mean={onnx_fluid.mean():.6e}, max={onnx_fluid.max():.6e}")
    print(f"  Solid voxels ({solid_mask.sum()}):")
    print(f"    TF:   mean={tf_solid.mean():.6e}, max={np.abs(tf_solid).max():.6e}")
    print(f"    ONNX: mean={onnx_solid.mean():.6e}, max={np.abs(onnx_solid).max():.6e}")

    # Scaled velocity statistics
    scale = SCALING[direction]
    tf_vel = tf_out * scale
    onnx_vel = onnx_out * scale
    print(f"\n  Scaled velocity (factor {scale}):")
    print(f"    TF:   max={tf_vel.max():.6e} m/s, mean(nonzero)="
          f"{tf_vel[np.abs(tf_vel)>1e-15].mean():.6e} m/s" if tf_nonzero > 0 else "")
    print(f"    ONNX: max={onnx_vel.max():.6e} m/s, mean(nonzero)="
          f"{onnx_vel[np.abs(onnx_vel)>1e-15].mean():.6e} m/s" if onnx_nonzero > 0 else "")

    # Pass/fail
    if diff.max() < 1e-5 and (correlation > 0.9999 if tf_nonzero > 0 else True):
        print(f"\n  RESULT: PASS (max diff {diff.max():.2e})")
        return True
    else:
        print(f"\n  RESULT: FAIL")
        return False


def main():
    print("Loading 80x80x80 geometry...")
    geom_80 = load_geometry(GEOM_80, 80)
    print(f"  Shape: {geom_80.shape}")
    print(f"  Value distribution: 0={np.sum(geom_80==0)/geom_80.size*100:.1f}%, "
          f"1={np.sum(geom_80==1)/geom_80.size*100:.1f}%, "
          f"2={np.sum(geom_80==2)/geom_80.size*100:.1f}%")

    # Verify fiber orientation
    # Fibers (solid=0 after inversion) should be along axis 0 for fdir=0
    solid_projection = np.sum(geom_80 == 0, axis=0)  # project along axis 0
    print(f"  Solid projection along axis 0: mean={solid_projection.mean():.1f}, "
          f"std={solid_projection.std():.1f}")
    print(f"  (Low std = fibers along that axis)")

    all_pass = True

    for direction in ["x", "y", "z"]:
        print(f"\n--- Loading models for direction {direction} ---")

        tf_path = TF_MODELS[direction]
        onnx_path = ONNX_MODELS[direction]

        print(f"  TF model:   {tf_path}")
        print(f"  ONNX model: {onnx_path}")

        try:
            print("  Running TF prediction...")
            tf_out = predict_tf(geom_80, tf_path)
        except Exception as e:
            print(f"  TF prediction FAILED: {e}")
            all_pass = False
            continue

        try:
            print("  Running ONNX prediction...")
            onnx_out = predict_onnx(geom_80, onnx_path)
        except Exception as e:
            print(f"  ONNX prediction FAILED: {e}")
            all_pass = False
            continue

        passed = compare_predictions(tf_out, onnx_out, direction, geom_80)
        if not passed:
            all_pass = False

    print(f"\n{'='*60}")
    if all_pass:
        print("ALL DIRECTIONS PASS: TF and ONNX outputs match.")
    else:
        print("SOME DIRECTIONS FAILED: Check output above.")
    print(f"{'='*60}")

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
