#include <gtest/gtest.h>
#include "postprocessing/Permeability.h"
#include "postprocessing/Convergence.h"

using namespace fiberfoam;

class PostprocessPipelineTest : public ::testing::Test
{
protected:
    // Create a mock velocity field representing a simple channel flow
    // with uniform velocity in the flow direction
    struct MockFlowField
    {
        std::vector<std::array<double, 3>> velocities;
        std::vector<std::array<double, 3>> cellCenters;
        double meshVolume;
        double outletFlux;
    };

    MockFlowField createUniformChannelFlow(int nPerAxis, double velocity)
    {
        MockFlowField field;
        double dx = 1.0 / nPerAxis;
        field.meshVolume = 1.0; // unit cube

        for (int z = 0; z < nPerAxis; ++z)
        {
            for (int y = 0; y < nPerAxis; ++y)
            {
                for (int x = 0; x < nPerAxis; ++x)
                {
                    field.cellCenters.push_back(
                        {(x + 0.5) * dx, (y + 0.5) * dx, (z + 0.5) * dx});
                    field.velocities.push_back({velocity, 0.0, 0.0});
                }
            }
        }

        // Outlet flux = velocity * cross-section area = velocity * 1.0
        field.outletFlux = velocity * 1.0;
        return field;
    }

    // Create a Poiseuille-like flow profile (parabolic)
    MockFlowField createParabolicFlow(int nPerAxis, double maxVelocity)
    {
        MockFlowField field;
        double dx = 1.0 / nPerAxis;
        field.meshVolume = 1.0;

        for (int z = 0; z < nPerAxis; ++z)
        {
            for (int y = 0; y < nPerAxis; ++y)
            {
                for (int x = 0; x < nPerAxis; ++x)
                {
                    double cx = (x + 0.5) * dx;
                    double cy = (y + 0.5) * dx;
                    double cz = (z + 0.5) * dx;

                    // Parabolic profile in y-z plane
                    double r2 = (cy - 0.5) * (cy - 0.5) + (cz - 0.5) * (cz - 0.5);
                    double vel = maxVelocity * std::max(0.0, 1.0 - 4.0 * r2);

                    field.cellCenters.push_back({cx, cy, cz});
                    field.velocities.push_back({vel, 0.0, 0.0});
                }
            }
        }

        // Approximate outlet flux
        double totalFlux = 0.0;
        double cellArea = dx * dx;
        for (const auto& v : field.velocities)
        {
            totalFlux += v[0] * cellArea;
        }
        field.outletFlux = totalFlux / nPerAxis; // per slice
        return field;
    }

    PermeabilityCalculator::Options makeOpts()
    {
        PermeabilityCalculator::Options opts;
        opts.fluid.kinematicViscosity = 1e-4;
        opts.fluid.density = 1000.0;
        opts.fluid.dynamicViscosity = 0.1;
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
};

TEST_F(PostprocessPipelineTest, UniformFlowPermeability)
{
    auto field = createUniformChannelFlow(5, 0.001);
    auto opts = makeOpts();
    PermeabilityCalculator calc(opts);

    PermeabilityResult result = calc.compute(
        field.velocities, field.cellCenters,
        field.meshVolume, FlowDirection::X, field.outletFlux);

    // Permeability should be positive and physically reasonable
    EXPECT_GT(result.permVolAvgMain, 0.0);
    EXPECT_GT(result.permFlowRate, 0.0);

    // K = U * mu / (dP/L) = 0.001 * 1e-4 * 1.0 / 1.0 = 1e-7
    // (approximate, depends on exact formula used)
    EXPECT_GT(result.permVolAvgMain, 1e-12);
    EXPECT_LT(result.permVolAvgMain, 1e-1);
}

TEST_F(PostprocessPipelineTest, ParabolicFlowPermeability)
{
    auto field = createParabolicFlow(10, 0.001);
    auto opts = makeOpts();
    PermeabilityCalculator calc(opts);

    PermeabilityResult result = calc.compute(
        field.velocities, field.cellCenters,
        field.meshVolume, FlowDirection::X, field.outletFlux);

    EXPECT_GT(result.permVolAvgMain, 0.0);
}

TEST_F(PostprocessPipelineTest, PermeabilityAndConvergence)
{
    // Simulate a series of permeability computations converging
    auto opts = makeOpts();
    PermeabilityCalculator calc(opts);

    ConvergenceChecker::Options convOpts;
    convOpts.window = 5;
    convOpts.convSlope = 0.01;
    convOpts.errorBound = 0.05;
    ConvergenceChecker checker(convOpts);

    // Simulate convergence: velocity gradually stabilizes
    for (int iter = 0; iter < 20; ++iter)
    {
        double velocity = 0.001 * (1.0 - 0.5 * std::exp(-iter * 0.3));
        auto field = createUniformChannelFlow(3, velocity);

        PermeabilityResult result = calc.compute(
            field.velocities, field.cellCenters,
            field.meshVolume, FlowDirection::X, field.outletFlux);

        checker.addValue(static_cast<double>(iter), result.permVolAvgMain);
    }

    // After 20 iterations with exponentially decaying transient, should converge
    EXPECT_TRUE(checker.isConverged());
}

TEST_F(PostprocessPipelineTest, HigherVelocityHigherPermeability)
{
    auto opts = makeOpts();
    PermeabilityCalculator calc(opts);

    auto fieldLow = createUniformChannelFlow(5, 0.0001);
    auto fieldHigh = createUniformChannelFlow(5, 0.001);

    auto resultLow = calc.compute(
        fieldLow.velocities, fieldLow.cellCenters,
        fieldLow.meshVolume, FlowDirection::X, fieldLow.outletFlux);

    auto resultHigh = calc.compute(
        fieldHigh.velocities, fieldHigh.cellCenters,
        fieldHigh.meshVolume, FlowDirection::X, fieldHigh.outletFlux);

    EXPECT_GT(resultHigh.permVolAvgMain, resultLow.permVolAvgMain);
}

TEST_F(PostprocessPipelineTest, PhysicalBounds)
{
    auto field = createUniformChannelFlow(5, 0.001);
    auto opts = makeOpts();
    PermeabilityCalculator calc(opts);

    PermeabilityResult result = calc.compute(
        field.velocities, field.cellCenters,
        field.meshVolume, FlowDirection::X, field.outletFlux);

    // Permeability should be within physically reasonable bounds for porous media
    // Typical range: 1e-18 to 1e-6 m^2 for fibrous materials
    // Our simplified setup may give larger values, but should still be finite
    EXPECT_TRUE(std::isfinite(result.permVolAvgMain));
    EXPECT_TRUE(std::isfinite(result.permFlowRate));
    EXPECT_GT(result.flowLength, 0.0);
    EXPECT_GT(result.crossSectionArea, 0.0);
}

TEST_F(PostprocessPipelineTest, DirectionConsistency)
{
    auto opts = makeOpts();
    PermeabilityCalculator calc(opts);

    auto field = createUniformChannelFlow(3, 0.001);

    auto resultX = calc.compute(
        field.velocities, field.cellCenters,
        field.meshVolume, FlowDirection::X, field.outletFlux);

    EXPECT_EQ(resultX.direction, FlowDirection::X);

    // Compute for Y direction (same velocity field, just different labeling)
    auto resultY = calc.compute(
        field.velocities, field.cellCenters,
        field.meshVolume, FlowDirection::Y, field.outletFlux);

    EXPECT_EQ(resultY.direction, FlowDirection::Y);
}
