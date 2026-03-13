"""
Pure numpy/scipy FNO (Fourier Neural Operator) 3D inference engine.

Replaces TensorFlow dependency for FNO model inference. Loads weights
from .npz files exported from TF SavedModels via scripts/export_fno_weights.py.

Architecture:
  Input (B,80,80,80,1)
  → concat grid coords → (B,80,80,80,4)
  → Dense(4→8) lifting
  → transpose to channels-first (B,8,80,80,80)
  → 4× FNO blocks:
      spectral_conv(x) → MLP(8→8, GELU, 8→8, ReLU)
      + conv1x1 skip on block input
      → GELU
  → transpose to channels-last (B,80,80,80,8)
  → MLP projection: Conv1x1(8→32, GELU) → Conv1x1(32→1, ReLU)
  → Output (B,80,80,80,1)
"""
import numpy as np
from scipy.special import erf


def gelu(x):
    """Gaussian Error Linear Unit activation."""
    return 0.5 * x * (1.0 + erf(x / np.sqrt(2.0)))


def conv3d_1x1(x, kernel, bias):
    """Apply 1×1×1 convolution (channels-last format).

    x:      (B, D, H, W, C_in)
    kernel: (1, 1, 1, C_in, C_out)
    bias:   (C_out,)
    Returns: (B, D, H, W, C_out)
    """
    # 1×1×1 conv is just a matmul on the last axis
    k = kernel.reshape(kernel.shape[-2], kernel.shape[-1])
    return np.tensordot(x, k, axes=([-1], [0])) + bias


def spectral_conv3d(x, weights, modes):
    """3D spectral convolution (channels-first).

    x: (B, C_in, D, H, W)
    weights: dict with w1_real, w1_imag, ..., w4_real, w4_imag
             each shape (C_in, C_out, modes, modes, modes)
    modes: number of Fourier modes to keep per axis (24)
    Returns: (B, C_out, D, H, W)
    """
    B, C_in, D, H, W = x.shape

    # FFT along spatial dims (last 3 axes of each channel)
    # Reshape to (B*C_in, D, H, W) for rfftn
    x_flat = x.reshape(B * C_in, D, H, W)
    x_ft = np.fft.rfftn(x_flat, axes=(-3, -2, -1))
    # x_ft shape: (B*C_in, D, H, W//2+1) complex
    x_ft = x_ft.reshape(B, C_in, D, H, W // 2 + 1)

    C_out = weights["w1_real"].shape[1]
    out_ft = np.zeros((B, C_out, D, H, W // 2 + 1), dtype=np.complex64)

    m = modes  # shorthand

    # Build complex weights for 4 quadrants
    for wname, slices in [
        ("w1", (slice(None, m), slice(None, m), slice(None, m))),
        ("w2", (slice(-m, None), slice(None, m), slice(None, m))),
        ("w3", (slice(None, m), slice(-m, None), slice(None, m))),
        ("w4", (slice(-m, None), slice(-m, None), slice(None, m))),
    ]:
        w_complex = (
            weights[f"{wname}_real"] + 1j * weights[f"{wname}_imag"]
        ).astype(np.complex64)
        # w_complex: (C_in, C_out, m, m, m)

        # Extract the relevant Fourier modes from input
        d_sl, h_sl, w_sl = slices
        x_modes = x_ft[:, :, d_sl, h_sl, w_sl]
        # x_modes: (B, C_in, m, m, m)

        # Einsum: contract over C_in
        # 'bixyz,ioxy->boxyz'
        result = np.einsum("bixyz,ioxyz->boxyz", x_modes, w_complex)

        # Place result in correct position in output spectrum
        out_ft[:, :, d_sl, h_sl, w_sl] += result

    # Inverse FFT
    out_flat = out_ft.reshape(B * C_out, D, H, W // 2 + 1)
    out = np.fft.irfftn(out_flat, s=(D, H, W), axes=(-3, -2, -1))
    return out.reshape(B, C_out, D, H, W).astype(np.float32)


def get_grid_3d(shape):
    """Generate normalized [0,1] grid coordinates.

    shape: (B, D, H, W, ...)
    Returns: (B, D, H, W, 3)
    """
    B, D, H, W = shape[:4]
    grid_d = np.linspace(0, 1, D, dtype=np.float32)
    grid_h = np.linspace(0, 1, H, dtype=np.float32)
    grid_w = np.linspace(0, 1, W, dtype=np.float32)

    gd, gh, gw = np.meshgrid(grid_d, grid_h, grid_w, indexing="ij")
    grid = np.stack([gd, gh, gw], axis=-1)  # (D, H, W, 3)
    grid = np.tile(grid[np.newaxis], (B, 1, 1, 1, 1))  # (B, D, H, W, 3)
    return grid


class FNOModel:
    """Pure numpy FNO 3D inference model."""

    def __init__(self, npz_path):
        data = np.load(npz_path)
        self.w = dict(data)
        data.close()
        self.modes = 24
        self.n_blocks = 4
        self.padding = 6

    def _get_spectral_weights(self, block_idx):
        prefix = f"spectral_conv3d_{block_idx}"
        return {
            key: self.w[f"{prefix}/{key}"]
            for key in [
                "w1_real", "w1_imag", "w2_real", "w2_imag",
                "w3_real", "w3_imag", "w4_real", "w4_imag",
            ]
        }

    def _get_mlp_weights(self, block_idx):
        """Get MLP weights for FNO block skip path."""
        if block_idx == 0:
            # Block 0 uses 'mlp3d' scope (first instance)
            return {
                "k1": self.w["mlp3d/conv3d/kernel"],
                "b1": self.w["mlp3d/conv3d/bias"],
                "k2": self.w["mlp3d/conv3d_1/kernel"],
                "b2": self.w["mlp3d/conv3d_1/bias"],
            }
        else:
            n = block_idx
            c1 = n * 2
            c2 = c1 + 1
            return {
                "k1": self.w[f"mlp3d_{n}/conv3d_{c1}/kernel"],
                "b1": self.w[f"mlp3d_{n}/conv3d_{c1}/bias"],
                "k2": self.w[f"mlp3d_{n}/conv3d_{c1 + 1}/kernel"],
                "b2": self.w[f"mlp3d_{n}/conv3d_{c1 + 1}/bias"],
            }

    def predict(self, geom):
        """Run FNO inference.

        geom: numpy array, shape (D, H, W) with float32 values
        Returns: numpy array, shape (D, H, W)
        """
        x = geom.reshape(1, *geom.shape, 1).astype(np.float32)
        B, D, H, W, _ = x.shape

        # 1. Append grid coordinates
        grid = get_grid_3d(x.shape)  # (B, D, H, W, 3)
        x = np.concatenate([x, grid], axis=-1)  # (B, D, H, W, 4)

        # 2. Lifting: Dense(4 → 8)
        x = conv3d_1x1(
            x,
            self.w["dense/kernel"].reshape(1, 1, 1, 4, 8),
            self.w["dense/bias"],
        )  # (B, D, H, W, 8)

        # 3. Transpose to channels-first for spectral convolution
        x = x.transpose(0, 4, 1, 2, 3)  # (B, 8, D, H, W)

        # 3b. Spatial padding (the TF model pads 6 on axis D end)
        # This avoids periodic boundary artifacts from FFT.
        pad_d = self.padding
        if pad_d > 0:
            x = np.pad(
                x, ((0, 0), (0, 0), (0, pad_d), (0, 0), (0, 0))
            )

        # 4. FNO blocks
        for i in range(self.n_blocks):
            spectral_w = self._get_spectral_weights(i)
            mlp_w = self._get_mlp_weights(i)

            # Spectral convolution (channels-first)
            x1 = spectral_conv3d(x, spectral_w, self.modes)
            # (B, 8, D, H, W)

            # Reshape spectral output to channels-last for MLP
            x1 = x1.transpose(0, 2, 3, 4, 1)  # (B, D, H, W, 8)

            # MLP on spectral output: Conv(8→8) → GELU → Conv(8→8) → ReLU
            x1 = conv3d_1x1(x1, mlp_w["k1"], mlp_w["b1"])
            x1 = gelu(x1)
            x1 = conv3d_1x1(x1, mlp_w["k2"], mlp_w["b2"])
            x1 = np.maximum(x1, 0)  # ReLU

            # Linear skip: Conv1x1 on block input (channels-last)
            skip_idx = 8 + i
            x_last = x.transpose(0, 2, 3, 4, 1)  # channels-last
            x2 = conv3d_1x1(
                x_last,
                self.w[f"conv3d_{skip_idx}/kernel"],
                self.w[f"conv3d_{skip_idx}/bias"],
            )

            # Combine and activate
            x = gelu(x1 + x2)
            # (B, D, H, W, 8) channels-last

            # Back to channels-first for next block
            x = x.transpose(0, 4, 1, 2, 3)

        # 5. Remove spatial padding before projection
        if pad_d > 0:
            x = x[:, :, :D, :, :]

        # 6. Output projection (channels-last)
        x = x.transpose(0, 2, 3, 4, 1)  # (B, D, H, W, 8)

        # The output MLP reuses 'mlp3d' scope (second instance, stored as __dup)
        proj_k1 = self.w.get(
            "mlp3d/conv3d/kernel__dup1", self.w.get("mlp3d/conv3d/kernel")
        )
        proj_b1 = self.w.get(
            "mlp3d/conv3d/bias__dup1", self.w.get("mlp3d/conv3d/bias")
        )
        proj_k2 = self.w.get(
            "mlp3d/conv3d_1/kernel__dup1",
            self.w.get("mlp3d/conv3d_1/kernel"),
        )
        proj_b2 = self.w.get(
            "mlp3d/conv3d_1/bias__dup1", self.w.get("mlp3d/conv3d_1/bias")
        )

        x = conv3d_1x1(x, proj_k1, proj_b1)  # 8→32
        x = gelu(x)
        x = conv3d_1x1(x, proj_k2, proj_b2)  # 32→1
        x = np.maximum(x, 0)  # ReLU

        return x.squeeze()
