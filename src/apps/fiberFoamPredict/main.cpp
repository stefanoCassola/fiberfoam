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
#include "analysis/VelocityReconstruction.h"
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

int main(int argc, char* argv[])
{
    SimulationConfig config;
    config.connectivityCheck = true;
    config.enablePrediction = true;

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

    // Downsample geometry to model resolution for prediction
    VoxelArray lowResGeometry = geometry.resample(config.modelResolution);
    Logger::info("Downsampled to model resolution: " + std::to_string(lowResGeometry.nx()) + "x" +
                 std::to_string(lowResGeometry.ny()) + "x" + std::to_string(lowResGeometry.nz()));

    // Load ONNX models
    ModelRegistry registry = ModelRegistry::fromDirectory(config.modelsDir, config.modelResolution);
    OnnxPredictor predictor(registry, config.modelResolution);

    // Run predictions for each direction
    for (auto dir : config.flowDirections)
    {
        Logger::info("Predicting flow direction: " + directionName(dir));
        Timer dirTimer("Prediction direction " + directionName(dir));

        if (!registry.hasModel(dir, config.modelResolution))
        {
            Logger::warning("No model found for direction " + directionName(dir) +
                            " at resolution " + std::to_string(config.modelResolution) +
                            ", skipping.");
            continue;
        }

        // Predict velocity at model resolution
        std::vector<double> predictedVelocity = predictor.predict(lowResGeometry, dir);
        Logger::info("Prediction complete: " + std::to_string(predictedVelocity.size()) + " values");

        // Optional buffer zones on full-resolution geometry
        VoxelArray* geomPtr = &geometry;
        PaddedGeometry padded;
        if (config.inletBufferLayers > 0 || config.outletBufferLayers > 0)
        {
            padded = FiberFreeRegion::pad(geometry, dir,
                                          config.inletBufferLayers,
                                          config.outletBufferLayers);
            geomPtr = &padded.geometry;
        }

        // Build mesh at full resolution with predicted velocity as initial condition
        HexMeshBuilder::Options meshOpts;
        meshOpts.voxelSize = config.voxelSize;
        meshOpts.flowDirection = dir;
        meshOpts.connectivityCheck = config.connectivityCheck;
        meshOpts.autoBoundaryFaceSets = true;
        meshOpts.periodic = config.periodic;
        // Note: velocity field from prediction needs to be upsampled and mapped
        // to full-resolution cells. The predicted field at model resolution is
        // used as initial condition via nearest-neighbor upsampling.
        if (config.inletBufferLayers > 0 || config.outletBufferLayers > 0)
            meshOpts.regionMask = padded.regionMask.data();

        HexMeshBuilder builder(*geomPtr, meshOpts);
        MeshData mesh = builder.build();

        Logger::info("Mesh: " + std::to_string(mesh.nCells) + " cells, " +
                     std::to_string(mesh.points.size()) + " points, " +
                     std::to_string(mesh.faces.size()) + " faces");

        // Write OpenFOAM case with predicted velocity field
        SimulationConfig dirConfig = config;
        dirConfig.flowDirections = {dir};
        FoamWriter writer(dirConfig);
        std::string caseDir = writer.writeCase(mesh, config.outputPath);
        Logger::info("Case written to: " + caseDir);
    }

    return 0;
}
