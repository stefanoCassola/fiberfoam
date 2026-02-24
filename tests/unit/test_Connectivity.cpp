#include <gtest/gtest.h>
#include "mesh/Connectivity.h"
#include "geometry/VoxelArray.h"

using namespace fiberfoam;

class ConnectivityTest : public ::testing::Test
{
protected:
    // Build a cell map from a VoxelArray (fluid cells only)
    std::map<int, CellData> buildCellMap(const VoxelArray& geom)
    {
        std::map<int, CellData> cellMap;
        int cellIndex = 0;
        for (int z = 0; z < geom.nz(); ++z)
        {
            for (int y = 0; y < geom.ny(); ++y)
            {
                for (int x = 0; x < geom.nx(); ++x)
                {
                    if (geom.at(x, y, z) == 1)
                    {
                        CellData cd;
                        cd.coord = {x, y, z};
                        cellMap[cellIndex] = cd;
                        ++cellIndex;
                    }
                }
            }
        }
        return cellMap;
    }
};

TEST_F(ConnectivityTest, SingleComponent)
{
    // 2x2x2 all fluid - single connected component
    std::vector<int8_t> data(8, 1);
    VoxelArray geom(data, 2, 2, 2);
    auto cellMap = buildCellMap(geom);

    auto largest = findLargestComponent(cellMap, geom);

    // All 8 cells should be in the largest component
    EXPECT_EQ(static_cast<int>(largest.size()), 8);
}

TEST_F(ConnectivityTest, DisconnectedComponents)
{
    // 5x1x1 geometry: fluid - solid - fluid - fluid - fluid
    // This creates two disconnected components: {0} and {2,3,4}
    std::vector<int8_t> data = {1, 0, 1, 1, 1};
    VoxelArray geom(data, 5, 1, 1);
    auto cellMap = buildCellMap(geom);

    auto largest = findLargestComponent(cellMap, geom);

    // The largest component has 3 cells (indices 2,3,4 in the voxel grid)
    EXPECT_EQ(static_cast<int>(largest.size()), 3);
}

TEST_F(ConnectivityTest, FilterCellMap)
{
    // 5x1x1 with disconnected components
    std::vector<int8_t> data = {1, 0, 1, 1, 1};
    VoxelArray geom(data, 5, 1, 1);
    auto cellMap = buildCellMap(geom);

    auto keepSet = findLargestComponent(cellMap, geom);
    auto filtered = filterCellMap(cellMap, keepSet);

    // Should only contain cells from the largest component
    EXPECT_EQ(static_cast<int>(filtered.size()), 3);

    // Verify re-indexing: filtered map should have indices 0..2
    for (const auto& [idx, cd] : filtered)
    {
        EXPECT_GE(idx, 0);
        EXPECT_LT(idx, 3);
    }
}

TEST_F(ConnectivityTest, SingleFluidCell)
{
    // 3x1x1: solid - fluid - solid
    std::vector<int8_t> data = {0, 1, 0};
    VoxelArray geom(data, 3, 1, 1);
    auto cellMap = buildCellMap(geom);

    auto largest = findLargestComponent(cellMap, geom);

    EXPECT_EQ(static_cast<int>(largest.size()), 1);
}

TEST_F(ConnectivityTest, AllSolidReturnsEmpty)
{
    std::vector<int8_t> data(8, 0);
    VoxelArray geom(data, 2, 2, 2);
    auto cellMap = buildCellMap(geom);

    // Empty cell map
    EXPECT_TRUE(cellMap.empty());
    auto largest = findLargestComponent(cellMap, geom);
    EXPECT_TRUE(largest.empty());
}

TEST_F(ConnectivityTest, ThreeDimensionalConnectivity)
{
    // 3x3x3 with two disconnected regions:
    // One fluid cell at (0,0,0) and a block at (2,2,2),(2,1,2),(2,2,1)
    std::vector<int8_t> data(27, 0);
    // Region 1: single cell at corner
    data[0 + 3 * (0 + 3 * 0)] = 1; // (0,0,0)
    // Region 2: three connected cells
    data[2 + 3 * (2 + 3 * 2)] = 1; // (2,2,2)
    data[2 + 3 * (1 + 3 * 2)] = 1; // (2,1,2)
    data[2 + 3 * (2 + 3 * 1)] = 1; // (2,2,1)

    VoxelArray geom(data, 3, 3, 3);
    auto cellMap = buildCellMap(geom);

    auto largest = findLargestComponent(cellMap, geom);

    // The larger component has 3 cells
    EXPECT_EQ(static_cast<int>(largest.size()), 3);
}

TEST_F(ConnectivityTest, FilterPreservesCoordinates)
{
    std::vector<int8_t> data = {1, 0, 1, 1, 1};
    VoxelArray geom(data, 5, 1, 1);
    auto cellMap = buildCellMap(geom);

    auto keepSet = findLargestComponent(cellMap, geom);
    auto filtered = filterCellMap(cellMap, keepSet);

    // Verify coordinates are preserved (should be x=2, x=3, x=4)
    bool foundX2 = false, foundX3 = false, foundX4 = false;
    for (const auto& [idx, cd] : filtered)
    {
        if (cd.coord[0] == 2) foundX2 = true;
        if (cd.coord[0] == 3) foundX3 = true;
        if (cd.coord[0] == 4) foundX4 = true;
    }
    EXPECT_TRUE(foundX2);
    EXPECT_TRUE(foundX3);
    EXPECT_TRUE(foundX4);
}
