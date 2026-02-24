#pragma once

#include "VoxelArray.h"
#include "common/Types.h"
#include <vector>

namespace fiberfoam
{

struct PaddedGeometry
{
    VoxelArray geometry;
    std::vector<int8_t> regionMask; // same size as geometry, values = CellRegion
};

class FiberFreeRegion
{
public:
    // Pad geometry with all-fluid layers along flow axis
    // Inlet layers added at the start of the flow axis, outlet at the end
    // Region mask: 0=fibrous, 1=inlet_buffer, 2=outlet_buffer
    static PaddedGeometry pad(
        const VoxelArray& geometry,
        FlowDirection direction,
        int inletLayers,
        int outletLayers);

    // Get physical extent of fibrous region [start, end] in meters
    static std::pair<double, double> fibrousExtent(
        const PaddedGeometry& padded,
        FlowDirection direction,
        double voxelSize);
};

} // namespace fiberfoam
