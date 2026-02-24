#include <gtest/gtest.h>
#include "geometry/FiberFreeRegion.h"
#include "geometry/VoxelArray.h"

using namespace fiberfoam;

class FiberFreeRegionTest : public ::testing::Test
{
protected:
    // Create a 3x3x3 all-fluid geometry
    VoxelArray makeAllFluid3x3x3()
    {
        std::vector<int8_t> data(27, 1);
        return VoxelArray(data, 3, 3, 3);
    }

    // Create a 3x3x3 geometry with center column as fluid
    VoxelArray makeCenterColumn3x3x3()
    {
        std::vector<int8_t> data(27, 0);
        // Center column: x=1 for all y,z
        for (int z = 0; z < 3; ++z)
            for (int y = 0; y < 3; ++y)
                data[1 + 3 * (y + 3 * z)] = 1;
        return VoxelArray(data, 3, 3, 3);
    }
};

TEST_F(FiberFreeRegionTest, PadXDirection)
{
    VoxelArray geom = makeAllFluid3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 2, 2);

    // Original 3x3x3 padded with 2 inlet and 2 outlet along X: (3+2+2)x3x3 = 7x3x3
    EXPECT_EQ(padded.geometry.nx(), 7);
    EXPECT_EQ(padded.geometry.ny(), 3);
    EXPECT_EQ(padded.geometry.nz(), 3);
}

TEST_F(FiberFreeRegionTest, PadYDirection)
{
    VoxelArray geom = makeAllFluid3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::Y, 2, 2);

    // Padded along Y: 3x(3+2+2)x3 = 3x7x3
    EXPECT_EQ(padded.geometry.nx(), 3);
    EXPECT_EQ(padded.geometry.ny(), 7);
    EXPECT_EQ(padded.geometry.nz(), 3);
}

TEST_F(FiberFreeRegionTest, PadZDirection)
{
    VoxelArray geom = makeAllFluid3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::Z, 2, 2);

    // Padded along Z: 3x3x(3+2+2) = 3x3x7
    EXPECT_EQ(padded.geometry.nx(), 3);
    EXPECT_EQ(padded.geometry.ny(), 3);
    EXPECT_EQ(padded.geometry.nz(), 7);
}

TEST_F(FiberFreeRegionTest, RegionMaskSize)
{
    VoxelArray geom = makeAllFluid3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 2, 2);

    // Region mask should be the same size as the padded geometry
    EXPECT_EQ(static_cast<int>(padded.regionMask.size()), padded.geometry.size());
}

TEST_F(FiberFreeRegionTest, RegionMaskValues)
{
    VoxelArray geom = makeAllFluid3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 2, 2);

    int nx = padded.geometry.nx();
    int ny = padded.geometry.ny();
    int nz = padded.geometry.nz();

    // Check that inlet layers (x=0,1) are marked as BufferInlet
    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                int idx = x + nx * (y + ny * z);
                EXPECT_EQ(padded.regionMask[idx],
                          static_cast<int8_t>(CellRegion::BufferInlet))
                    << "Inlet region mismatch at (" << x << "," << y << "," << z << ")";
            }
        }
    }

    // Check that outlet layers (x=5,6) are marked as BufferOutlet
    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            for (int x = nx - 2; x < nx; ++x)
            {
                int idx = x + nx * (y + ny * z);
                EXPECT_EQ(padded.regionMask[idx],
                          static_cast<int8_t>(CellRegion::BufferOutlet))
                    << "Outlet region mismatch at (" << x << "," << y << "," << z << ")";
            }
        }
    }

    // Check that fibrous layers (x=2,3,4) are marked as Fibrous
    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 2; x < 5; ++x)
            {
                int idx = x + nx * (y + ny * z);
                EXPECT_EQ(padded.regionMask[idx],
                          static_cast<int8_t>(CellRegion::Fibrous))
                    << "Fibrous region mismatch at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

TEST_F(FiberFreeRegionTest, PaddedLayersAreFluid)
{
    VoxelArray geom = makeCenterColumn3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 2, 2);

    int nx = padded.geometry.nx();
    int ny = padded.geometry.ny();
    int nz = padded.geometry.nz();

    // Inlet buffer layers should be all fluid (1)
    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                EXPECT_EQ(padded.geometry.at(x, y, z), 1)
                    << "Buffer inlet not fluid at (" << x << "," << y << "," << z << ")";
            }
        }
    }

    // Outlet buffer layers should be all fluid (1)
    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            for (int x = nx - 2; x < nx; ++x)
            {
                EXPECT_EQ(padded.geometry.at(x, y, z), 1)
                    << "Buffer outlet not fluid at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

TEST_F(FiberFreeRegionTest, NoPaddingReturnsOriginalSize)
{
    VoxelArray geom = makeAllFluid3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 0, 0);

    EXPECT_EQ(padded.geometry.nx(), 3);
    EXPECT_EQ(padded.geometry.ny(), 3);
    EXPECT_EQ(padded.geometry.nz(), 3);
}

TEST_F(FiberFreeRegionTest, FibrousExtent)
{
    VoxelArray geom = makeAllFluid3x3x3();
    double voxelSize = 1e-6; // 1 micron
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 2, 2);

    auto [start, end] = FiberFreeRegion::fibrousExtent(padded, FlowDirection::X, voxelSize);

    // Fibrous region starts at layer 2 and ends at layer 4 (original 3 layers)
    // Extent = 3 * voxelSize = 3e-6
    double fibrousLength = end - start;
    EXPECT_NEAR(fibrousLength, 3.0 * voxelSize, 1e-12);
}

TEST_F(FiberFreeRegionTest, AsymmetricPadding)
{
    VoxelArray geom = makeAllFluid3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 1, 3);

    // 3 + 1 + 3 = 7 along X
    EXPECT_EQ(padded.geometry.nx(), 7);
    EXPECT_EQ(padded.geometry.ny(), 3);
    EXPECT_EQ(padded.geometry.nz(), 3);
}
