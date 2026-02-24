#include <gtest/gtest.h>
#include "postprocessing/Permeability.h"

using namespace fiberfoam;

class PermeabilityTest : public ::testing::Test
{
protected:
    PermeabilityCalculator::Options makeSimpleOpts()
    {
        PermeabilityCalculator::Options opts;
        opts.fluid.kinematicViscosity = 1.0;
        opts.fluid.density = 1.0;
        opts.fluid.dynamicViscosity = 1.0;
        opts.fluid.pressureInlet = 1.0;
        opts.fluid.pressureOutlet = 0.0;
        opts.fibrousRegionOnly = false;
        opts.meshMinMain = 0.0;
        opts.meshMaxMain = 1.0;
        opts.meshMinSec = 0.0;
        opts.meshMaxSec = 1.0;
        opts.meshMinTert = 0.0;
        opts.meshMaxTert = 1.0;
        opts.inletLength = 0.0;
        opts.outletLength = 0.0;
        opts.scale = 1.0;
        return opts;
    }

    // Create a uniform velocity field: all cells with U=(ux, 0, 0)
    std::vector<std::array<double, 3>> makeUniformVelocity(int nCells, double ux)
    {
        std::vector<std::array<double, 3>> vel(nCells, {ux, 0.0, 0.0});
        return vel;
    }

    // Create uniform cell centers in a unit cube
    std::vector<std::array<double, 3>> makeUniformCenters(int nPerAxis)
    {
        std::vector<std::array<double, 3>> centers;
        double dx = 1.0 / nPerAxis;
        for (int z = 0; z < nPerAxis; ++z)
        {
            for (int y = 0; y < nPerAxis; ++y)
            {
                for (int x = 0; x < nPerAxis; ++x)
                {
                    centers.push_back({(x + 0.5) * dx, (y + 0.5) * dx, (z + 0.5) * dx});
                }
            }
        }
        return centers;
    }
};

TEST_F(PermeabilityTest, UniformFlowXDirection)
{
    // For uniform U=(1,0,0) in a unit cube with nu=1, density=1, dP=1:
    // K_vol_avg = U_avg * nu * L / dP = 1 * 1 * 1 / 1 = 1
    auto opts = makeSimpleOpts();
    PermeabilityCalculator calc(opts);

    int nPerAxis = 5;
    int nCells = nPerAxis * nPerAxis * nPerAxis;
    auto velocities = makeUniformVelocity(nCells, 1.0);
    auto centers = makeUniformCenters(nPerAxis);
    double meshVolume = 1.0;

    PermeabilityResult result =
        calc.compute(velocities, centers, meshVolume, FlowDirection::X, 1.0);

    EXPECT_EQ(result.direction, FlowDirection::X);
    EXPECT_GT(result.permVolAvgMain, 0.0);
}

TEST_F(PermeabilityTest, ZeroFlowGivesZeroPermeability)
{
    auto opts = makeSimpleOpts();
    PermeabilityCalculator calc(opts);

    int nPerAxis = 3;
    int nCells = nPerAxis * nPerAxis * nPerAxis;
    auto velocities = makeUniformVelocity(nCells, 0.0);
    auto centers = makeUniformCenters(nPerAxis);

    PermeabilityResult result =
        calc.compute(velocities, centers, 1.0, FlowDirection::X, 0.0);

    EXPECT_NEAR(result.permVolAvgMain, 0.0, 1e-15);
}

TEST_F(PermeabilityTest, DirectionStored)
{
    auto opts = makeSimpleOpts();
    PermeabilityCalculator calc(opts);

    int nCells = 27;
    auto velocities = makeUniformVelocity(nCells, 1.0);
    auto centers = makeUniformCenters(3);

    PermeabilityResult resultX =
        calc.compute(velocities, centers, 1.0, FlowDirection::X, 1.0);
    EXPECT_EQ(resultX.direction, FlowDirection::X);
}

TEST_F(PermeabilityTest, PermeabilityScalesWithVelocity)
{
    auto opts = makeSimpleOpts();
    PermeabilityCalculator calc(opts);

    int nCells = 27;
    auto centers = makeUniformCenters(3);

    auto vel1 = makeUniformVelocity(nCells, 1.0);
    auto vel2 = makeUniformVelocity(nCells, 2.0);

    auto r1 = calc.compute(vel1, centers, 1.0, FlowDirection::X, 1.0);
    auto r2 = calc.compute(vel2, centers, 1.0, FlowDirection::X, 2.0);

    // Double velocity => double volume-averaged permeability
    if (r1.permVolAvgMain > 0)
    {
        EXPECT_NEAR(r2.permVolAvgMain / r1.permVolAvgMain, 2.0, 0.1);
    }
}

TEST_F(PermeabilityTest, FlowRateMethod)
{
    auto opts = makeSimpleOpts();
    PermeabilityCalculator calc(opts);

    int nCells = 27;
    auto velocities = makeUniformVelocity(nCells, 1.0);
    auto centers = makeUniformCenters(3);

    PermeabilityResult result =
        calc.compute(velocities, centers, 1.0, FlowDirection::X, 1.0);

    // Flow rate permeability should also be computed
    EXPECT_GT(result.permFlowRate, 0.0);
}

TEST_F(PermeabilityTest, CrossSectionAreaPositive)
{
    auto opts = makeSimpleOpts();
    PermeabilityCalculator calc(opts);

    int nCells = 27;
    auto velocities = makeUniformVelocity(nCells, 1.0);
    auto centers = makeUniformCenters(3);

    PermeabilityResult result =
        calc.compute(velocities, centers, 1.0, FlowDirection::X, 1.0);

    EXPECT_GT(result.crossSectionArea, 0.0);
}

TEST_F(PermeabilityTest, FlowLengthPositive)
{
    auto opts = makeSimpleOpts();
    PermeabilityCalculator calc(opts);

    int nCells = 27;
    auto velocities = makeUniformVelocity(nCells, 1.0);
    auto centers = makeUniformCenters(3);

    PermeabilityResult result =
        calc.compute(velocities, centers, 1.0, FlowDirection::X, 1.0);

    EXPECT_GT(result.flowLength, 0.0);
}
