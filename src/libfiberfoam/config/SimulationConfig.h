#pragma once

#include "common/Types.h"
#include <string>
#include <vector>

namespace fiberfoam
{

struct SimulationConfig
{
    // Geometry
    std::string inputPath;
    int voxelResolution = 320;
    double voxelSize = 0.5e-6;

    // Flow
    std::vector<FlowDirection> flowDirections = {FlowDirection::X};
    FluidProperties fluid;

    // Buffer zones
    int inletBufferLayers = 0;
    int outletBufferLayers = 0;

    // Mesh generation
    bool connectivityCheck = true;
    bool autoBoundaryFaceSets = true;
    bool periodic = false;

    // ML prediction
    bool enablePrediction = false;
    std::string modelsDir;
    int modelResolution = 80;

    // Solver
    std::string solverName = "simpleFoamMod";
    int maxIterations = 1000000;
    int writeInterval = 50000;

    // Permeability convergence
    bool convPermeability = true;
    double convSlope = 0.01;
    int convWindow = 10;
    double errorBound = 0.01;

    // Post-processing
    bool fibrousRegionOnly = true;
    std::string permeabilityMethod = "both"; // "volumeAveraged", "flowRate", "both"

    // Output
    std::string outputPath;

    // Load from YAML file
    static SimulationConfig fromYaml(const std::string& path);

    // Save to YAML file
    void toYaml(const std::string& path) const;
};

} // namespace fiberfoam
