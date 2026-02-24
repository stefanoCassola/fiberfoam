#pragma once

#include "common/Types.h"
#include "geometry/VoxelArray.h"
#include <map>
#include <set>

namespace fiberfoam
{

// BFS-based 6-connected component analysis
std::set<int> findLargestComponent(
    const std::map<int, CellData>& cellMap,
    const VoxelArray& geometry);

// Remove cells not in keepSet, reindex from 0
std::map<int, CellData> filterCellMap(
    const std::map<int, CellData>& cellMap,
    const std::set<int>& keepSet);

} // namespace fiberfoam
