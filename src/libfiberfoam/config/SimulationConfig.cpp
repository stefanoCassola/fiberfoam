#include "config/SimulationConfig.h"
#include "common/Logger.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <stdexcept>

namespace fiberfoam
{

SimulationConfig SimulationConfig::fromYaml(const std::string& path)
{
    Logger::info("Loading configuration from " + path);

    YAML::Node root;
    try
    {
        root = YAML::LoadFile(path);
    }
    catch (const YAML::Exception& e)
    {
        throw std::runtime_error("Failed to load config file: " + std::string(e.what()));
    }

    SimulationConfig config;

    // Geometry section
    if (auto geom = root["geometry"])
    {
        if (geom["input"])
            config.inputPath = geom["input"].as<std::string>();
        if (geom["voxelResolution"])
            config.voxelResolution = geom["voxelResolution"].as<int>();
        if (geom["voxelSize"])
            config.voxelSize = geom["voxelSize"].as<double>();
    }

    // Flow section
    if (auto flow = root["flow"])
    {
        if (flow["directions"])
        {
            config.flowDirections.clear();
            for (const auto& d : flow["directions"])
            {
                config.flowDirections.push_back(directionFromName(d.as<std::string>()));
            }
        }

        if (auto fluid = flow["fluid"])
        {
            if (fluid["kinematicViscosity"])
                config.fluid.kinematicViscosity = fluid["kinematicViscosity"].as<double>();
            if (fluid["density"])
                config.fluid.density = fluid["density"].as<double>();
            if (fluid["dynamicViscosity"])
                config.fluid.dynamicViscosity = fluid["dynamicViscosity"].as<double>();
            if (fluid["pressureInlet"])
                config.fluid.pressureInlet = fluid["pressureInlet"].as<double>();
            if (fluid["pressureOutlet"])
                config.fluid.pressureOutlet = fluid["pressureOutlet"].as<double>();
        }
    }

    // Buffer zones
    if (auto buffer = root["bufferZones"])
    {
        if (buffer["inletLayers"])
            config.inletBufferLayers = buffer["inletLayers"].as<int>();
        if (buffer["outletLayers"])
            config.outletBufferLayers = buffer["outletLayers"].as<int>();
    }

    // Mesh section
    if (auto mesh = root["mesh"])
    {
        if (mesh["connectivityCheck"])
            config.connectivityCheck = mesh["connectivityCheck"].as<bool>();
        if (mesh["autoBoundaryFaceSets"])
            config.autoBoundaryFaceSets = mesh["autoBoundaryFaceSets"].as<bool>();
        if (mesh["periodic"])
            config.periodic = mesh["periodic"].as<bool>();
    }

    // ML prediction section
    if (auto ml = root["prediction"])
    {
        if (ml["enabled"])
            config.enablePrediction = ml["enabled"].as<bool>();
        if (ml["modelsDir"])
            config.modelsDir = ml["modelsDir"].as<std::string>();
        if (ml["modelResolution"])
            config.modelResolution = ml["modelResolution"].as<int>();
    }

    // Solver section
    if (auto solver = root["solver"])
    {
        if (solver["name"])
            config.solverName = solver["name"].as<std::string>();
        if (solver["maxIterations"])
            config.maxIterations = solver["maxIterations"].as<int>();
        if (solver["writeInterval"])
            config.writeInterval = solver["writeInterval"].as<int>();

        if (auto conv = solver["convergence"])
        {
            if (conv["enabled"])
                config.convPermeability = conv["enabled"].as<bool>();
            if (conv["slope"])
                config.convSlope = conv["slope"].as<double>();
            if (conv["window"])
                config.convWindow = conv["window"].as<int>();
            if (conv["errorBound"])
                config.errorBound = conv["errorBound"].as<double>();
        }
    }

    // Post-processing section
    if (auto post = root["postProcessing"])
    {
        if (post["fibrousRegionOnly"])
            config.fibrousRegionOnly = post["fibrousRegionOnly"].as<bool>();
        if (post["method"])
            config.permeabilityMethod = post["method"].as<std::string>();
    }

    // Output section
    if (auto out = root["output"])
    {
        if (out["path"])
            config.outputPath = out["path"].as<std::string>();
    }

    return config;
}

void SimulationConfig::toYaml(const std::string& path) const
{
    YAML::Emitter out;
    out << YAML::BeginMap;

    // Geometry
    out << YAML::Key << "geometry" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "input" << YAML::Value << inputPath;
    out << YAML::Key << "voxelResolution" << YAML::Value << voxelResolution;
    out << YAML::Key << "voxelSize" << YAML::Value << voxelSize;
    out << YAML::EndMap;

    // Flow
    out << YAML::Key << "flow" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "directions" << YAML::Value << YAML::BeginSeq;
    for (auto d : flowDirections)
        out << directionName(d);
    out << YAML::EndSeq;
    out << YAML::Key << "fluid" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "kinematicViscosity" << YAML::Value << fluid.kinematicViscosity;
    out << YAML::Key << "density" << YAML::Value << fluid.density;
    out << YAML::Key << "dynamicViscosity" << YAML::Value << fluid.dynamicViscosity;
    out << YAML::Key << "pressureInlet" << YAML::Value << fluid.pressureInlet;
    out << YAML::Key << "pressureOutlet" << YAML::Value << fluid.pressureOutlet;
    out << YAML::EndMap;
    out << YAML::EndMap;

    // Buffer zones
    out << YAML::Key << "bufferZones" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "inletLayers" << YAML::Value << inletBufferLayers;
    out << YAML::Key << "outletLayers" << YAML::Value << outletBufferLayers;
    out << YAML::EndMap;

    // Mesh
    out << YAML::Key << "mesh" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "connectivityCheck" << YAML::Value << connectivityCheck;
    out << YAML::Key << "autoBoundaryFaceSets" << YAML::Value << autoBoundaryFaceSets;
    out << YAML::Key << "periodic" << YAML::Value << periodic;
    out << YAML::EndMap;

    // ML
    out << YAML::Key << "prediction" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "enabled" << YAML::Value << enablePrediction;
    out << YAML::Key << "modelsDir" << YAML::Value << modelsDir;
    out << YAML::Key << "modelResolution" << YAML::Value << modelResolution;
    out << YAML::EndMap;

    // Solver
    out << YAML::Key << "solver" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << solverName;
    out << YAML::Key << "maxIterations" << YAML::Value << maxIterations;
    out << YAML::Key << "writeInterval" << YAML::Value << writeInterval;
    out << YAML::Key << "convergence" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "enabled" << YAML::Value << convPermeability;
    out << YAML::Key << "slope" << YAML::Value << convSlope;
    out << YAML::Key << "window" << YAML::Value << convWindow;
    out << YAML::Key << "errorBound" << YAML::Value << errorBound;
    out << YAML::EndMap;
    out << YAML::EndMap;

    // Post-processing
    out << YAML::Key << "postProcessing" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "fibrousRegionOnly" << YAML::Value << fibrousRegionOnly;
    out << YAML::Key << "method" << YAML::Value << permeabilityMethod;
    out << YAML::EndMap;

    // Output
    out << YAML::Key << "output" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "path" << YAML::Value << outputPath;
    out << YAML::EndMap;

    out << YAML::EndMap;

    std::ofstream fout(path);
    if (!fout.is_open())
        throw std::runtime_error("Cannot write config to: " + path);
    fout << out.c_str();
}

} // namespace fiberfoam
