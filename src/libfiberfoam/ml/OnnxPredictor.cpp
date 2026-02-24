#include "ml/OnnxPredictor.h"
#include "common/Logger.h"

#include <stdexcept>

namespace fiberfoam
{

// ===========================================================================
// When ONNX Runtime is available
// ===========================================================================
#ifdef FIBERFOAM_HAS_ONNX

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
OnnxPredictor::OnnxPredictor(const ModelRegistry& registry, int resolution)
    : registry_(registry), resolution_(resolution),
      env_(ORT_LOGGING_LEVEL_WARNING, "fiberfoam")
{
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
OnnxPredictor::~OnnxPredictor() = default;

// ---------------------------------------------------------------------------
// loadModel - create an ONNX Runtime session from the .onnx file
// ---------------------------------------------------------------------------
void OnnxPredictor::loadModel(FlowDirection direction)
{
    if (sessions_.count(direction))
        return; // already loaded

    const ModelInfo& info = registry_.getModel(direction, resolution_);
    Logger::info("Loading ONNX model: " + info.path);

    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(1);
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    auto session = std::make_unique<Ort::Session>(env_, info.path.c_str(), sessionOptions);
    sessions_[direction] = std::move(session);

    Logger::info("ONNX model loaded for direction " + directionName(direction));
}

// ---------------------------------------------------------------------------
// predict - run inference for a single flow direction
//
//   1. Resample input geometry to model resolution if needed
//   2. Convert to float, expand dims to [1, nx, ny, nz, 1] (batch + channel)
//   3. Run ONNX inference
//   4. Squeeze output and return as doubles
// ---------------------------------------------------------------------------
std::vector<double> OnnxPredictor::predict(const VoxelArray& geometry,
                                           FlowDirection direction)
{
    // Ensure model is loaded
    loadModel(direction);

    Ort::Session& session = *sessions_.at(direction);

    // Step 1: Resample geometry to model resolution if dimensions differ
    VoxelArray geom = (geometry.nx() != resolution_) ? geometry.resample(resolution_) : geometry;

    int nx = geom.nx();
    int ny = geom.ny();
    int nz = geom.nz();
    int64_t totalVoxels = static_cast<int64_t>(nx) * ny * nz;

    Logger::debug("Predict " + directionName(direction) +
                  ": input shape = [" + std::to_string(nx) + ", " +
                  std::to_string(ny) + ", " + std::to_string(nz) + "]");

    // Step 2: Convert voxel data to float with shape [1, nx, ny, nz, 1]
    std::vector<float> inputData(totalVoxels);
    const auto& rawData = geom.data();
    for (int64_t i = 0; i < totalVoxels; ++i)
    {
        inputData[i] = static_cast<float>(rawData[i]);
    }

    // Input tensor: [batch=1, nx, ny, nz, channels=1]
    std::array<int64_t, 5> inputShape = {1, nx, ny, nz, 1};

    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memInfo, inputData.data(), inputData.size(),
        inputShape.data(), inputShape.size());

    // Step 3: Query input/output names from the model
    Ort::AllocatorWithDefaultOptions allocator;

    auto inputNameAlloc = session.GetInputNameAllocated(0, allocator);
    const char* inputName = inputNameAlloc.get();

    auto outputNameAlloc = session.GetOutputNameAllocated(0, allocator);
    const char* outputName = outputNameAlloc.get();

    // Run inference
    auto outputTensors = session.Run(
        Ort::RunOptions{nullptr},
        &inputName, &inputTensor, 1,
        &outputName, 1);

    // Step 4: Extract output and squeeze
    const float* outputData = outputTensors[0].GetTensorData<float>();

    auto outputInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
    auto outputShape = outputInfo.GetShape();

    // Compute total output elements
    int64_t outputTotal = 1;
    for (auto dim : outputShape)
    {
        outputTotal *= dim;
    }

    // Convert to double vector (squeeze batch and channel dimensions)
    std::vector<double> result(outputTotal);
    for (int64_t i = 0; i < outputTotal; ++i)
    {
        result[i] = static_cast<double>(outputData[i]);
    }

    Logger::info("Prediction complete for direction " + directionName(direction) +
                 " (" + std::to_string(outputTotal) + " values)");

    return result;
}

// ---------------------------------------------------------------------------
// predictAll - run inference for all 3 flow directions
// ---------------------------------------------------------------------------
std::map<FlowDirection, std::vector<double>> OnnxPredictor::predictAll(
    const VoxelArray& geometry)
{
    std::map<FlowDirection, std::vector<double>> results;

    for (auto dir : {FlowDirection::X, FlowDirection::Y, FlowDirection::Z})
    {
        if (registry_.hasModel(dir, resolution_))
        {
            results[dir] = predict(geometry, dir);
        }
        else
        {
            Logger::warning("No model available for direction " + directionName(dir) +
                            " at resolution " + std::to_string(resolution_));
        }
    }

    return results;
}

// ===========================================================================
// When ONNX Runtime is NOT available - stub implementation
// ===========================================================================
#else

OnnxPredictor::OnnxPredictor(const ModelRegistry& registry, int resolution)
    : registry_(registry), resolution_(resolution)
{
}

OnnxPredictor::~OnnxPredictor() = default;

void OnnxPredictor::loadModel(FlowDirection /*direction*/)
{
    throw std::runtime_error(
        "OnnxPredictor: ONNX Runtime is not available. "
        "Rebuild with -DONNX_AVAILABLE=ON and ensure onnxruntime is installed.");
}

std::vector<double> OnnxPredictor::predict(const VoxelArray& /*geometry*/,
                                           FlowDirection /*direction*/)
{
    throw std::runtime_error(
        "OnnxPredictor: ONNX Runtime is not available. "
        "Rebuild with -DONNX_AVAILABLE=ON and ensure onnxruntime is installed.");
}

std::map<FlowDirection, std::vector<double>> OnnxPredictor::predictAll(
    const VoxelArray& /*geometry*/)
{
    throw std::runtime_error(
        "OnnxPredictor: ONNX Runtime is not available. "
        "Rebuild with -DONNX_AVAILABLE=ON and ensure onnxruntime is installed.");
}

#endif // FIBERFOAM_HAS_ONNX

} // namespace fiberfoam
