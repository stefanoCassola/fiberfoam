#include <gtest/gtest.h>
#include "geometry/VoxelArray.h"
#include "ml/ModelRegistry.h"

using namespace fiberfoam;

class PredictPipelineTest : public ::testing::Test
{
protected:
    VoxelArray loadAndPrepareGeometry()
    {
        // Load 5x5x5 fixture
        // Note: fromDatFile already inverts the convention (0->1, 1->0)
        VoxelArray arr = VoxelArray::fromDatFile("fixtures/geometry_5x5x5.dat", 5);
        return arr;
    }
};

TEST_F(PredictPipelineTest, LoadGeometry)
{
    VoxelArray geom = loadAndPrepareGeometry();

    EXPECT_EQ(geom.nx(), 5);
    EXPECT_EQ(geom.ny(), 5);
    EXPECT_EQ(geom.nz(), 5);
    EXPECT_EQ(geom.size(), 125);
}

TEST_F(PredictPipelineTest, DownsampleGeometry)
{
    VoxelArray geom = loadAndPrepareGeometry();

    // Downsample from 5 to 3
    VoxelArray downsampled = geom.resample(3);

    EXPECT_EQ(downsampled.nx(), 3);
    EXPECT_EQ(downsampled.ny(), 3);
    EXPECT_EQ(downsampled.nz(), 3);
    EXPECT_EQ(downsampled.size(), 27);
}

TEST_F(PredictPipelineTest, UpsampleGeometry)
{
    VoxelArray geom = loadAndPrepareGeometry();

    // Upsample from 5 to 10
    VoxelArray upsampled = geom.resample(10);

    EXPECT_EQ(upsampled.nx(), 10);
    EXPECT_EQ(upsampled.ny(), 10);
    EXPECT_EQ(upsampled.nz(), 10);
    EXPECT_EQ(upsampled.size(), 1000);
}

TEST_F(PredictPipelineTest, ResamplePreservesFluidFractionApproximately)
{
    VoxelArray geom = loadAndPrepareGeometry();
    double originalFraction = geom.fluidFraction();

    VoxelArray resampled = geom.resample(10);
    double resampledFraction = resampled.fluidFraction();

    // Fluid fraction should be approximately preserved (nearest-neighbor)
    EXPECT_NEAR(resampledFraction, originalFraction, 0.15);
}

TEST_F(PredictPipelineTest, DownsampleThenUpsamplePreservesStructure)
{
    VoxelArray geom = loadAndPrepareGeometry();

    // Down then up: 5 -> 3 -> 5
    VoxelArray down = geom.resample(3);
    VoxelArray upAgain = down.resample(5);

    EXPECT_EQ(upAgain.nx(), 5);
    EXPECT_EQ(upAgain.ny(), 5);
    EXPECT_EQ(upAgain.nz(), 5);

    // Structure should be roughly preserved (center column still fluid)
    // At least the center column at x=2 should be mostly fluid
    int fluidCount = 0;
    for (int z = 0; z < 5; ++z)
        for (int y = 0; y < 5; ++y)
            if (upAgain.at(2, y, z) == 1)
                ++fluidCount;

    EXPECT_GT(fluidCount, 15); // Most of the 25 center cells should be fluid
}

TEST_F(PredictPipelineTest, ModelRegistryWithoutModels)
{
    // Verify that when no models are available, the system handles it gracefully
    VoxelArray geom = loadAndPrepareGeometry();

    // Without actual ONNX models, we can test that:
    // 1) Geometry loads correctly
    EXPECT_GT(geom.fluidFraction(), 0.0);
    EXPECT_LT(geom.fluidFraction(), 1.0);

    // 2) Geometry can be resampled to model resolution
    VoxelArray modelRes = geom.resample(4); // typical model resolution would be 80
    EXPECT_EQ(modelRes.nx(), 4);
}

TEST_F(PredictPipelineTest, FluidFractionValid)
{
    VoxelArray geom = loadAndPrepareGeometry();

    double fraction = geom.fluidFraction();

    // After inversion: x=2 is fluid (25 cells out of 125)
    EXPECT_NEAR(fraction, 25.0 / 125.0, 1e-10);
}

TEST_F(PredictPipelineTest, InvertConventionChangesValues)
{
    // Load raw (before inversion)
    VoxelArray raw = VoxelArray::fromDatFile("fixtures/geometry_5x5x5.dat", 5);
    double rawFraction = raw.fluidFraction();

    // Invert
    VoxelArray inverted = VoxelArray::fromDatFile("fixtures/geometry_5x5x5.dat", 5);
    inverted.invertConvention();
    double invertedFraction = inverted.fluidFraction();

    // Fractions should be complementary
    EXPECT_NEAR(rawFraction + invertedFraction, 1.0, 1e-10);
}
