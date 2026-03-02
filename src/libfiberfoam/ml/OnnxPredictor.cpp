#include "ml/OnnxPredictor.h"
#include "common/Logger.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fiberfoam
{

// ===========================================================================
// Cubic B-spline zoom (shared helpers for geometry downsampling)
// Matches scipy.ndimage.zoom(arr, factor, order=3, mode='constant', cval=0).
// ===========================================================================

// 1-D causal+anti-causal IIR prefilter for cubic B-spline coefficients.
// Uses constant boundary extension (cval=0) matching scipy's default mode.
static void bsplinePrefilter1D(double* c, int n)
{
    if (n < 2) return;

    const double pole = -2.0 + std::sqrt(3.0);
    const double lambda = 6.0;

    for (int i = 0; i < n; ++i)
        c[i] *= lambda;

    // Causal initialisation (constant boundary, cval=0)
    // c+(0) = c[0] since extension values are all 0
    // c[0] is already correct after gain scaling

    // Causal recursion
    for (int i = 1; i < n; ++i)
        c[i] += pole * c[i - 1];

    // Anti-causal initialisation (constant boundary, cval=0)
    // c-(n-1) = c_filt[n-1] * pole / (pole - 1)
    c[n - 1] = c[n - 1] * pole / (pole - 1.0);

    // Anti-causal recursion
    for (int i = n - 2; i >= 0; --i)
        c[i] = pole * (c[i + 1] - c[i]);
}

// Apply 1-D prefilter along one axis of a 3-D C-order array [d0 x d1 x d2].
static void bsplinePrefilter3D(std::vector<double>& data, int d0, int d1, int d2, int axis)
{
    if (axis == 0)
    {
        std::vector<double> line(d0);
        for (int j = 0; j < d1; ++j)
            for (int k = 0; k < d2; ++k)
            {
                for (int i = 0; i < d0; ++i) line[i] = data[i * d1 * d2 + j * d2 + k];
                bsplinePrefilter1D(line.data(), d0);
                for (int i = 0; i < d0; ++i) data[i * d1 * d2 + j * d2 + k] = line[i];
            }
    }
    else if (axis == 1)
    {
        std::vector<double> line(d1);
        for (int i = 0; i < d0; ++i)
            for (int k = 0; k < d2; ++k)
            {
                for (int j = 0; j < d1; ++j) line[j] = data[i * d1 * d2 + j * d2 + k];
                bsplinePrefilter1D(line.data(), d1);
                for (int j = 0; j < d1; ++j) data[i * d1 * d2 + j * d2 + k] = line[j];
            }
    }
    else
    {
        for (int i = 0; i < d0; ++i)
            for (int j = 0; j < d1; ++j)
                bsplinePrefilter1D(&data[i * d1 * d2 + j * d2], d2);
    }
}

// Cubic B-spline basis weights for fractional position t in [0,1).
static inline void bsplineBasis(double t, double w[4])
{
    double omt = 1.0 - t;
    w[0] = omt * omt * omt / 6.0;
    w[1] = (3.0 * t * t * t - 6.0 * t * t + 4.0) / 6.0;
    w[2] = (3.0 * omt * omt * omt - 6.0 * omt * omt + 4.0) / 6.0;
    w[3] = t * t * t / 6.0;
}

// Cubic B-spline zoom of a 3-D C-order array from [sx, sy, sz] to [dx, dy, dz].
static std::vector<double> bsplineZoom3D(
    const std::vector<double>& src, int sx, int sy, int sz,
    int dx, int dy, int dz)
{
    // 1. Convert input to B-spline coefficients via prefiltering
    std::vector<double> coeffs(src);
    bsplinePrefilter3D(coeffs, sx, sy, sz, 0);
    bsplinePrefilter3D(coeffs, sx, sy, sz, 1);
    bsplinePrefilter3D(coeffs, sx, sy, sz, 2);

    // 2. Pre-compute per-axis indices and weights
    // For constant boundary (mode='constant', cval=0), out-of-bounds coefficients
    // contribute 0 to the interpolation (weight zeroed out).
    struct AxisSample { int idx[4]; double w[4]; };

    auto buildSamples = [&](int srcDim, int dstDim) {
        double scale = (dstDim > 1) ? static_cast<double>(srcDim - 1) / (dstDim - 1) : 0.0;
        std::vector<AxisSample> samples(dstDim);
        for (int d = 0; d < dstDim; ++d)
        {
            double s = d * scale;
            int i = static_cast<int>(std::floor(s));
            double t = s - i;
            bsplineBasis(t, samples[d].w);
            for (int k = 0; k < 4; ++k)
            {
                int idx = i - 1 + k;
                if (idx < 0 || idx >= srcDim)
                {
                    samples[d].idx[k] = 0; // dummy (weight is zeroed)
                    samples[d].w[k] = 0.0;
                }
                else
                {
                    samples[d].idx[k] = idx;
                }
            }
        }
        return samples;
    };

    auto xSamples = buildSamples(sx, dx);
    auto ySamples = buildSamples(sy, dy);
    auto zSamples = buildSamples(sz, dz);

    // 3. Evaluate
    std::vector<double> dst(static_cast<size_t>(dx) * dy * dz);

    for (int ix = 0; ix < dx; ++ix)
    {
        const auto& ax = xSamples[ix];
        for (int iy = 0; iy < dy; ++iy)
        {
            const auto& ay = ySamples[iy];
            for (int iz = 0; iz < dz; ++iz)
            {
                const auto& az = zSamples[iz];
                double val = 0.0;
                for (int a = 0; a < 4; ++a)
                    for (int b = 0; b < 4; ++b)
                        for (int c = 0; c < 4; ++c)
                            val += ax.w[a] * ay.w[b] * az.w[c] *
                                   coeffs[ax.idx[a] * sy * sz +
                                          ay.idx[b] * sz +
                                          az.idx[c]];
                dst[ix * dy * dz + iy * dz + iz] = val;
            }
        }
    }

    return dst;
}

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

    // Step 1: Build float input at model resolution.
    // If geometry differs from model resolution, downsample using trilinear
    // interpolation (matching Python's ndimage.zoom(geom, 0.25) which uses
    // cubic spline by default, but trilinear is a close approximation).
    int nx = resolution_;
    int ny = resolution_;
    int nz = resolution_;
    int64_t totalVoxels = static_cast<int64_t>(nx) * ny * nz;

    Logger::debug("Predict " + directionName(direction) +
                  ": input shape = [" + std::to_string(nx) + ", " +
                  std::to_string(ny) + ", " + std::to_string(nz) + "]");

    // Step 2: Convert voxel data to float with shape [1, nx, ny, nz, 1]
    // Use cubic B-spline interpolation to downsample from full-resolution geometry
    // to model resolution, matching Python's ndimage.zoom(geom, 0.25, order=3).
    std::vector<float> inputData(totalVoxels);

    int srcNx = geometry.nx();
    int srcNy = geometry.ny();
    int srcNz = geometry.nz();

    if (srcNx == nx && srcNy == ny && srcNz == nz)
    {
        // No resampling needed — direct copy
        for (int ix = 0; ix < nx; ++ix)
            for (int iy = 0; iy < ny; ++iy)
                for (int iz = 0; iz < nz; ++iz)
                    inputData[ix * ny * nz + iy * nz + iz] =
                        static_cast<float>(geometry.at(ix, iy, iz));
    }
    else
    {
        // B-spline downsampling from full-res to model-res
        // 1. Convert int8_t geometry to doubles
        int64_t srcTotal = static_cast<int64_t>(srcNx) * srcNy * srcNz;
        std::vector<double> srcData(srcTotal);
        for (int ix = 0; ix < srcNx; ++ix)
            for (int iy = 0; iy < srcNy; ++iy)
                for (int iz = 0; iz < srcNz; ++iz)
                    srcData[ix * srcNy * srcNz + iy * srcNz + iz] =
                        static_cast<double>(geometry.at(ix, iy, iz));

        Logger::info("B-spline downsampling geometry from " +
                     std::to_string(srcNx) + "x" + std::to_string(srcNy) + "x" +
                     std::to_string(srcNz) + " to " +
                     std::to_string(nx) + "x" + std::to_string(ny) + "x" +
                     std::to_string(nz));

        // 2. Apply B-spline zoom (prefilter + evaluate)
        std::vector<double> downsampled = bsplineZoom3D(srcData, srcNx, srcNy, srcNz,
                                                         nx, ny, nz);
        srcData.clear(); // free memory

        // 3. Copy to float input
        for (int64_t i = 0; i < totalVoxels; ++i)
            inputData[i] = static_cast<float>(downsampled[i]);
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
