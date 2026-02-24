#include <gtest/gtest.h>
#include "config/SimulationConfig.h"

#include <filesystem>
#include <fstream>

using namespace fiberfoam;

class SimulationConfigTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tmpDir_ = std::filesystem::temp_directory_path() / "fiberfoam_test_sim_config";
        std::filesystem::create_directories(tmpDir_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(tmpDir_);
    }

    SimulationConfig makeTestConfig()
    {
        SimulationConfig cfg;
        cfg.inputPath = "/path/to/geometry.dat";
        cfg.voxelResolution = 320;
        cfg.voxelSize = 0.5e-6;
        cfg.flowDirections = {FlowDirection::X, FlowDirection::Y};
        cfg.fluid.kinematicViscosity = 7.934782609e-05;
        cfg.fluid.density = 920.0;
        cfg.fluid.dynamicViscosity = 0.073;
        cfg.fluid.pressureInlet = 1.0;
        cfg.fluid.pressureOutlet = 0.0;
        cfg.inletBufferLayers = 5;
        cfg.outletBufferLayers = 5;
        cfg.connectivityCheck = true;
        cfg.autoBoundaryFaceSets = true;
        cfg.periodic = false;
        cfg.enablePrediction = false;
        cfg.modelsDir = "/path/to/models";
        cfg.modelResolution = 80;
        cfg.solverName = "simpleFoamMod";
        cfg.maxIterations = 500000;
        cfg.writeInterval = 25000;
        cfg.convPermeability = true;
        cfg.convSlope = 0.01;
        cfg.convWindow = 10;
        cfg.errorBound = 0.01;
        cfg.fibrousRegionOnly = true;
        cfg.permeabilityMethod = "both";
        cfg.outputPath = "/path/to/output";
        return cfg;
    }

    std::filesystem::path tmpDir_;
};

TEST_F(SimulationConfigTest, YamlRoundTrip)
{
    SimulationConfig original = makeTestConfig();
    std::string yamlPath = (tmpDir_ / "config.yaml").string();

    // Write to YAML
    original.toYaml(yamlPath);

    // Read back
    SimulationConfig loaded = SimulationConfig::fromYaml(yamlPath);

    // Verify all fields match
    EXPECT_EQ(loaded.inputPath, original.inputPath);
    EXPECT_EQ(loaded.voxelResolution, original.voxelResolution);
    EXPECT_DOUBLE_EQ(loaded.voxelSize, original.voxelSize);
    EXPECT_EQ(loaded.flowDirections.size(), original.flowDirections.size());
    for (size_t i = 0; i < loaded.flowDirections.size(); ++i)
    {
        EXPECT_EQ(loaded.flowDirections[i], original.flowDirections[i]);
    }
}

TEST_F(SimulationConfigTest, YamlRoundTripFluidProperties)
{
    SimulationConfig original = makeTestConfig();
    std::string yamlPath = (tmpDir_ / "config_fluid.yaml").string();

    original.toYaml(yamlPath);
    SimulationConfig loaded = SimulationConfig::fromYaml(yamlPath);

    EXPECT_DOUBLE_EQ(loaded.fluid.kinematicViscosity, original.fluid.kinematicViscosity);
    EXPECT_DOUBLE_EQ(loaded.fluid.density, original.fluid.density);
    EXPECT_DOUBLE_EQ(loaded.fluid.dynamicViscosity, original.fluid.dynamicViscosity);
    EXPECT_DOUBLE_EQ(loaded.fluid.pressureInlet, original.fluid.pressureInlet);
    EXPECT_DOUBLE_EQ(loaded.fluid.pressureOutlet, original.fluid.pressureOutlet);
}

TEST_F(SimulationConfigTest, YamlRoundTripBufferZones)
{
    SimulationConfig original = makeTestConfig();
    std::string yamlPath = (tmpDir_ / "config_buffer.yaml").string();

    original.toYaml(yamlPath);
    SimulationConfig loaded = SimulationConfig::fromYaml(yamlPath);

    EXPECT_EQ(loaded.inletBufferLayers, original.inletBufferLayers);
    EXPECT_EQ(loaded.outletBufferLayers, original.outletBufferLayers);
}

TEST_F(SimulationConfigTest, YamlRoundTripMeshOptions)
{
    SimulationConfig original = makeTestConfig();
    std::string yamlPath = (tmpDir_ / "config_mesh.yaml").string();

    original.toYaml(yamlPath);
    SimulationConfig loaded = SimulationConfig::fromYaml(yamlPath);

    EXPECT_EQ(loaded.connectivityCheck, original.connectivityCheck);
    EXPECT_EQ(loaded.autoBoundaryFaceSets, original.autoBoundaryFaceSets);
    EXPECT_EQ(loaded.periodic, original.periodic);
}

TEST_F(SimulationConfigTest, YamlRoundTripSolverSettings)
{
    SimulationConfig original = makeTestConfig();
    std::string yamlPath = (tmpDir_ / "config_solver.yaml").string();

    original.toYaml(yamlPath);
    SimulationConfig loaded = SimulationConfig::fromYaml(yamlPath);

    EXPECT_EQ(loaded.solverName, original.solverName);
    EXPECT_EQ(loaded.maxIterations, original.maxIterations);
    EXPECT_EQ(loaded.writeInterval, original.writeInterval);
}

TEST_F(SimulationConfigTest, YamlRoundTripConvergence)
{
    SimulationConfig original = makeTestConfig();
    std::string yamlPath = (tmpDir_ / "config_conv.yaml").string();

    original.toYaml(yamlPath);
    SimulationConfig loaded = SimulationConfig::fromYaml(yamlPath);

    EXPECT_EQ(loaded.convPermeability, original.convPermeability);
    EXPECT_DOUBLE_EQ(loaded.convSlope, original.convSlope);
    EXPECT_EQ(loaded.convWindow, original.convWindow);
    EXPECT_DOUBLE_EQ(loaded.errorBound, original.errorBound);
}

TEST_F(SimulationConfigTest, YamlRoundTripPostprocessing)
{
    SimulationConfig original = makeTestConfig();
    std::string yamlPath = (tmpDir_ / "config_pp.yaml").string();

    original.toYaml(yamlPath);
    SimulationConfig loaded = SimulationConfig::fromYaml(yamlPath);

    EXPECT_EQ(loaded.fibrousRegionOnly, original.fibrousRegionOnly);
    EXPECT_EQ(loaded.permeabilityMethod, original.permeabilityMethod);
}

TEST_F(SimulationConfigTest, YamlRoundTripMLSettings)
{
    SimulationConfig original = makeTestConfig();
    std::string yamlPath = (tmpDir_ / "config_ml.yaml").string();

    original.toYaml(yamlPath);
    SimulationConfig loaded = SimulationConfig::fromYaml(yamlPath);

    EXPECT_EQ(loaded.enablePrediction, original.enablePrediction);
    EXPECT_EQ(loaded.modelsDir, original.modelsDir);
    EXPECT_EQ(loaded.modelResolution, original.modelResolution);
}

TEST_F(SimulationConfigTest, DefaultValues)
{
    SimulationConfig cfg;

    EXPECT_EQ(cfg.voxelResolution, 320);
    EXPECT_DOUBLE_EQ(cfg.voxelSize, 0.5e-6);
    EXPECT_EQ(cfg.flowDirections.size(), 1u);
    EXPECT_EQ(cfg.flowDirections[0], FlowDirection::X);
    EXPECT_EQ(cfg.inletBufferLayers, 0);
    EXPECT_EQ(cfg.outletBufferLayers, 0);
    EXPECT_TRUE(cfg.connectivityCheck);
    EXPECT_TRUE(cfg.autoBoundaryFaceSets);
    EXPECT_FALSE(cfg.periodic);
    EXPECT_FALSE(cfg.enablePrediction);
    EXPECT_EQ(cfg.modelResolution, 80);
    EXPECT_EQ(cfg.solverName, "simpleFoamMod");
    EXPECT_EQ(cfg.maxIterations, 1000000);
    EXPECT_EQ(cfg.writeInterval, 50000);
    EXPECT_TRUE(cfg.convPermeability);
    EXPECT_DOUBLE_EQ(cfg.convSlope, 0.01);
    EXPECT_EQ(cfg.convWindow, 10);
    EXPECT_DOUBLE_EQ(cfg.errorBound, 0.01);
    EXPECT_TRUE(cfg.fibrousRegionOnly);
    EXPECT_EQ(cfg.permeabilityMethod, "both");
}

TEST_F(SimulationConfigTest, FromYamlThrowsOnMissingFile)
{
    EXPECT_THROW(
        SimulationConfig::fromYaml("/nonexistent/path/config.yaml"),
        std::runtime_error);
}

TEST_F(SimulationConfigTest, YamlFileCreated)
{
    SimulationConfig cfg = makeTestConfig();
    std::string yamlPath = (tmpDir_ / "output_config.yaml").string();

    cfg.toYaml(yamlPath);

    EXPECT_TRUE(std::filesystem::exists(yamlPath));
    EXPECT_GT(std::filesystem::file_size(yamlPath), 0u);
}
