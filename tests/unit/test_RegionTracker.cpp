#include <gtest/gtest.h>
#include "geometry/RegionTracker.h"

using namespace fiberfoam;

class RegionTrackerTest : public ::testing::Test
{
protected:
    // Create a region mask for a 3x3x1 grid:
    // Row 0: BufferInlet, Row 1: Fibrous, Row 2: BufferOutlet (along x)
    std::vector<int8_t> make3x3x1Mask()
    {
        // 3x3x1 = 9 cells, indexed as data[x + 3*(y + 3*z)]
        std::vector<int8_t> mask(9);
        for (int y = 0; y < 3; ++y)
        {
            // x=0: inlet buffer
            mask[0 + 3 * y] = static_cast<int8_t>(CellRegion::BufferInlet);
            // x=1: fibrous
            mask[1 + 3 * y] = static_cast<int8_t>(CellRegion::Fibrous);
            // x=2: outlet buffer
            mask[2 + 3 * y] = static_cast<int8_t>(CellRegion::BufferOutlet);
        }
        return mask;
    }

    // Build a cell map where every voxel is fluid
    std::map<int, CellData> makeAllFluidCellMap(int nx, int ny, int nz)
    {
        std::map<int, CellData> cellMap;
        int idx = 0;
        for (int z = 0; z < nz; ++z)
        {
            for (int y = 0; y < ny; ++y)
            {
                for (int x = 0; x < nx; ++x)
                {
                    CellData cd;
                    cd.coord = {x, y, z};
                    cellMap[idx] = cd;
                    ++idx;
                }
            }
        }
        return cellMap;
    }
};

TEST_F(RegionTrackerTest, ConstructFromMask)
{
    auto mask = make3x3x1Mask();
    RegionTracker tracker(mask, 3, 3, 1);

    EXPECT_EQ(tracker.regionAt(0, 0, 0), CellRegion::BufferInlet);
    EXPECT_EQ(tracker.regionAt(1, 0, 0), CellRegion::Fibrous);
    EXPECT_EQ(tracker.regionAt(2, 0, 0), CellRegion::BufferOutlet);
}

TEST_F(RegionTrackerTest, CountFibrousCells)
{
    auto mask = make3x3x1Mask();
    auto cellMap = makeAllFluidCellMap(3, 3, 1);

    RegionTracker tracker;
    tracker.buildFromCellMap(cellMap, mask, 3, 3, 1);

    // 3 fibrous cells (x=1 for y=0,1,2)
    EXPECT_EQ(tracker.countFibrousCells(), 3);
}

TEST_F(RegionTrackerTest, CountBufferInletCells)
{
    auto mask = make3x3x1Mask();
    auto cellMap = makeAllFluidCellMap(3, 3, 1);

    RegionTracker tracker;
    tracker.buildFromCellMap(cellMap, mask, 3, 3, 1);

    // 3 inlet buffer cells (x=0 for y=0,1,2)
    EXPECT_EQ(tracker.countBufferInletCells(), 3);
}

TEST_F(RegionTrackerTest, CountBufferOutletCells)
{
    auto mask = make3x3x1Mask();
    auto cellMap = makeAllFluidCellMap(3, 3, 1);

    RegionTracker tracker;
    tracker.buildFromCellMap(cellMap, mask, 3, 3, 1);

    // 3 outlet buffer cells (x=2 for y=0,1,2)
    EXPECT_EQ(tracker.countBufferOutletCells(), 3);
}

TEST_F(RegionTrackerTest, TotalCountMatchesCellMap)
{
    auto mask = make3x3x1Mask();
    auto cellMap = makeAllFluidCellMap(3, 3, 1);

    RegionTracker tracker;
    tracker.buildFromCellMap(cellMap, mask, 3, 3, 1);

    int total = tracker.countFibrousCells() +
                tracker.countBufferInletCells() +
                tracker.countBufferOutletCells();

    EXPECT_EQ(total, static_cast<int>(cellMap.size()));
}

TEST_F(RegionTrackerTest, RegionForCellConsistent)
{
    auto mask = make3x3x1Mask();
    auto cellMap = makeAllFluidCellMap(3, 3, 1);

    RegionTracker tracker;
    tracker.buildFromCellMap(cellMap, mask, 3, 3, 1);

    // Verify each cell's region matches the mask
    for (const auto& [idx, cd] : cellMap)
    {
        int x = cd.coord[0], y = cd.coord[1], z = cd.coord[2];
        CellRegion expected = static_cast<CellRegion>(mask[x + 3 * (y + 3 * z)]);
        EXPECT_EQ(tracker.regionForCell(idx), expected)
            << "Mismatch at cell " << idx << " (" << x << "," << y << "," << z << ")";
    }
}

TEST_F(RegionTrackerTest, AllFibrousMask)
{
    // All cells are fibrous
    std::vector<int8_t> mask(8, static_cast<int8_t>(CellRegion::Fibrous));
    auto cellMap = makeAllFluidCellMap(2, 2, 2);

    RegionTracker tracker;
    tracker.buildFromCellMap(cellMap, mask, 2, 2, 2);

    EXPECT_EQ(tracker.countFibrousCells(), 8);
    EXPECT_EQ(tracker.countBufferInletCells(), 0);
    EXPECT_EQ(tracker.countBufferOutletCells(), 0);
}

TEST_F(RegionTrackerTest, DefaultConstructor)
{
    RegionTracker tracker;

    EXPECT_EQ(tracker.countFibrousCells(), 0);
    EXPECT_EQ(tracker.countBufferInletCells(), 0);
    EXPECT_EQ(tracker.countBufferOutletCells(), 0);
}

TEST_F(RegionTrackerTest, CellRegionsMapAccessible)
{
    auto mask = make3x3x1Mask();
    auto cellMap = makeAllFluidCellMap(3, 3, 1);

    RegionTracker tracker;
    tracker.buildFromCellMap(cellMap, mask, 3, 3, 1);

    const auto& regions = tracker.cellRegions();
    EXPECT_EQ(static_cast<int>(regions.size()), 9);
}
