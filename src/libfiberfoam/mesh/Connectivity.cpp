#include "mesh/Connectivity.h"
#include "common/Logger.h"

#include <queue>
#include <unordered_map>
#include <tuple>

namespace fiberfoam
{

namespace
{

// Hash for VoxelCoord (std::array<int,3>)
struct VoxelCoordHash
{
    std::size_t operator()(const VoxelCoord& c) const
    {
        // Simple spatial hash combining the three ints
        std::size_t h = 0;
        h ^= std::hash<int>()(c[0]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(c[1]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(c[2]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// 6-connected neighbor offsets
constexpr int NEIGHBOR_OFFSETS[6][3] = {
    {-1, 0, 0}, {1, 0, 0},
    {0, -1, 0}, {0, 1, 0},
    {0, 0, -1}, {0, 0, 1}
};

} // anonymous namespace

std::set<int> findLargestComponent(
    const std::map<int, CellData>& cellMap,
    const VoxelArray& geometry)
{
    if (cellMap.empty())
        return {};

    Logger::info("Building adjacency graph for connectivity check...");

    // Build coord -> cell index mapping
    std::unordered_map<VoxelCoord, int, VoxelCoordHash> coordToIndex;
    coordToIndex.reserve(cellMap.size());
    for (const auto& [idx, cell] : cellMap)
    {
        coordToIndex[cell.coord] = idx;
    }

    // Build adjacency list
    std::unordered_map<int, std::vector<int>> adjacency;
    adjacency.reserve(cellMap.size());
    for (const auto& [idx, cell] : cellMap)
    {
        auto& neighbors = adjacency[idx];
        for (const auto& off : NEIGHBOR_OFFSETS)
        {
            VoxelCoord neighborCoord = {
                cell.coord[0] + off[0],
                cell.coord[1] + off[1],
                cell.coord[2] + off[2]
            };
            auto it = coordToIndex.find(neighborCoord);
            if (it != coordToIndex.end())
            {
                neighbors.push_back(it->second);
            }
        }
    }

    // BFS to find all connected components
    Logger::info("Finding largest connected component via BFS...");
    std::unordered_map<int, bool> visited;
    visited.reserve(cellMap.size());
    for (const auto& [idx, _] : cellMap)
    {
        visited[idx] = false;
    }

    std::set<int> largestComponent;

    for (const auto& [startIdx, _] : cellMap)
    {
        if (visited[startIdx])
            continue;

        // BFS from startIdx
        std::set<int> component;
        std::queue<int> queue;
        queue.push(startIdx);
        visited[startIdx] = true;

        while (!queue.empty())
        {
            int current = queue.front();
            queue.pop();
            component.insert(current);

            auto adjIt = adjacency.find(current);
            if (adjIt != adjacency.end())
            {
                for (int neighbor : adjIt->second)
                {
                    if (!visited[neighbor])
                    {
                        visited[neighbor] = true;
                        queue.push(neighbor);
                    }
                }
            }
        }

        if (component.size() > largestComponent.size())
        {
            largestComponent = std::move(component);
        }
    }

    Logger::info("Largest connected component has "
                 + std::to_string(largestComponent.size()) + " of "
                 + std::to_string(cellMap.size()) + " cells");

    return largestComponent;
}

std::map<int, CellData> filterCellMap(
    const std::map<int, CellData>& cellMap,
    const std::set<int>& keepSet)
{
    std::map<int, CellData> filtered;
    int newIndex = 0;
    for (const auto& [idx, cell] : cellMap)
    {
        if (keepSet.count(idx))
        {
            filtered[newIndex] = cell;
            ++newIndex;
        }
    }

    Logger::info("Filtered cell map: " + std::to_string(filtered.size())
                 + " cells (re-indexed from 0)");
    return filtered;
}

} // namespace fiberfoam
