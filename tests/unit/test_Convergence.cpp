#include <gtest/gtest.h>
#include "postprocessing/Convergence.h"

using namespace fiberfoam;

class ConvergenceTest : public ::testing::Test
{
protected:
    ConvergenceChecker::Options defaultOpts()
    {
        ConvergenceChecker::Options opts;
        opts.window = 5;
        opts.convSlope = 0.01;
        opts.errorBound = 0.01;
        return opts;
    }
};

TEST_F(ConvergenceTest, ConstantValuesConvergeImmediately)
{
    auto opts = defaultOpts();
    ConvergenceChecker checker(opts);

    // Add constant permeability values (slope = 0)
    for (int i = 0; i < 10; ++i)
    {
        checker.addValue(static_cast<double>(i), 1.0e-10);
    }

    EXPECT_TRUE(checker.isConverged());
    EXPECT_NEAR(checker.currentSlope(), 0.0, opts.convSlope);
}

TEST_F(ConvergenceTest, LinearlyIncreasingDoesNotConverge)
{
    auto opts = defaultOpts();
    opts.convSlope = 0.001; // strict slope threshold
    ConvergenceChecker checker(opts);

    // Add linearly increasing values with significant slope
    for (int i = 0; i < 10; ++i)
    {
        checker.addValue(static_cast<double>(i), 1.0 + i * 0.1);
    }

    EXPECT_FALSE(checker.isConverged());
}

TEST_F(ConvergenceTest, TooFewValuesNotConverged)
{
    auto opts = defaultOpts();
    opts.window = 10;
    ConvergenceChecker checker(opts);

    // Add fewer values than window size
    for (int i = 0; i < 5; ++i)
    {
        checker.addValue(static_cast<double>(i), 1.0);
    }

    // Should not be converged with fewer values than the window
    EXPECT_FALSE(checker.isConverged());
}

TEST_F(ConvergenceTest, ConvergesAfterTransient)
{
    auto opts = defaultOpts();
    opts.window = 5;
    opts.convSlope = 0.01;
    opts.errorBound = 0.05;
    ConvergenceChecker checker(opts);

    // Transient phase: linearly increasing
    for (int i = 0; i < 20; ++i)
    {
        checker.addValue(static_cast<double>(i), 0.5 + i * 0.01);
    }

    // Steady phase: constant
    for (int i = 20; i < 40; ++i)
    {
        checker.addValue(static_cast<double>(i), 0.7);
    }

    EXPECT_TRUE(checker.isConverged());
}

TEST_F(ConvergenceTest, PredictedPermeabilityCloseToActual)
{
    auto opts = defaultOpts();
    opts.window = 10;
    ConvergenceChecker checker(opts);

    double target = 2.5e-10;
    for (int i = 0; i < 20; ++i)
    {
        checker.addValue(static_cast<double>(i), target);
    }

    EXPECT_NEAR(checker.predictedPermeability(), target, target * 0.01);
}

TEST_F(ConvergenceTest, SlopeOfConstantIsZero)
{
    auto opts = defaultOpts();
    opts.window = 5;
    ConvergenceChecker checker(opts);

    for (int i = 0; i < 10; ++i)
    {
        checker.addValue(static_cast<double>(i), 42.0);
    }

    EXPECT_NEAR(checker.currentSlope(), 0.0, 1e-10);
}

TEST_F(ConvergenceTest, PredictionErrorSmallWhenConverged)
{
    auto opts = defaultOpts();
    opts.window = 5;
    ConvergenceChecker checker(opts);

    for (int i = 0; i < 10; ++i)
    {
        checker.addValue(static_cast<double>(i), 1.0);
    }

    EXPECT_LT(checker.predictionError(), opts.errorBound);
}

TEST_F(ConvergenceTest, StoredValuesAccessible)
{
    auto opts = defaultOpts();
    ConvergenceChecker checker(opts);

    checker.addValue(1.0, 10.0);
    checker.addValue(2.0, 20.0);
    checker.addValue(3.0, 30.0);

    EXPECT_EQ(checker.iterations().size(), 3u);
    EXPECT_EQ(checker.permeabilities().size(), 3u);
    EXPECT_DOUBLE_EQ(checker.iterations()[0], 1.0);
    EXPECT_DOUBLE_EQ(checker.permeabilities()[2], 30.0);
}

TEST_F(ConvergenceTest, OscillatingValuesDoNotConverge)
{
    auto opts = defaultOpts();
    opts.window = 5;
    opts.convSlope = 0.001;
    opts.errorBound = 0.001;
    ConvergenceChecker checker(opts);

    // Oscillating values
    for (int i = 0; i < 20; ++i)
    {
        double val = (i % 2 == 0) ? 1.0 : 2.0;
        checker.addValue(static_cast<double>(i), val);
    }

    // Large prediction error should prevent convergence
    EXPECT_FALSE(checker.isConverged());
}
