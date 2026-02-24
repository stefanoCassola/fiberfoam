#pragma once

#include "common/Types.h"
#include "geometry/RegionTracker.h"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace fiberfoam
{

class PermeabilityCalculator
{
public:
    struct Options
    {
        FluidProperties fluid;
        bool fibrousRegionOnly = true;
        std::optional<RegionTracker> regionTracker;
        // Mesh bounds info (from blockMeshDict)
        double meshMinMain = 0, meshMaxMain = 0;
        double meshMinSec = 0, meshMaxSec = 0;
        double meshMinTert = 0, meshMaxTert = 0;
        double inletLength = 0, outletLength = 0;
        double scale = 1.0;
    };

    explicit PermeabilityCalculator(Options opts);

    // Compute permeability from velocity and pressure fields
    // velocities: per-cell (u, v, w)
    // cellCenters: per-cell (x, y, z)
    // meshVolume: total mesh volume
    PermeabilityResult computeFromFields(
        const std::vector<std::array<double, 3>>& velocities,
        const std::vector<std::array<double, 3>>& cellCenters,
        double meshVolume,
        FlowDirection direction,
        double outletFlux = 0.0);

    // Compute both methods and return result
    PermeabilityResult compute(
        const std::vector<std::array<double, 3>>& velocities,
        const std::vector<std::array<double, 3>>& cellCenters,
        double meshVolume,
        FlowDirection direction,
        double outletFlux);

private:
    Options opts_;

    // Bounding box for the fibrous region of interest
    struct BBox
    {
        double minMain, maxMain, minSec, maxSec, minTert, maxTert;
    };

    BBox computeROIBounds(FlowDirection direction) const;

    bool isCellInROI(const std::array<double, 3>& center,
                     FlowDirection direction,
                     const BBox& roi) const;
};

} // namespace fiberfoam
