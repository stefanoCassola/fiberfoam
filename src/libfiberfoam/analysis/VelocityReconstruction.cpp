#include "analysis/VelocityReconstruction.h"
#include "common/Logger.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace fiberfoam
{
namespace
{

// ---------------------------------------------------------------------------
// Natural cubic spline interpolation.
//
// Given sorted knots (xs, ys) of size n, builds the spline coefficients once
// and evaluates at a single query point x.  For repeated evaluations the
// coefficients could be cached, but the data set here is tiny (13 points)
// so rebuilding is negligible.
// ---------------------------------------------------------------------------
class CubicSpline
{
public:
    CubicSpline(const std::vector<double>& xs, const std::vector<double>& ys)
        : xs_(xs), ys_(ys)
    {
        const int n = static_cast<int>(xs.size());
        if (n < 2)
            throw std::runtime_error("CubicSpline: need at least 2 data points");

        // Step sizes and slopes.
        std::vector<double> h(n - 1), alpha(n - 1);
        for (int i = 0; i < n - 1; ++i)
            h[i] = xs[i + 1] - xs[i];
        for (int i = 1; i < n - 1; ++i)
        {
            alpha[i] = 3.0 / h[i] * (ys[i + 1] - ys[i]) -
                        3.0 / h[i - 1] * (ys[i] - ys[i - 1]);
        }

        // Solve tridiagonal system for natural spline (c[0] = c[n-1] = 0).
        c_.resize(n, 0.0);
        std::vector<double> l(n, 1.0), mu(n, 0.0), z(n, 0.0);
        for (int i = 1; i < n - 1; ++i)
        {
            l[i] = 2.0 * (xs[i + 1] - xs[i - 1]) - h[i - 1] * mu[i - 1];
            mu[i] = h[i] / l[i];
            z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
        }
        for (int j = n - 2; j >= 0; --j)
            c_[j] = z[j] - mu[j] * c_[j + 1];

        // Compute b and d coefficients.
        b_.resize(n - 1);
        d_.resize(n - 1);
        for (int i = 0; i < n - 1; ++i)
        {
            b_[i] = (ys[i + 1] - ys[i]) / h[i] -
                     h[i] * (c_[i + 1] + 2.0 * c_[i]) / 3.0;
            d_[i] = (c_[i + 1] - c_[i]) / (3.0 * h[i]);
        }
    }

    double operator()(double x) const
    {
        const int n = static_cast<int>(xs_.size());

        // Clamp to the data range.
        if (x <= xs_.front())
            return ys_.front();
        if (x >= xs_.back())
            return ys_.back();

        // Binary search for the correct interval.
        int lo = 0, hi = n - 2;
        while (lo < hi)
        {
            int mid = (lo + hi) / 2;
            if (xs_[mid + 1] < x)
                lo = mid + 1;
            else
                hi = mid;
        }

        double dx = x - xs_[lo];
        return ys_[lo] + b_[lo] * dx + c_[lo] * dx * dx + d_[lo] * dx * dx * dx;
    }

private:
    std::vector<double> xs_, ys_;
    std::vector<double> b_, c_, d_;
};

// Empirical angle-to-ratio data from the Python reference.
const std::vector<double> kAngles = {
    0.0, 11.25, 15.0, 22.5, 30.0, 33.75, 45.0,
    56.25, 60.0, 67.5, 75.0, 78.75, 90.0};

const std::vector<double> kRatios = {
    0.0,
    0.181081085,
    0.239252444,
    0.368015911,
    0.505970879,
    0.565527008,
    0.821783827,
    1.129726343,
    1.28506696,
    1.521385831,
    1.675121897,
    1.740792248,
    0.0};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

double velocityRatioFromAngle(double angleDeg)
{
    if (angleDeg < 0.0 || angleDeg > 90.0)
        throw std::runtime_error(
            "velocityRatioFromAngle: angle must be in [0, 90], got " +
            std::to_string(angleDeg));

    static const CubicSpline spline(kAngles, kRatios);
    return spline(angleDeg);
}

std::vector<double> reconstructSecondaryVelocity(
    const std::vector<double>& primaryVelocity,
    FlowDirection flowDirection,
    double fiberAngleDeg)
{
    // Determine the scaling angle following the Python convention:
    //   X flow  ->  scaling_angle = fiberAngle
    //   Y flow  ->  scaling_angle = 90 - fiberAngle
    double scalingAngle = 0.0;
    switch (flowDirection)
    {
    case FlowDirection::X:
        scalingAngle = fiberAngleDeg;
        break;
    case FlowDirection::Y:
        scalingAngle = 90.0 - fiberAngleDeg;
        break;
    case FlowDirection::Z:
        // Not present in the original Python code; default to using the angle
        // directly (same convention as X).
        scalingAngle = fiberAngleDeg;
        Logger::warning("reconstructSecondaryVelocity: Z flow direction not in "
                        "the original model -- using fiberAngle directly");
        break;
    }

    // Clamp to valid interpolation range.
    scalingAngle = std::max(0.0, std::min(90.0, scalingAngle));

    const double ratio = velocityRatioFromAngle(scalingAngle);

    Logger::info("Velocity reconstruction: scalingAngle=" +
                 std::to_string(scalingAngle) + " deg, ratio=" +
                 std::to_string(ratio));

    std::vector<double> result(primaryVelocity.size());
    for (size_t i = 0; i < primaryVelocity.size(); ++i)
        result[i] = primaryVelocity[i] * ratio;

    return result;
}

} // namespace fiberfoam
