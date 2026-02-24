#include <gtest/gtest.h>
#include "ml/ModelRegistry.h"

#include <filesystem>
#include <fstream>

using namespace fiberfoam;

class ModelRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tmpDir_ = std::filesystem::temp_directory_path() / "fiberfoam_test_model_registry";
        std::filesystem::create_directories(tmpDir_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(tmpDir_);
    }

    void createFakeOnnxFile(const std::string& name)
    {
        auto path = tmpDir_ / name;
        std::ofstream(path.string()) << "fake onnx content";
    }

    void createYamlConfig(const std::string& content)
    {
        auto path = tmpDir_ / "models.yaml";
        std::ofstream(path.string()) << content;
    }

    std::filesystem::path tmpDir_;
};

TEST_F(ModelRegistryTest, FromDirectoryDetectsModels)
{
    createFakeOnnxFile("x.onnx");
    createFakeOnnxFile("y.onnx");
    createFakeOnnxFile("z.onnx");

    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir_.string(), 80);

    EXPECT_TRUE(registry.hasModel(FlowDirection::X, 80));
    EXPECT_TRUE(registry.hasModel(FlowDirection::Y, 80));
    EXPECT_TRUE(registry.hasModel(FlowDirection::Z, 80));
}

TEST_F(ModelRegistryTest, FromDirectoryPartialModels)
{
    createFakeOnnxFile("x.onnx");
    // Only X model present

    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir_.string(), 80);

    EXPECT_TRUE(registry.hasModel(FlowDirection::X, 80));
    EXPECT_FALSE(registry.hasModel(FlowDirection::Y, 80));
    EXPECT_FALSE(registry.hasModel(FlowDirection::Z, 80));
}

TEST_F(ModelRegistryTest, GetModelReturnsCorrectInfo)
{
    createFakeOnnxFile("x.onnx");

    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir_.string(), 80);

    const ModelInfo& info = registry.getModel(FlowDirection::X, 80);
    EXPECT_EQ(info.resolution, 80);
    EXPECT_EQ(info.direction, FlowDirection::X);
    EXPECT_FALSE(info.path.empty());
}

TEST_F(ModelRegistryTest, GetModelThrowsForMissing)
{
    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir_.string(), 80);

    EXPECT_THROW(registry.getModel(FlowDirection::X, 80), std::runtime_error);
}

TEST_F(ModelRegistryTest, ModelsDirectoryStored)
{
    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir_.string(), 80);

    EXPECT_EQ(registry.modelsDir(), tmpDir_.string());
}

TEST_F(ModelRegistryTest, DifferentResolutions)
{
    createFakeOnnxFile("x.onnx");

    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir_.string(), 80);

    // Model registered at res 80 should not be found at res 40
    EXPECT_TRUE(registry.hasModel(FlowDirection::X, 80));
    EXPECT_FALSE(registry.hasModel(FlowDirection::X, 40));
}

TEST_F(ModelRegistryTest, FromNonExistentDirectoryThrows)
{
    EXPECT_THROW(
        ModelRegistry::fromDirectory("/nonexistent/path/to/models", 80),
        std::runtime_error);
}

TEST_F(ModelRegistryTest, FromYamlLoadsConfig)
{
    // Create a simple YAML config
    std::string yaml =
        "models_dir: " + tmpDir_.string() + "\n"
        "models:\n"
        "  - direction: x\n"
        "    resolution: 80\n"
        "    path: " + (tmpDir_ / "x.onnx").string() + "\n"
        "  - direction: y\n"
        "    resolution: 80\n"
        "    path: " + (tmpDir_ / "y.onnx").string() + "\n";

    createYamlConfig(yaml);
    createFakeOnnxFile("x.onnx");
    createFakeOnnxFile("y.onnx");

    ModelRegistry registry = ModelRegistry::fromYaml((tmpDir_ / "models.yaml").string());

    EXPECT_TRUE(registry.hasModel(FlowDirection::X, 80));
    EXPECT_TRUE(registry.hasModel(FlowDirection::Y, 80));
    EXPECT_FALSE(registry.hasModel(FlowDirection::Z, 80));
}

TEST_F(ModelRegistryTest, EmptyDirectoryNoModels)
{
    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir_.string(), 80);

    EXPECT_FALSE(registry.hasModel(FlowDirection::X, 80));
    EXPECT_FALSE(registry.hasModel(FlowDirection::Y, 80));
    EXPECT_FALSE(registry.hasModel(FlowDirection::Z, 80));
}
