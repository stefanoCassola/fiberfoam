#pragma once

#include "common/Types.h"
#include <vector>

namespace fiberfoam
{

/// Compute velocity ratio from fiber angle using cubic spline interpolation
/// of empirical data points.
double velocityRatioFromAngle(double angleDeg);

/// Reconstruct secondary velocity component from primary velocity
/// and fiber orientation angle.
std::vector<double> reconstructSecondaryVelocity(
    const std::vector<double>& primaryVelocity,
    FlowDirection flowDirection,
    double fiberAngleDeg);

} // namespace fiberfoam
