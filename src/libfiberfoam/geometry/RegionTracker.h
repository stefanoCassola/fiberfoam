#pragma once

#include "common/Types.h"
#include <vector>
#include <map>

namespace fiberfoam
{

class RegionTracker
{
public:
    RegionTracker() = default;
    explicit RegionTracker(const std::vector<int8_t>& regionMask, int nx, int ny, int nz);

    CellRegion regionAt(int x, int y, int z) const;
    CellRegion regionForCell(int cellIndex) const;

    // Set cell index -> region mapping from cell map
    void buildFromCellMap(const std::map<int, CellData>& cellMap,
                          const std::vector<int8_t>& regionMask,
                          int nx, int ny, int nz);

    int countFibrousCells() const;
    int countBufferInletCells() const;
    int countBufferOutletCells() const;

    const std::map<int, CellRegion>& cellRegions() const { return cellRegions_; }

private:
    std::vector<int8_t> regionMask_;
    int nx_ = 0, ny_ = 0, nz_ = 0;
    std::map<int, CellRegion> cellRegions_;
};

} // namespace fiberfoam
