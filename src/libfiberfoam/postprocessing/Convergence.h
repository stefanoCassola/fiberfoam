#pragma once

#include <vector>

namespace fiberfoam
{

class ConvergenceChecker
{
public:
    struct Options
    {
        int window = 10;          // number of values for regression
        double convSlope = 0.01;  // convergence slope threshold
        double errorBound = 0.01; // error between predicted and calculated
    };

    explicit ConvergenceChecker(Options opts);

    // Add a new permeability value at given iteration
    void addValue(double iteration, double permeability);

    // Check if converged
    bool isConverged() const;

    // Get current regression slope (normalized)
    double currentSlope() const;

    // Get predicted permeability
    double predictedPermeability() const;

    // Get error between predicted and current
    double predictionError() const;

    // Get all stored values
    const std::vector<double>& iterations() const { return iterations_; }
    const std::vector<double>& permeabilities() const { return permeabilities_; }

private:
    // Linear regression using Eigen (QR decomposition on Vandermonde matrix)
    // Returns {intercept, slope}
    static std::pair<double, double> linearFit(const std::vector<double>& x,
                                               const std::vector<double>& y);

    // Recompute convergence state from the latest window of values
    void updateState() const;

    Options opts_;
    std::vector<double> iterations_;
    std::vector<double> permeabilities_;

    mutable double slope_ = 0;
    mutable double predicted_ = 0;
    mutable double error_ = 0;
    mutable bool converged_ = false;
    mutable bool dirty_ = true; // whether state needs recomputation
};

} // namespace fiberfoam
