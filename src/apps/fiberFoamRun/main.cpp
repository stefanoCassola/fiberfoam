#include "common/Types.h"
#include "common/Logger.h"
#include "common/Timer.h"
#include "config/SimulationConfig.h"
#include "geometry/VoxelArray.h"
#include "geometry/FiberFreeRegion.h"
#include "mesh/HexMeshBuilder.h"
#include "io/FoamWriter.h"
#include "io/CsvWriter.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace fiberfoam;

void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  -config <path>         YAML configuration file (required)\n"
              << "  -mesh                  Run mesh generation step\n"
              << "  -predict               Run ML prediction step (requires ONNX models)\n"
              << "  -solve                 Run OpenFOAM solver step\n"
              << "  -postProcess           Run post-processing step\n"
              << "  -all                   Run all steps (mesh + solve + postProcess)\n"
              << "  -input <path>          Override input geometry path\n"
              << "  -output <path>         Override output directory\n"
              << "  -flowDirection <dir>   Override flow direction: x, y, z, or all\n"
              << std::endl;
}

int runMeshGeneration(const SimulationConfig& config)
{
    Logger::info("=== Step 1: Mesh Generation ===");
    Timer timer("Mesh generation");

    VoxelArray geometry = VoxelArray::fromDatFile(config.inputPath, config.voxelResolution);
    Logger::info("Geometry loaded: " + std::to_string(geometry.nx()) + "x" +
                 std::to_string(geometry.ny()) + "x" + std::to_string(geometry.nz()));

    for (auto dir : config.flowDirections)
    {
        Logger::info("Processing direction: " + directionName(dir));

        VoxelArray* geomPtr = &geometry;
        PaddedGeometry padded;
        if (config.inletBufferLayers > 0 || config.outletBufferLayers > 0)
        {
            padded = FiberFreeRegion::pad(geometry, dir,
                                          config.inletBufferLayers,
                                          config.outletBufferLayers);
            geomPtr = &padded.geometry;
        }

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

        Logger::info("Mesh: " + std::to_string(mesh.nCells) + " cells");

        SimulationConfig dirConfig = config;
        dirConfig.flowDirections = {dir};
        FoamWriter writer(dirConfig);
        std::string caseDir = writer.writeCase(mesh, config.outputPath);
        Logger::info("Case written to: " + caseDir);
    }

    return 0;
}

int runSolver(const SimulationConfig& config)
{
    Logger::info("=== Step 2: OpenFOAM Solver ===");

    for (auto dir : config.flowDirections)
    {
        std::string caseDir = config.outputPath + "/" + directionName(dir);
        std::string solverCmd = config.solverName + " -case " + caseDir;
        Logger::info("Running: " + solverCmd);

        int ret = std::system(solverCmd.c_str());
        if (ret != 0)
        {
            Logger::error("Solver failed for direction " + directionName(dir) +
                          " with exit code " + std::to_string(ret));
            return ret;
        }
        Logger::info("Solver completed for direction: " + directionName(dir));
    }

    return 0;
}

int runPostProcessing(const SimulationConfig& config)
{
    Logger::info("=== Step 3: Post-Processing ===");

    for (auto dir : config.flowDirections)
    {
        std::string caseDir = config.outputPath + "/" + directionName(dir);
        std::string postCmd = "fiberFoamPostProcess"
                              " -case " + caseDir +
                              " -method " + config.permeabilityMethod +
                              " -flowDirection " + directionName(dir) +
                              " -output " + caseDir + "/" + directionName(dir) + "Permeability.csv";
        if (config.fibrousRegionOnly)
            postCmd += " -fibrousRegionOnly";
        else
            postCmd += " -fullDomain";

        Logger::info("Running: " + postCmd);
        int ret = std::system(postCmd.c_str());
        if (ret != 0)
        {
            Logger::error("Post-processing failed for direction " + directionName(dir) +
                          " with exit code " + std::to_string(ret));
            return ret;
        }
        Logger::info("Post-processing completed for direction: " + directionName(dir));
    }

    return 0;
}

int main(int argc, char* argv[])
{
    bool doMesh = false;
    bool doPredict = false;
    bool doSolve = false;
    bool doPostProcess = false;
    std::string configPath;
    SimulationConfig config;

    // Parse arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-config" && i + 1 < argc)
            configPath = argv[++i];
        else if (arg == "-mesh")
            doMesh = true;
        else if (arg == "-predict")
            doPredict = true;
        else if (arg == "-solve")
            doSolve = true;
        else if (arg == "-postProcess")
            doPostProcess = true;
        else if (arg == "-all")
        {
            doMesh = true;
            doSolve = true;
            doPostProcess = true;
        }
        else if (arg == "-input" && i + 1 < argc)
            config.inputPath = argv[++i];
        else if (arg == "-output" && i + 1 < argc)
            config.outputPath = argv[++i];
        else if (arg == "-flowDirection" && i + 1 < argc)
        {
            std::string dir = argv[++i];
            config.flowDirections.clear();
            if (dir == "all")
                config.flowDirections = {FlowDirection::X, FlowDirection::Y, FlowDirection::Z};
            else
                config.flowDirections.push_back(directionFromName(dir));
        }
        else if (arg == "-help" || arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    // Load config from file, then override with CLI args
    if (!configPath.empty())
    {
        SimulationConfig fileConfig = SimulationConfig::fromYaml(configPath);
        // Override only non-default values from CLI
        if (!config.inputPath.empty())
            fileConfig.inputPath = config.inputPath;
        if (!config.outputPath.empty())
            fileConfig.outputPath = config.outputPath;
        if (!config.flowDirections.empty())
            fileConfig.flowDirections = config.flowDirections;
        config = fileConfig;
    }

    if (configPath.empty() && config.inputPath.empty())
    {
        Logger::error("No configuration specified. Use -config <path> or -input/-output.");
        printUsage(argv[0]);
        return 1;
    }

    if (!doMesh && !doPredict && !doSolve && !doPostProcess)
    {
        Logger::error("No steps selected. Use -mesh, -solve, -postProcess, -predict, or -all.");
        printUsage(argv[0]);
        return 1;
    }

    Logger::info("fiberFoamRun - Full Pipeline Orchestrator");
    Logger::info("Input: " + config.inputPath);
    Logger::info("Output: " + config.outputPath);

    Timer totalTimer("Full pipeline");
    int ret = 0;

    // Step 1: Mesh generation
    if (doMesh && ret == 0)
    {
        ret = runMeshGeneration(config);
    }

    // Step 1b: ML prediction (optional, runs fiberFoamPredict)
    if (doPredict && ret == 0)
    {
        Logger::info("=== Step 1b: ML Prediction ===");
        std::string predictCmd = "fiberFoamPredict"
                                 " -input " + config.inputPath +
                                 " -output " + config.outputPath +
                                 " -modelsDir " + config.modelsDir +
                                 " -voxelRes " + std::to_string(config.voxelResolution) +
                                 " -modelRes " + std::to_string(config.modelResolution);
        for (auto dir : config.flowDirections)
            predictCmd += " -flowDirection " + directionName(dir);

        Logger::info("Running: " + predictCmd);
        ret = std::system(predictCmd.c_str());
        if (ret != 0)
            Logger::error("Prediction failed with exit code " + std::to_string(ret));
    }

    // Step 2: Run solver
    if (doSolve && ret == 0)
    {
        ret = runSolver(config);
    }

    // Step 3: Post-processing
    if (doPostProcess && ret == 0)
    {
        ret = runPostProcessing(config);
    }

    if (ret == 0)
        Logger::info("Pipeline completed successfully.");
    else
        Logger::error("Pipeline failed at one or more steps.");

    return ret;
}
