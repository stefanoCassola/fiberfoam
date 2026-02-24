#include "postprocessing/Permeability.h"

#include <cmath>
#include <stdexcept>

namespace fiberfoam
{

PermeabilityCalculator::PermeabilityCalculator(Options opts)
    : opts_(std::move(opts))
{
}

PermeabilityCalculator::BBox
PermeabilityCalculator::computeROIBounds(FlowDirection direction) const
{
    // The ROI trims inlet and outlet buffer regions from the main flow
    // direction while keeping the full extent in the secondary and tertiary
    // directions.  This matches the permCalc.H logic:
    //   mainDirMinDim = Nxmin + inlet_length * scale
    //   mainDirMaxDim = Nxmax - outlet_length * scale

    BBox roi{};
    roi.minMain = opts_.meshMinMain + opts_.inletLength * opts_.scale;
    roi.maxMain = opts_.meshMaxMain - opts_.outletLength * opts_.scale;
    roi.minSec = opts_.meshMinSec;
    roi.maxSec = opts_.meshMaxSec;
    roi.minTert = opts_.meshMinTert;
    roi.maxTert = opts_.meshMaxTert;

    return roi;
}

bool PermeabilityCalculator::isCellInROI(const std::array<double, 3>& center,
                                         FlowDirection direction,
                                         const BBox& roi) const
{
    const int mainIdx = axisIndex(direction);
    const int secIdx = axisIndex(secondaryDirection(direction));
    const int tertIdx = axisIndex(tertiaryDirection(direction));

    return center[mainIdx] >= roi.minMain && center[mainIdx] <= roi.maxMain &&
           center[secIdx] >= roi.minSec && center[secIdx] <= roi.maxSec &&
           center[tertIdx] >= roi.minTert && center[tertIdx] <= roi.maxTert;
}

PermeabilityResult PermeabilityCalculator::computeFromFields(
    const std::vector<std::array<double, 3>>& velocities,
    const std::vector<std::array<double, 3>>& cellCenters,
    double meshVolume,
    FlowDirection direction,
    double outletFlux)
{
    return compute(velocities, cellCenters, meshVolume, direction, outletFlux);
}

PermeabilityResult PermeabilityCalculator::compute(
    const std::vector<std::array<double, 3>>& velocities,
    const std::vector<std::array<double, 3>>& cellCenters,
    double meshVolume,
    FlowDirection direction,
    double outletFlux)
{
    if (velocities.size() != cellCenters.size())
    {
        throw std::invalid_argument(
            "PermeabilityCalculator::compute: velocities and cellCenters size mismatch");
    }

    const int mainIdx = axisIndex(direction);
    const int secIdx = axisIndex(secondaryDirection(direction));
    const int tertIdx = axisIndex(tertiaryDirection(direction));

    // ------------------------------------------------------------------
    // 1. Compute ROI bounding box (fibrous region without inlet/outlet)
    // ------------------------------------------------------------------
    const BBox roi = computeROIBounds(direction);

    // ------------------------------------------------------------------
    // 2. Select cells within the ROI and accumulate velocity
    // ------------------------------------------------------------------
    double sumU_main = 0.0;
    double sumU_sec = 0.0;
    double sumU_tert = 0.0;
    int selectedCount = 0;

    for (size_t i = 0; i < cellCenters.size(); ++i)
    {
        if (!isCellInROI(cellCenters[i], direction, roi))
            continue;

        sumU_main += velocities[i][mainIdx];
        sumU_sec += velocities[i][secIdx];
        sumU_tert += velocities[i][tertIdx];
        ++selectedCount;
    }

    PermeabilityResult result{};
    result.direction = direction;

    if (selectedCount == 0)
    {
        // No cells in ROI -- return zeroed result
        return result;
    }

    // Volume-averaged velocity in ROI
    const double avgU_main = sumU_main / selectedCount;
    const double avgU_sec = sumU_sec / selectedCount;
    const double avgU_tert = sumU_tert / selectedCount;

    // ------------------------------------------------------------------
    // 3. Geometric quantities
    // ------------------------------------------------------------------
    const double flowLengthROI = roi.maxMain - roi.minMain;
    const double flowCrossArea =
        (roi.maxSec - roi.minSec) * (roi.maxTert - roi.minTert);

    // Full domain flow length (for FVC calculation)
    const double flowLengthFull = opts_.meshMaxMain - opts_.meshMinMain;

    const double nu = opts_.fluid.kinematicViscosity;
    const double density = opts_.fluid.density;
    const double dP = opts_.fluid.pressureOutlet - opts_.fluid.pressureInlet;

    // ------------------------------------------------------------------
    // 4. Permeability (volume-averaged velocity)
    //    permVolAvg = -(avgU * nu * density * flowLengthROI) / (pOut - pIn)
    // ------------------------------------------------------------------
    if (std::abs(dP) > 1e-30)
    {
        result.permVolAvgMain =
            -(avgU_main * nu * density * flowLengthROI) / dP;
        result.permVolAvgSecondary =
            -(avgU_sec * nu * density * flowLengthROI) / dP;
        result.permVolAvgTertiary =
            -(avgU_tert * nu * density * flowLengthROI) / dP;
    }

    // ------------------------------------------------------------------
    // 5. Permeability (flow rate)
    //    permFlowRate = -((phi_outlet / crossArea) * nu * density
    //                     * flowLengthROI) / (pOut - pIn)
    // ------------------------------------------------------------------
    if (std::abs(dP) > 1e-30 && flowCrossArea > 1e-30)
    {
        const double avgFluxVel = outletFlux / flowCrossArea;
        result.permFlowRate =
            -(avgFluxVel * nu * density * flowLengthROI) / dP;
    }

    // ------------------------------------------------------------------
    // 6. Fiber volume content
    //    FVC = (1 - meshVol / (flowLength * crossArea)) * 100
    // ------------------------------------------------------------------
    const double domainVol = flowLengthFull * flowCrossArea;
    if (domainVol > 1e-30)
    {
        result.fiberVolumeContent =
            (1.0 - meshVolume / domainVol) * 100.0;
    }

    result.flowLength = flowLengthROI;
    result.crossSectionArea = flowCrossArea;

    return result;
}

} // namespace fiberfoam
