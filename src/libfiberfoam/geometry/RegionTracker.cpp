#include "geometry/RegionTracker.h"

#include <stdexcept>

namespace fiberfoam
{

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
RegionTracker::RegionTracker(const std::vector<int8_t>& regionMask, int nx, int ny, int nz)
    : regionMask_(regionMask), nx_(nx), ny_(ny), nz_(nz)
{
    if (static_cast<int>(regionMask_.size()) != nx_ * ny_ * nz_)
    {
        throw std::runtime_error(
            "RegionTracker: mask size (" + std::to_string(regionMask_.size()) +
            ") does not match dimensions (" + std::to_string(nx_) + " x " +
            std::to_string(ny_) + " x " + std::to_string(nz_) + ")");
    }
}

// ---------------------------------------------------------------------------
// regionAt - look up the region at voxel coordinate (x, y, z)
// ---------------------------------------------------------------------------
CellRegion RegionTracker::regionAt(int x, int y, int z) const
{
    int idx = x + nx_ * (y + ny_ * z);
    if (idx < 0 || idx >= static_cast<int>(regionMask_.size()))
    {
        throw std::out_of_range(
            "RegionTracker::regionAt: index out of range for (" +
            std::to_string(x) + ", " + std::to_string(y) + ", " +
            std::to_string(z) + ")");
    }
    return static_cast<CellRegion>(regionMask_[idx]);
}

// ---------------------------------------------------------------------------
// regionForCell - look up the region for a given cell index
// ---------------------------------------------------------------------------
CellRegion RegionTracker::regionForCell(int cellIndex) const
{
    auto it = cellRegions_.find(cellIndex);
    if (it == cellRegions_.end())
    {
        throw std::out_of_range(
            "RegionTracker::regionForCell: cell index " +
            std::to_string(cellIndex) + " not found");
    }
    return it->second;
}

// ---------------------------------------------------------------------------
// buildFromCellMap - populate cellRegions_ from a mesh cell map
// ---------------------------------------------------------------------------
void RegionTracker::buildFromCellMap(
    const std::map<int, CellData>& cellMap,
    const std::vector<int8_t>& regionMask,
    int nx, int ny, int nz)
{
    regionMask_ = regionMask;
    nx_ = nx;
    ny_ = ny;
    nz_ = nz;

    cellRegions_.clear();

    for (const auto& [cellIdx, cellData] : cellMap)
    {
        int x = cellData.coord[0];
        int y = cellData.coord[1];
        int z = cellData.coord[2];

        int maskIdx = x + nx_ * (y + ny_ * z);
        if (maskIdx >= 0 && maskIdx < static_cast<int>(regionMask_.size()))
        {
            cellRegions_[cellIdx] = static_cast<CellRegion>(regionMask_[maskIdx]);
        }
        else
        {
            // Cells outside the mask default to Fibrous
            cellRegions_[cellIdx] = CellRegion::Fibrous;
        }
    }
}

// ---------------------------------------------------------------------------
// Count methods
// ---------------------------------------------------------------------------
int RegionTracker::countFibrousCells() const
{
    int count = 0;
    for (const auto& [idx, region] : cellRegions_)
    {
        if (region == CellRegion::Fibrous)
            ++count;
    }
    return count;
}

int RegionTracker::countBufferInletCells() const
{
    int count = 0;
    for (const auto& [idx, region] : cellRegions_)
    {
        if (region == CellRegion::BufferInlet)
            ++count;
    }
    return count;
}

int RegionTracker::countBufferOutletCells() const
{
    int count = 0;
    for (const auto& [idx, region] : cellRegions_)
    {
        if (region == CellRegion::BufferOutlet)
            ++count;
    }
    return count;
}

} // namespace fiberfoam
