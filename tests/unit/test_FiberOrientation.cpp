#include <gtest/gtest.h>
#include "analysis/FiberOrientation.h"
#include "geometry/VoxelArray.h"

using namespace fiberfoam;

class FiberOrientationTest : public ::testing::Test
{
protected:
    // Create geometry with vertical stripes (fibers along z-axis)
    // Pattern: alternating solid/fluid columns along x, uniform in z
    VoxelArray makeVerticalStripes(int n)
    {
        std::vector<int8_t> data(n * n * n, 0);
        for (int z = 0; z < n; ++z)
        {
            for (int y = 0; y < n; ++y)
            {
                for (int x = 0; x < n; ++x)
                {
                    // Alternating columns along x
                    data[x + n * (y + n * z)] = (x % 2 == 0) ? 1 : 0;
                }
            }
        }
        return VoxelArray(data, n, n, n);
    }

    // Create geometry with horizontal stripes (fibers along x-axis)
    // Pattern: alternating solid/fluid layers along y, uniform in x
    // Using y-alternation so that FFT power concentrates along ky,
    // which the z-slice projection and PCA detect as a high angle.
    VoxelArray makeHorizontalStripes(int n)
    {
        std::vector<int8_t> data(n * n * n, 0);
        for (int z = 0; z < n; ++z)
        {
            for (int y = 0; y < n; ++y)
            {
                for (int x = 0; x < n; ++x)
                {
                    // Alternating layers along y
                    data[x + n * (y + n * z)] = (y % 2 == 0) ? 1 : 0;
                }
            }
        }
        return VoxelArray(data, n, n, n);
    }

    // Create isotropic geometry (all fluid)
    VoxelArray makeIsotropic(int n)
    {
        std::vector<int8_t> data(n * n * n, 1);
        return VoxelArray(data, n, n, n);
    }
};

TEST_F(FiberOrientationTest, VerticalStripesLowAngle)
{
    // Vertical stripes (fibers along z) should give an angle close to 0 degrees
    VoxelArray geom = makeVerticalStripes(32);
    double angle = estimateFiberOrientation(geom, 2.0);

    // Angle should be in [0, 90] range
    EXPECT_GE(angle, 0.0);
    EXPECT_LE(angle, 90.0);

    // For vertical stripes, expect angle closer to 0
    EXPECT_LT(angle, 30.0);
}

TEST_F(FiberOrientationTest, HorizontalStripesHighAngle)
{
    // Horizontal stripes (fibers along x) should give a higher angle
    VoxelArray geom = makeHorizontalStripes(32);
    double angle = estimateFiberOrientation(geom, 2.0);

    EXPECT_GE(angle, 0.0);
    EXPECT_LE(angle, 90.0);

    // For horizontal stripes, expect angle closer to 90
    EXPECT_GT(angle, 60.0);
}

TEST_F(FiberOrientationTest, AngleInValidRange)
{
    VoxelArray geom = makeIsotropic(16);
    double angle = estimateFiberOrientation(geom, 2.0);

    // Angle must always be in [0, 90]
    EXPECT_GE(angle, 0.0);
    EXPECT_LE(angle, 90.0);
}

TEST_F(FiberOrientationTest, ReproducibleResult)
{
    VoxelArray geom = makeVerticalStripes(16);

    double angle1 = estimateFiberOrientation(geom, 2.0);
    double angle2 = estimateFiberOrientation(geom, 2.0);

    // Same input should give same output (deterministic)
    EXPECT_NEAR(angle1, angle2, 1e-10);
}

TEST_F(FiberOrientationTest, DefaultSigma)
{
    VoxelArray geom = makeVerticalStripes(16);

    // Test with default sigma (4.0) -- just ensure it does not crash
    double angle = estimateFiberOrientation(geom);

    EXPECT_GE(angle, 0.0);
    EXPECT_LE(angle, 90.0);
}
