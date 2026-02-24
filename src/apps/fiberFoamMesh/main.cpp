#include "common/Types.h"
#include "common/Logger.h"
#include "common/Timer.h"
#include "config/SimulationConfig.h"
#include "geometry/VoxelArray.h"
#include "geometry/FiberFreeRegion.h"
#include "mesh/HexMeshBuilder.h"
#include "io/FoamWriter.h"
#include <iostream>
#include <string>
#include <vector>

using namespace fiberfoam;

void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  -input <path>         Input geometry file (.dat or .npy)\n"
              << "  -output <path>        Output case directory\n"
              << "  -voxelSize <value>    Voxel size in meters (e.g. 0.5e-6)\n"
              << "  -voxelRes <int>       Voxel resolution of input array\n"
              << "  -flowDirection <dir>  Flow direction: x, y, z, or all\n"
              << "  -inletBuffer <int>    Number of inlet buffer layers (default: 0)\n"
              << "  -outletBuffer <int>   Number of outlet buffer layers (default: 0)\n"
              << "  -connectivity         Enable connectivity check (default: on)\n"
              << "  -noConnectivity       Disable connectivity check\n"
              << "  -config <path>        Load settings from YAML config\n"
              << std::endl;
}

int main(int argc, char* argv[])
{
    SimulationConfig config;
    config.connectivityCheck = true;

    // Parse arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-input" && i + 1 < argc)
            config.inputPath = argv[++i];
        else if (arg == "-output" && i + 1 < argc)
            config.outputPath = argv[++i];
        else if (arg == "-voxelSize" && i + 1 < argc)
            config.voxelSize = std::stod(argv[++i]);
        else if (arg == "-voxelRes" && i + 1 < argc)
            config.voxelResolution = std::stoi(argv[++i]);
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

    Logger::info("fiberFoamMesh - Voxel to OpenFOAM Hex Mesh Converter");
    Logger::info("Input: " + config.inputPath);
    Logger::info("Output: " + config.outputPath);

    // Load geometry
    Timer totalTimer("Total mesh generation");
    VoxelArray geometry = VoxelArray::fromDatFile(config.inputPath, config.voxelResolution);
    Logger::info("Geometry loaded: " + std::to_string(geometry.nx()) + "x" +
                 std::to_string(geometry.ny()) + "x" + std::to_string(geometry.nz()));
    Logger::info("Fluid fraction: " + std::to_string(geometry.fluidFraction() * 100) + "%");

    // Process each flow direction
    for (auto dir : config.flowDirections)
    {
        Logger::info("Processing flow direction: " + directionName(dir));
        Timer dirTimer("Direction " + directionName(dir));

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

        // Write OpenFOAM case
        SimulationConfig dirConfig = config;
        dirConfig.flowDirections = {dir};
        FoamWriter writer(dirConfig);
        std::string caseDir = writer.writeCase(mesh, config.outputPath);
        Logger::info("Case written to: " + caseDir);
    }

    return 0;
}
