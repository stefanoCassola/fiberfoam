#include "common/Types.h"
#include "common/Logger.h"
#include "common/Timer.h"
#include "config/SimulationConfig.h"
#include "geometry/VoxelArray.h"
#include "geometry/FiberFreeRegion.h"
#include "mesh/HexMeshBuilder.h"
#include "io/FoamWriter.h"
#include "ml/ModelRegistry.h"
#include "ml/OnnxPredictor.h"
#include "ml/Scaling.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace fiberfoam;

void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  -input <path>          Input geometry file (.dat or .npy)\n"
              << "  -output <path>         Output case directory\n"
              << "  -modelsDir <path>      Directory containing ONNX models\n"
              << "  -scalingFactors <path> Path to scaling_factors.json\n"
              << "  -voxelSize <value>     Voxel size in meters (e.g. 0.5e-6)\n"
              << "  -voxelRes <int>        Voxel resolution of input array\n"
              << "  -modelRes <int>        Model resolution (default: 80)\n"
              << "  -flowDirection <dir>   Flow direction: x, y, z, or all\n"
              << "  -inletBuffer <int>     Number of inlet buffer layers (default: 0)\n"
              << "  -outletBuffer <int>    Number of outlet buffer layers (default: 0)\n"
              << "  -connectivity          Enable connectivity check (default: on)\n"
              << "  -noConnectivity        Disable connectivity check\n"
              << "  -config <path>         Load settings from YAML config\n"
              << std::endl;
}

// ---------------------------------------------------------------------------
// Cubic B-spline zoom matching scipy.ndimage.zoom(arr, factor, order=3).
//
//  1.  Apply 1-D IIR prefilter along each axis to convert samples into
//      cubic B-spline coefficients (constant boundary, cval=0 — scipy default).
//  2.  Evaluate the cubic B-spline at the target positions using the
//      standard 4-point basis.
//
// This produces the same tiny non-zero artefacts in zero regions that
// ndimage.zoom produces, ensuring no exact zeros in the output when the
// source has neighbouring non-zero values anywhere along the prefilter
// path.
// ---------------------------------------------------------------------------

// 1-D causal+anti-causal IIR prefilter for cubic B-spline coefficients.
// Operates in-place on a contiguous 1-D line of length n.
// Uses constant boundary extension (cval=0) matching scipy's default mode.
static void bsplinePrefilter1D(double* c, int n)
{
    if (n < 2) return;

    const double pole = -2.0 + std::sqrt(3.0);   // ≈ -0.267949
    const double lambda = 6.0;                    // gain for cubic B-spline

    // Scale by lambda
    for (int i = 0; i < n; ++i)
        c[i] *= lambda;

    // --- Causal initialisation (constant boundary, cval=0) ---
    // c+(0) = c[0] since extension values are all 0
    // c[0] is already correct after gain scaling

    // Causal recursion
    for (int i = 1; i < n; ++i)
        c[i] += pole * c[i - 1];

    // --- Anti-causal initialisation (constant boundary, cval=0) ---
    // c-(n-1) = c_filt[n-1] * pole / (pole - 1)
    c[n - 1] = c[n - 1] * pole / (pole - 1.0);

    // Anti-causal recursion
    for (int i = n - 2; i >= 0; --i)
        c[i] = pole * (c[i + 1] - c[i]);
}

// Apply the 1-D prefilter along one axis of a 3-D C-order array [d0 x d1 x d2].
// axis: 0 => lines along dim-0, 1 => dim-1, 2 => dim-2.
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
    else // axis == 2
    {
        for (int i = 0; i < d0; ++i)
            for (int j = 0; j < d1; ++j)
                bsplinePrefilter1D(&data[i * d1 * d2 + j * d2], d2);
    }
}

// Cubic B-spline basis weights for fractional position t in [0,1).
// w[0]..w[3] correspond to coefficients at indices i-1, i, i+1, i+2.
static inline void bsplineBasis(double t, double w[4])
{
    double omt = 1.0 - t;
    w[0] = omt * omt * omt / 6.0;
    w[1] = (3.0 * t * t * t - 6.0 * t * t + 4.0) / 6.0;
    w[2] = (3.0 * omt * omt * omt - 6.0 * omt * omt + 4.0) / 6.0;
    w[3] = t * t * t / 6.0;
}

// Separable cubic B-spline zoom of a 3-D C-order array from srcRes³ to dstRes³.
static std::vector<double> bsplineZoom3D(
    const std::vector<double>& src, int srcRes, int dstRes)
{
    // 1. Convert input to B-spline coefficients via prefiltering
    std::vector<double> coeffs(src);
    bsplinePrefilter3D(coeffs, srcRes, srcRes, srcRes, 0);
    bsplinePrefilter3D(coeffs, srcRes, srcRes, srcRes, 1);
    bsplinePrefilter3D(coeffs, srcRes, srcRes, srcRes, 2);

    // 2. Evaluate B-spline at target positions
    double scale = (dstRes > 1) ? static_cast<double>(srcRes - 1) / (dstRes - 1) : 0.0;

    // Pre-compute per-axis indices and weights
    // For constant boundary (mode='constant', cval=0), out-of-bounds coefficients
    // contribute 0 to the interpolation (weight zeroed out).
    struct AxisSample { int idx[4]; double w[4]; };
    std::vector<AxisSample> axSamples(dstRes);
    for (int d = 0; d < dstRes; ++d)
    {
        double s = d * scale;
        int i = static_cast<int>(std::floor(s));
        double t = s - i;
        bsplineBasis(t, axSamples[d].w);
        for (int k = 0; k < 4; ++k)
        {
            int idx = i - 1 + k;
            if (idx < 0 || idx >= srcRes)
            {
                axSamples[d].idx[k] = 0; // dummy (weight is zeroed)
                axSamples[d].w[k] = 0.0;
            }
            else
            {
                axSamples[d].idx[k] = idx;
            }
        }
    }

    std::vector<double> dst(static_cast<size_t>(dstRes) * dstRes * dstRes);

    for (int dx = 0; dx < dstRes; ++dx)
    {
        const auto& ax = axSamples[dx];
        for (int dy = 0; dy < dstRes; ++dy)
        {
            const auto& ay = axSamples[dy];
            for (int dz = 0; dz < dstRes; ++dz)
            {
                const auto& az = axSamples[dz];
                double val = 0.0;
                for (int a = 0; a < 4; ++a)
                    for (int b = 0; b < 4; ++b)
                        for (int c = 0; c < 4; ++c)
                            val += ax.w[a] * ay.w[b] * az.w[c] *
                                   coeffs[ax.idx[a] * srcRes * srcRes +
                                          ay.idx[b] * srcRes +
                                          az.idx[c]];
                dst[dx * dstRes * dstRes + dy * dstRes + dz] = val;
            }
        }
    }

    return dst;
}

// Map a 3D velocity prediction array into mesh cell velocities.
// predictedVel is a flat array of size (modelRes^3) at model resolution.
// The prediction is first upsampled to geometry resolution via trilinear
// interpolation (matching Python's ndimage.zoom) before mapping to cells.
// Buffer zone cells (inlet/outlet padding) are left at zero velocity.
static void mapPredictionToMesh(
    MeshData& mesh,
    const std::vector<double>& predictedVel,
    int modelRes,
    const VoxelArray& geometry,
    FlowDirection dir,
    int inletLayers,
    int outletLayers,
    double scalingFactor)
{
    int geomNx = geometry.nx();
    int geomNy = geometry.ny();
    int geomNz = geometry.nz();

    // Original (un-padded) resolution along each axis
    int totalBufferMain = inletLayers + outletLayers;
    int origNx = geomNx - (dir == FlowDirection::X ? totalBufferMain : 0);
    int origNy = geomNy - (dir == FlowDirection::Y ? totalBufferMain : 0);
    int origNz = geomNz - (dir == FlowDirection::Z ? totalBufferMain : 0);

    int mainAxis = axisIndex(dir);
    int secAxis = axisIndex(secondaryDirection(dir));
    int tertAxis = axisIndex(tertiaryDirection(dir));

    // The original geometry is cubic: all sides have the same un-padded size
    int baseRes = (dir == FlowDirection::X) ? origNx :
                  (dir == FlowDirection::Y) ? origNy : origNz;

    // Scale the raw prediction by scalingFactor, then upsample to geometry resolution.
    // This matches Python: pred = np.squeeze(pred * scale); pred_up = ndimage.zoom(pred, 4.0)
    std::vector<double> scaledPred(predictedVel.size());
    for (size_t i = 0; i < predictedVel.size(); ++i)
        scaledPred[i] = predictedVel[i] * scalingFactor;

    Logger::info("Upsampling prediction from " + std::to_string(modelRes) +
                 "³ to " + std::to_string(baseRes) + "³ using B-spline interpolation...");
    std::vector<double> upsampled = bsplineZoom3D(scaledPred, modelRes, baseRes);
    scaledPred.clear(); // free memory

    int mapped = 0;
    int skipped = 0;
    int bufferCells = 0;

    for (auto& [cellId, cell] : mesh.cellMap)
    {
        if (cell.region != CellRegion::Fibrous)
        {
            bufferCells++;
            continue;
        }

        int coords[3] = {cell.coord[0], cell.coord[1], cell.coord[2]};

        // Convert padded coordinate to original coordinate
        int origMain = coords[mainAxis] - inletLayers;
        int origSec  = coords[secAxis];
        int origTert = coords[tertAxis];

        if (origMain < 0 || origMain >= baseRes ||
            origSec  < 0 || origSec  >= baseRes ||
            origTert < 0 || origTert >= baseRes)
        {
            skipped++;
            continue;
        }

        // Direct lookup in upsampled array (now at geometry resolution)
        int fullCoords[3];
        fullCoords[mainAxis] = origMain;
        fullCoords[secAxis]  = origSec;
        fullCoords[tertAxis] = origTert;

        int flatIdx = fullCoords[0] * baseRes * baseRes +
                      fullCoords[1] * baseRes +
                      fullCoords[2];

        double vel = upsampled[flatIdx];

        switch (dir)
        {
        case FlowDirection::X: cell.u = vel; break;
        case FlowDirection::Y: cell.v = vel; break;
        case FlowDirection::Z: cell.w = vel; break;
        }

        mapped++;
    }

    Logger::info("Mapped velocity to " + std::to_string(mapped) + " cells" +
                 (bufferCells > 0 ? " (" + std::to_string(bufferCells) + " buffer cells zeroed)" : "") +
                 (skipped > 0 ? " (" + std::to_string(skipped) + " skipped)" : ""));
}

int main(int argc, char* argv[])
{
    SimulationConfig config;
    config.connectivityCheck = true;
    config.enablePrediction = true;
    std::string scalingFactorsPath;

    // Parse arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-input" && i + 1 < argc)
            config.inputPath = argv[++i];
        else if (arg == "-output" && i + 1 < argc)
            config.outputPath = argv[++i];
        else if (arg == "-modelsDir" && i + 1 < argc)
            config.modelsDir = argv[++i];
        else if (arg == "-scalingFactors" && i + 1 < argc)
            scalingFactorsPath = argv[++i];
        else if (arg == "-voxelSize" && i + 1 < argc)
            config.voxelSize = std::stod(argv[++i]);
        else if (arg == "-voxelRes" && i + 1 < argc)
            config.voxelResolution = std::stoi(argv[++i]);
        else if (arg == "-modelRes" && i + 1 < argc)
            config.modelResolution = std::stoi(argv[++i]);
        else if (arg == "-flowDirection" && i + 1 < argc)
        {
            std::string dir = argv[++i];
            config.flowDirections.clear();
            if (dir == "all")
            {
                config.flowDirections = {FlowDirection::X, FlowDirection::Y, FlowDirection::Z};
            }
            else
            {
                config.flowDirections.push_back(directionFromName(dir));
            }
        }
        else if (arg == "-inletBuffer" && i + 1 < argc)
            config.inletBufferLayers = std::stoi(argv[++i]);
        else if (arg == "-outletBuffer" && i + 1 < argc)
            config.outletBufferLayers = std::stoi(argv[++i]);
        else if (arg == "-connectivity")
            config.connectivityCheck = true;
        else if (arg == "-noConnectivity")
            config.connectivityCheck = false;
        else if (arg == "-config" && i + 1 < argc)
            config = SimulationConfig::fromYaml(argv[++i]);
        else if (arg == "-help" || arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (config.inputPath.empty() || config.outputPath.empty())
    {
        printUsage(argv[0]);
        return 1;
    }

    if (config.modelsDir.empty())
    {
        Logger::error("No models directory specified. Use -modelsDir <path>.");
        return 1;
    }

    // Auto-detect scaling factors file if not specified
    if (scalingFactorsPath.empty())
    {
        std::string autoPath = config.modelsDir + "/scaling_factors.json";
        if (std::filesystem::exists(autoPath))
            scalingFactorsPath = autoPath;
    }

    if (!config.connectivityCheck)
    {
        Logger::warning("Connectivity check DISABLED. This may produce disconnected "
                        "mesh regions that will cause OpenFOAM to fail!");
    }

    Logger::info("fiberFoamPredict - ML-Accelerated Velocity Field Prediction");
    Logger::info("Input: " + config.inputPath);
    Logger::info("Output: " + config.outputPath);
    Logger::info("Models: " + config.modelsDir);
    Logger::info("Model resolution: " + std::to_string(config.modelResolution));

    Timer totalTimer("Total prediction pipeline");

    // Load geometry at full resolution
    VoxelArray geometry = VoxelArray::fromDatFile(config.inputPath, config.voxelResolution);
    Logger::info("Geometry loaded: " + std::to_string(geometry.nx()) + "x" +
                 std::to_string(geometry.ny()) + "x" + std::to_string(geometry.nz()));
    Logger::info("Fluid fraction: " + std::to_string(geometry.fluidFraction() * 100) + "%");

    Logger::info("Prediction geometry: " + std::to_string(config.modelResolution) + "x" +
                 std::to_string(config.modelResolution) + "x" +
                 std::to_string(config.modelResolution) +
                 " (downsampled from " + std::to_string(geometry.nx()) + "³ inside predictor)");

    // Load ONNX models
    ModelRegistry registry = ModelRegistry::fromDirectory(config.modelsDir, config.modelResolution);
    OnnxPredictor predictor(registry, config.modelResolution);

    // Load scaling factors
    ScalingFactors scaling;
    bool hasScaling = false;
    if (!scalingFactorsPath.empty())
    {
        scaling = ScalingFactors::fromJson(scalingFactorsPath);
        hasScaling = true;
    }
    else
    {
        Logger::warning("No scaling factors file found. Raw model output will be used.");
    }

    // Run predictions for each direction
    for (auto dir : config.flowDirections)
    {
        Logger::info("Processing flow direction: " + directionName(dir));
        Timer dirTimer("Direction " + directionName(dir));

        if (!registry.hasModel(dir, config.modelResolution))
        {
            Logger::warning("No model found for direction " + directionName(dir) +
                            " at resolution " + std::to_string(config.modelResolution) +
                            ", skipping.");
            continue;
        }

        // Predict velocity at model resolution (predictor handles downsampling internally)
        std::vector<double> predictedVelocity = predictor.predict(geometry, dir);
        Logger::info("Prediction complete: " + std::to_string(predictedVelocity.size()) + " values");

        // Apply scaling factor
        double scale = 1.0;
        if (hasScaling && scaling.hasFactor(dir, config.modelResolution))
        {
            scale = scaling.getFactor(dir, config.modelResolution);
            Logger::info("Scaling factor for " + directionName(dir) + ": " +
                         std::to_string(scale));
        }

        // Optional buffer zones
        VoxelArray* geomPtr = &geometry;
        PaddedGeometry padded;
        if (config.inletBufferLayers > 0 || config.outletBufferLayers > 0)
        {
            padded = FiberFreeRegion::pad(geometry, dir,
                                          config.inletBufferLayers,
                                          config.outletBufferLayers);
            geomPtr = &padded.geometry;
        }

        // Build mesh
        HexMeshBuilder::Options meshOpts;
        meshOpts.voxelSize = config.voxelSize;
        meshOpts.flowDirection = dir;
        meshOpts.connectivityCheck = config.connectivityCheck;
        meshOpts.autoBoundaryFaceSets = true;
        meshOpts.periodic = config.periodic;
        if (config.inletBufferLayers > 0 || config.outletBufferLayers > 0)
            meshOpts.regionMask = padded.regionMask.data();

        HexMeshBuilder builder(*geomPtr, meshOpts);
        MeshData mesh = builder.build();

        Logger::info("Mesh: " + std::to_string(mesh.nCells) + " cells, " +
                     std::to_string(mesh.points.size()) + " points, " +
                     std::to_string(mesh.faces.size()) + " faces");

        // Map predicted velocity to mesh cells (buffer zones get zero velocity)
        mapPredictionToMesh(mesh, predictedVelocity, config.modelResolution,
                            *geomPtr, dir, config.inletBufferLayers,
                            config.outletBufferLayers, scale);

        // Write OpenFOAM case with predicted velocity field
        SimulationConfig dirConfig = config;
        dirConfig.flowDirections = {dir};
        FoamWriter writer(dirConfig);
        std::string caseDir = writer.writeCase(mesh, config.outputPath);
        Logger::info("Case written to: " + caseDir);
    }

    Logger::info("Total prediction pipeline completed in " +
                 std::to_string(totalTimer.elapsedMs()) + " ms");
    return 0;
}
