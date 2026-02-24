#include <gtest/gtest.h>
#include "geometry/VoxelArray.h"

using namespace fiberfoam;

TEST(VoxelArrayTest, ConstructAndAccess)
{
    std::vector<int8_t> data(27, 0);
    data[13] = 1; // center voxel at (1,1,1): index = 1 + 3*(1 + 3*1) = 13
    VoxelArray arr(data, 3, 3, 3);

    EXPECT_EQ(arr.nx(), 3);
    EXPECT_EQ(arr.ny(), 3);
    EXPECT_EQ(arr.nz(), 3);
    EXPECT_EQ(arr.size(), 27);
    EXPECT_EQ(arr.at(1, 1, 1), 1);
    EXPECT_EQ(arr.at(0, 0, 0), 0);
}

TEST(VoxelArrayTest, FluidFraction)
{
    std::vector<int8_t> data(27, 0);
    for (int i = 0; i < 9; i++)
        data[i] = 1;
    VoxelArray arr(data, 3, 3, 3);

    EXPECT_NEAR(arr.fluidFraction(), 9.0 / 27.0, 1e-10);
}

TEST(VoxelArrayTest, FluidFractionAllFluid)
{
    std::vector<int8_t> data(8, 1);
    VoxelArray arr(data, 2, 2, 2);

    EXPECT_NEAR(arr.fluidFraction(), 1.0, 1e-10);
}

TEST(VoxelArrayTest, FluidFractionAllSolid)
{
    std::vector<int8_t> data(8, 0);
    VoxelArray arr(data, 2, 2, 2);

    EXPECT_NEAR(arr.fluidFraction(), 0.0, 1e-10);
}

TEST(VoxelArrayTest, InvertConvention)
{
    std::vector<int8_t> data = {0, 1, 0, 1};
    VoxelArray arr(data, 2, 2, 1);

    arr.invertConvention();

    EXPECT_EQ(arr.at(0, 0, 0), 1);
    EXPECT_EQ(arr.at(1, 0, 0), 0);
    EXPECT_EQ(arr.at(0, 1, 0), 1);
    EXPECT_EQ(arr.at(1, 1, 0), 0);
}

TEST(VoxelArrayTest, InvertConventionTwiceRestoresOriginal)
{
    std::vector<int8_t> data = {0, 1, 1, 0, 1, 0, 0, 1};
    VoxelArray arr(data, 2, 2, 2);
    std::vector<int8_t> original = arr.data();

    arr.invertConvention();
    arr.invertConvention();

    EXPECT_EQ(arr.data(), original);
}

TEST(VoxelArrayTest, Resample)
{
    std::vector<int8_t> data(8, 1); // 2x2x2 all fluid
    VoxelArray arr(data, 2, 2, 2);

    VoxelArray resampled = arr.resample(4);

    EXPECT_EQ(resampled.nx(), 4);
    EXPECT_EQ(resampled.ny(), 4);
    EXPECT_EQ(resampled.nz(), 4);
    EXPECT_EQ(resampled.size(), 64);
    // All should still be 1 (nearest neighbor of all-1 grid)
    EXPECT_EQ(resampled.at(0, 0, 0), 1);
    EXPECT_EQ(resampled.at(3, 3, 3), 1);
}

TEST(VoxelArrayTest, ResampleDownsample)
{
    // 4x4x4 all fluid, resample down to 2
    std::vector<int8_t> data(64, 1);
    VoxelArray arr(data, 4, 4, 4);

    VoxelArray resampled = arr.resample(2);

    EXPECT_EQ(resampled.nx(), 2);
    EXPECT_EQ(resampled.ny(), 2);
    EXPECT_EQ(resampled.nz(), 2);
    // All should still be 1
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
                EXPECT_EQ(resampled.at(x, y, z), 1);
}

TEST(VoxelArrayTest, ResampleSameResolution)
{
    std::vector<int8_t> data = {1, 0, 0, 1, 1, 0, 0, 1};
    VoxelArray arr(data, 2, 2, 2);

    VoxelArray resampled = arr.resample(2);

    EXPECT_EQ(resampled.nx(), 2);
    EXPECT_EQ(resampled.data(), data);
}

TEST(VoxelArrayTest, DefaultConstructor)
{
    VoxelArray arr;

    EXPECT_EQ(arr.nx(), 0);
    EXPECT_EQ(arr.ny(), 0);
    EXPECT_EQ(arr.nz(), 0);
    EXPECT_EQ(arr.size(), 0);
}

TEST(VoxelArrayTest, FromDatFile)
{
    // This relies on the fixture file being available at the test working directory
    std::string path = "fixtures/geometry_3x3x3.dat";
    VoxelArray arr = VoxelArray::fromDatFile(path, 3);

    EXPECT_EQ(arr.nx(), 3);
    EXPECT_EQ(arr.ny(), 3);
    EXPECT_EQ(arr.nz(), 3);
    EXPECT_EQ(arr.size(), 27);
}

TEST(VoxelArrayTest, MutableAccess)
{
    std::vector<int8_t> data(8, 0);
    VoxelArray arr(data, 2, 2, 2);

    arr.at(1, 1, 1) = 1;

    EXPECT_EQ(arr.at(1, 1, 1), 1);
    EXPECT_EQ(arr.at(0, 0, 0), 0);
}
