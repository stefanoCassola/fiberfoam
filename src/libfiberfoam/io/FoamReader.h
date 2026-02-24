#pragma once

#include "common/Types.h"
#include <array>
#include <string>
#include <vector>

namespace fiberfoam
{

class FoamReader
{
public:
    explicit FoamReader(const std::string& casePath);

    // Read velocity field from a time directory
    std::vector<std::array<double, 3>> readVelocity(const std::string& timeDir = "latestTime");

    // Read pressure field
    std::vector<double> readPressure(const std::string& timeDir = "latestTime");

    // Read phi (face flux) from boundary
    double readOutletFlux(const std::string& timeDir = "latestTime");

    // Find the latest time directory
    std::string findLatestTime() const;

private:
    std::string casePath_;
};

} // namespace fiberfoam
