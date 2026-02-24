#include "geometry/FiberFreeRegion.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace fiberfoam
{

// ---------------------------------------------------------------------------
// pad - add all-fluid layers along the flow axis
// ---------------------------------------------------------------------------
PaddedGeometry FiberFreeRegion::pad(
    const VoxelArray& geometry,
    FlowDirection direction,
    int inletLayers,
    int outletLayers)
{
    const int origNx = geometry.nx();
    const int origNy = geometry.ny();
    const int origNz = geometry.nz();

    int newNx = origNx;
    int newNy = origNy;
    int newNz = origNz;

    switch (direction)
    {
    case FlowDirection::X:
        newNx = origNx + inletLayers + outletLayers;
        break;
    case FlowDirection::Y:
        newNy = origNy + inletLayers + outletLayers;
        break;
    case FlowDirection::Z:
        newNz = origNz + inletLayers + outletLayers;
        break;
    }

    int totalNew = newNx * newNy * newNz;
    std::vector<int8_t> newData(totalNew, 1); // all fluid by default
    std::vector<int8_t> regionMask(totalNew, 0);

    // Fill the padded array:
    //   - Inlet buffer layers: at the start of the flow axis -> all fluid (1), region = BufferInlet
    //   - Fibrous region: copy from original geometry, region = Fibrous
    //   - Outlet buffer layers: at the end of the flow axis -> all fluid (1), region = BufferOutlet

    for (int iz = 0; iz < newNz; ++iz)
    {
        for (int iy = 0; iy < newNy; ++iy)
        {
            for (int ix = 0; ix < newNx; ++ix)
            {
                int idx = ix + newNx * (iy + newNy * iz);

                // Determine which layer along the flow axis this cell is in
                int flowCoord = 0;
                int flowMax = 0;
                int origX = ix, origY = iy, origZ = iz;

                switch (direction)
                {
                case FlowDirection::X:
                    flowCoord = ix;
                    flowMax = newNx;
                    origX = ix - inletLayers;
                    break;
                case FlowDirection::Y:
                    flowCoord = iy;
                    flowMax = newNy;
                    origY = iy - inletLayers;
                    break;
                case FlowDirection::Z:
                    flowCoord = iz;
                    flowMax = newNz;
                    origZ = iz - inletLayers;
                    break;
                }

                (void)flowMax; // suppress unused warning

                if (flowCoord < inletLayers)
                {
                    // Inlet buffer: all fluid
                    newData[idx] = 1;
                    regionMask[idx] = static_cast<int8_t>(CellRegion::BufferInlet);
                }
                else if (flowCoord >= inletLayers + (direction == FlowDirection::X ? origNx
                                                   : direction == FlowDirection::Y ? origNy
                                                                                   : origNz))
                {
                    // Outlet buffer: all fluid
                    newData[idx] = 1;
                    regionMask[idx] = static_cast<int8_t>(CellRegion::BufferOutlet);
                }
                else
                {
                    // Fibrous region: copy from original
                    newData[idx] = geometry.at(origX, origY, origZ);
                    regionMask[idx] = static_cast<int8_t>(CellRegion::Fibrous);
                }
            }
        }
    }

    PaddedGeometry result;
    result.geometry = VoxelArray(std::move(newData), newNx, newNy, newNz);
    result.regionMask = std::move(regionMask);
    return result;
}

// ---------------------------------------------------------------------------
// fibrousExtent - physical start/end of the fibrous region along flow axis
// ---------------------------------------------------------------------------
std::pair<double, double> FiberFreeRegion::fibrousExtent(
    const PaddedGeometry& padded,
    FlowDirection direction,
    double voxelSize)
{
    const int nx = padded.geometry.nx();
    const int ny = padded.geometry.ny();
    const int nz = padded.geometry.nz();

    int flowDimSize = 0;
    switch (direction)
    {
    case FlowDirection::X:
        flowDimSize = nx;
        break;
    case FlowDirection::Y:
        flowDimSize = ny;
        break;
    case FlowDirection::Z:
        flowDimSize = nz;
        break;
    }

    // Find first and last fibrous layer along flow direction
    int firstFibrous = flowDimSize; // will be reduced
    int lastFibrous = -1;           // will be increased

    for (int iz = 0; iz < nz; ++iz)
    {
        for (int iy = 0; iy < ny; ++iy)
        {
            for (int ix = 0; ix < nx; ++ix)
            {
                int idx = ix + nx * (iy + ny * iz);
                if (padded.regionMask[idx] == static_cast<int8_t>(CellRegion::Fibrous))
                {
                    int flowCoord = 0;
                    switch (direction)
                    {
                    case FlowDirection::X:
                        flowCoord = ix;
                        break;
                    case FlowDirection::Y:
                        flowCoord = iy;
                        break;
                    case FlowDirection::Z:
                        flowCoord = iz;
                        break;
                    }
                    firstFibrous = std::min(firstFibrous, flowCoord);
                    lastFibrous = std::max(lastFibrous, flowCoord);
                }
            }
        }
    }

    if (lastFibrous < 0)
    {
        // No fibrous cells found
        return {0.0, 0.0};
    }

    // The fibrous region spans from the start of firstFibrous voxel
    // to the end of lastFibrous voxel (i.e., lastFibrous + 1)
    double start = firstFibrous * voxelSize;
    double end = (lastFibrous + 1) * voxelSize;

    return {start, end};
}

} // namespace fiberfoam
