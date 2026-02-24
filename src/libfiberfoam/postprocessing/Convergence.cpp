#include "postprocessing/Convergence.h"

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>

namespace fiberfoam
{

ConvergenceChecker::ConvergenceChecker(Options opts)
    : opts_(std::move(opts))
{
}

void ConvergenceChecker::addValue(double iteration, double permeability)
{
    iterations_.push_back(iteration);
    permeabilities_.push_back(permeability);
    dirty_ = true;
}

bool ConvergenceChecker::isConverged() const
{
    if (dirty_)
        updateState();
    return converged_;
}

double ConvergenceChecker::currentSlope() const
{
    if (dirty_)
        updateState();
    return slope_;
}

double ConvergenceChecker::predictedPermeability() const
{
    if (dirty_)
        updateState();
    return predicted_;
}

double ConvergenceChecker::predictionError() const
{
    if (dirty_)
        updateState();
    return error_;
}

// ---------------------------------------------------------------------------
// Linear regression via Eigen's Householder QR decomposition.
// Builds a Vandermonde matrix T for polynomial order 1 (linear):
//   T(i, 0) = 1
//   T(i, 1) = x[i]
// Solves T * coeffs = y  =>  coeffs = {intercept, slope}
// ---------------------------------------------------------------------------
std::pair<double, double> ConvergenceChecker::linearFit(
    const std::vector<double>& x,
    const std::vector<double>& y)
{
    const int n = static_cast<int>(x.size());

    // Build Vandermonde matrix (n x 2)
    Eigen::MatrixXd T(n, 2);
    Eigen::VectorXd V(n);

    for (int i = 0; i < n; ++i)
    {
        T(i, 0) = 1.0;
        T(i, 1) = x[i];
        V(i) = y[i];
    }

    // Least-squares solve via Householder QR
    Eigen::VectorXd coeffs = T.householderQr().solve(V);

    return {coeffs(0), coeffs(1)}; // {intercept, slope}
}

// ---------------------------------------------------------------------------
// Recompute convergence state from the latest window of values.
//
// Matching permConv.H logic:
//   - Take the last `window` values
//   - Normalize iterations to [0, 1] by dividing by max(iterations_window)
//   - Normalize permeabilities to [0, 1] by dividing by max(permeabilities_all)
//   - Perform linear regression on normalized data
//   - Check: slope < convSlope AND |1 - predicted/current| < errorBound
//
// Predicted permeability (un-normalized):
//   In the normalized space the next point is at
//   xNext = (maxSubIter + 1) / maxSubIter   (since we normalized by max)
//   but permConv.H simply uses the next integer iteration in the *sub* space.
//   We follow the reference: predict at the next sub-iteration index,
//   normalise by the same maxIter used for the window, evaluate the linear
//   model, then multiply back by maxPerm.
// ---------------------------------------------------------------------------
void ConvergenceChecker::updateState() const
{
    dirty_ = false;
    converged_ = false;
    slope_ = 0;
    predicted_ = 0;
    error_ = 0;

    const int totalValues = static_cast<int>(iterations_.size());
    if (totalValues <= opts_.window)
        return;

    // ---- Extract last `window` values ----
    const int startIdx = totalValues - opts_.window;
    std::vector<double> subIter(opts_.window);
    std::vector<double> subPerm(opts_.window);

    for (int i = 0; i < opts_.window; ++i)
    {
        subIter[i] = iterations_[startIdx + i];
        subPerm[i] = permeabilities_[startIdx + i];
    }

    // ---- Find normalization factors ----
    const double maxIter =
        *std::max_element(subIter.begin(), subIter.end());

    // maxPerm from ALL permeability values (matching reference behaviour)
    const double maxPerm =
        *std::max_element(permeabilities_.begin(), permeabilities_.end());

    if (maxIter == 0.0 || maxPerm == 0.0)
        return;

    // ---- Normalize to [0, 1] ----
    std::vector<double> normIter(opts_.window);
    std::vector<double> normPerm(opts_.window);

    for (int i = 0; i < opts_.window; ++i)
    {
        normIter[i] = subIter[i] / maxIter;
        normPerm[i] = subPerm[i] / maxPerm;
    }

    // ---- Linear fit on normalized data ----
    auto [intercept, slope] = linearFit(normIter, normPerm);
    slope_ = slope;

    // ---- Predicted permeability ----
    // Next sub-iteration in the normalized space
    const double nextIterNorm = (maxIter + 1.0) / maxIter;
    const double predNorm = intercept + slope * nextIterNorm;
    predicted_ = predNorm * maxPerm;

    // ---- Error between predicted and current ----
    const double currentPerm = permeabilities_.back();
    if (std::abs(currentPerm) > 1e-30)
    {
        error_ = 1.0 - predicted_ / currentPerm;
    }

    // ---- Convergence check ----
    converged_ = (std::abs(slope) < opts_.convSlope) &&
                 (std::abs(error_) < opts_.errorBound);
}

} // namespace fiberfoam
