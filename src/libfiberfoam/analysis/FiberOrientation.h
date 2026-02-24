#pragma once

#include "geometry/VoxelArray.h"

namespace fiberfoam
{

/// FFT-based fiber orientation estimation.
/// Returns angle in degrees normalised to [0, 90].
/// Uses FFTW3 for 3D FFT and Eigen for PCA.
double estimateFiberOrientation(
    const VoxelArray& geometry,
    double gaussianSigma = 4.0);

} // namespace fiberfoam
