#include "common/Types.h"
#include "common/Logger.h"
#include "common/Timer.h"
#include "config/SimulationConfig.h"
#include "io/FoamReader.h"
#include "io/CsvWriter.h"
#include "postprocessing/Permeability.h"
#include <iostream>
#include <string>
#include <vector>

using namespace fiberfoam;

void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  -case <path>           Path to OpenFOAM case directory\n"
              << "  -method <method>       Permeability method: volumeAveraged, flowRate, or both\n"
              << "  -fibrousRegionOnly     Compute over fibrous region only (default: on)\n"
              << "  -fullDomain            Compute over full domain including buffers\n"
              << "  -output <path>         Output CSV file path (default: permeability.csv)\n"
              << "  -flowDirection <dir>   Flow direction: x, y, or z (auto-detected if omitted)\n"
              << "  -time <dir>            Time directory to read (default: latest)\n"
              << "  -config <path>         Load settings from YAML config\n"
              << std::endl;
}

int main(int argc, char* argv[])
{
    std::string casePath;
    std::string method = "both";
    bool fibrousRegionOnly = true;
    std::string outputPath = "permeability.csv";
    std::string flowDir;
    std::string timeDir = "latestTime";
    SimulationConfig config;

    // Parse arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-case" && i + 1 < argc)
            casePath = argv[++i];
        else if (arg == "-method" && i + 1 < argc)
            method = argv[++i];
        else if (arg == "-fibrousRegionOnly")
            fibrousRegionOnly = true;
        else if (arg == "-fullDomain")
            fibrousRegionOnly = false;
        else if (arg == "-output" && i + 1 < argc)
            outputPath = argv[++i];
        else if (arg == "-flowDirection" && i + 1 < argc)
            flowDir = argv[++i];
        else if (arg == "-time" && i + 1 < argc)
            timeDir = argv[++i];
        else if (arg == "-config" && i + 1 < argc)
            config = SimulationConfig::fromYaml(argv[++i]);
        else if (arg == "-help" || arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (casePath.empty())
    {
        Logger::error("No case path specified. Use -case <path>.");
        printUsage(argv[0]);
        return 1;
    }

    Logger::info("fiberFoamPostProcess - Permeability Calculation");
    Logger::info("Case: " + casePath);
    Logger::info("Method: " + method);
    Logger::info("Fibrous region only: " + std::string(fibrousRegionOnly ? "yes" : "no"));

    Timer totalTimer("Total post-processing");

    // Read fields from the case
    FoamReader reader(casePath);

    std::string resolvedTimeDir = timeDir;
    if (timeDir == "latestTime")
    {
        resolvedTimeDir = reader.findLatestTime();
    }
    Logger::info("Reading fields from time: " + resolvedTimeDir);

    // Read velocity field
    std::vector<std::array<double, 3>> velocities = reader.readVelocity(resolvedTimeDir);
    Logger::info("Velocity field: " + std::to_string(velocities.size()) + " cells");

    // Read pressure field
    std::vector<double> pressure = reader.readPressure(resolvedTimeDir);
    Logger::info("Pressure field: " + std::to_string(pressure.size()) + " cells");

    // Read outlet flux for flow-rate based permeability
    double outletFlux = 0.0;
    if (method == "flowRate" || method == "both")
    {
        outletFlux = reader.readOutletFlux(resolvedTimeDir);
        Logger::info("Outlet flux: " + std::to_string(outletFlux) + " m3/s");
    }

    // Determine flow direction
    FlowDirection direction = FlowDirection::X;
    if (!flowDir.empty())
    {
        direction = directionFromName(flowDir);
    }
    else
    {
        // Auto-detect from average velocity: largest component is main flow direction
        double avgU = 0, avgV = 0, avgW = 0;
        for (const auto& vel : velocities)
        {
            avgU += std::abs(vel[0]);
            avgV += std::abs(vel[1]);
            avgW += std::abs(vel[2]);
        }
        if (avgU >= avgV && avgU >= avgW)
            direction = FlowDirection::X;
        else if (avgV >= avgU && avgV >= avgW)
            direction = FlowDirection::Y;
        else
            direction = FlowDirection::Z;
        Logger::info("Auto-detected flow direction: " + directionName(direction));
    }

    // Set up permeability calculator
    PermeabilityCalculator::Options permOpts;
    permOpts.fluid = config.fluid;
    permOpts.fibrousRegionOnly = fibrousRegionOnly;

    PermeabilityCalculator calculator(permOpts);

    // Compute cell centers (placeholder: need mesh info in real implementation)
    // In practice, cell centers would be read from the mesh or computed
    std::vector<std::array<double, 3>> cellCenters;
    cellCenters.resize(velocities.size(), {0.0, 0.0, 0.0});

    // Compute mesh volume (placeholder)
    double meshVolume = 0.0;

    // Compute permeability
    PermeabilityResult result = calculator.compute(
        velocities, cellCenters, meshVolume, direction, outletFlux);

    // Report results
    Logger::info("--- Permeability Results ---");
    Logger::info("Flow direction: " + directionName(result.direction));
    Logger::info("Perm (volume-averaged, main): " + std::to_string(result.permVolAvgMain) + " m2");
    Logger::info("Perm (volume-averaged, secondary): " + std::to_string(result.permVolAvgSecondary) + " m2");
    Logger::info("Perm (volume-averaged, tertiary): " + std::to_string(result.permVolAvgTertiary) + " m2");
    Logger::info("Perm (flow rate): " + std::to_string(result.permFlowRate) + " m2");
    Logger::info("Fiber volume content: " + std::to_string(result.fiberVolumeContent) + "%");

    // Write CSV output
    CsvWriter::writePermeability(result, outputPath);
    Logger::info("Results written to: " + outputPath);

    return 0;
}
