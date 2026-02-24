#include <gtest/gtest.h>
#include "ml/OnnxPredictor.h"
#include "ml/ModelRegistry.h"
#include "geometry/VoxelArray.h"

#include <filesystem>

using namespace fiberfoam;

class OnnxPredictorTest : public ::testing::Test
{
protected:
    VoxelArray makeTestGeometry(int res)
    {
        std::vector<int8_t> data(res * res * res, 1);
        return VoxelArray(data, res, res, res);
    }
};

TEST_F(OnnxPredictorTest, ThrowsOnNonExistentModelDirectory)
{
    // fromDirectory should throw or return empty registry for non-existent path
    EXPECT_THROW(
        ModelRegistry::fromDirectory("/nonexistent/path/to/models", 80),
        std::runtime_error);
}

TEST_F(OnnxPredictorTest, ThrowsOnMissingModel)
{
    // Create a temporary directory with no model files
    auto tmpDir = std::filesystem::temp_directory_path() / "fiberfoam_test_empty_models";
    std::filesystem::create_directories(tmpDir);

    // Registry with no actual model files
    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir.string(), 80);

    // hasModel should return false for directions without model files
    EXPECT_FALSE(registry.hasModel(FlowDirection::X, 80));
    EXPECT_FALSE(registry.hasModel(FlowDirection::Y, 80));
    EXPECT_FALSE(registry.hasModel(FlowDirection::Z, 80));

    // Attempting to get a missing model should throw
    EXPECT_THROW(registry.getModel(FlowDirection::X, 80), std::runtime_error);

    std::filesystem::remove_all(tmpDir);
}

#ifdef FIBERFOAM_HAS_ONNX
TEST_F(OnnxPredictorTest, PredictWithMissingModel)
{
    auto tmpDir = std::filesystem::temp_directory_path() / "fiberfoam_test_no_onnx";
    std::filesystem::create_directories(tmpDir);

    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir.string(), 80);
    OnnxPredictor predictor(registry, 80);

    VoxelArray geom = makeTestGeometry(80);

    // Predict should throw when model file is missing
    EXPECT_THROW(predictor.predict(geom, FlowDirection::X), std::runtime_error);

    std::filesystem::remove_all(tmpDir);
}

TEST_F(OnnxPredictorTest, PredictAllWithMissingModels)
{
    auto tmpDir = std::filesystem::temp_directory_path() / "fiberfoam_test_no_onnx_all";
    std::filesystem::create_directories(tmpDir);

    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir.string(), 80);
    OnnxPredictor predictor(registry, 80);

    VoxelArray geom = makeTestGeometry(80);

    // predictAll should throw when model files are missing
    EXPECT_THROW(predictor.predictAll(geom), std::runtime_error);

    std::filesystem::remove_all(tmpDir);
}
#endif

TEST_F(OnnxPredictorTest, RegistryKeyLookup)
{
    auto tmpDir = std::filesystem::temp_directory_path() / "fiberfoam_test_key_lookup";
    std::filesystem::create_directories(tmpDir);

    // Create fake .onnx files so fromDirectory can detect them
    for (const std::string& dir : {"x", "y", "z"})
    {
        auto modelPath = tmpDir / (dir + ".onnx");
        std::ofstream(modelPath.string()) << "fake onnx content";
    }

    ModelRegistry registry = ModelRegistry::fromDirectory(tmpDir.string(), 80);

    EXPECT_TRUE(registry.hasModel(FlowDirection::X, 80));
    EXPECT_TRUE(registry.hasModel(FlowDirection::Y, 80));
    EXPECT_TRUE(registry.hasModel(FlowDirection::Z, 80));

    std::filesystem::remove_all(tmpDir);
}
