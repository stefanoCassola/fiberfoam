#include "ml/ModelRegistry.h"
#include "common/Logger.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <filesystem>
#include <regex>
#include <stdexcept>

namespace fs = std::filesystem;

namespace fiberfoam
{

// ---------------------------------------------------------------------------
// makeKey - build lookup key from resolution and direction
// ---------------------------------------------------------------------------
std::string ModelRegistry::makeKey(int resolution, FlowDirection direction)
{
    return "res" + std::to_string(resolution) + "_" + directionName(direction);
}

// ---------------------------------------------------------------------------
// fromYaml - parse a YAML config describing available models
//
//   models:
//     - resolution: 80
//       direction: x
//       path: res80/x_80.onnx
//     - resolution: 80
//       direction: y
//       path: res80/y_80.onnx
//     ...
//   modelsDir: /path/to/models   (optional base directory)
// ---------------------------------------------------------------------------
ModelRegistry ModelRegistry::fromYaml(const std::string& path)
{
    Logger::info("Loading model registry from " + path);

    YAML::Node root;
    try
    {
        root = YAML::LoadFile(path);
    }
    catch (const YAML::Exception& e)
    {
        throw std::runtime_error(
            "ModelRegistry::fromYaml: failed to load " + path + ": " + e.what());
    }

    ModelRegistry registry;

    // Optional base directory for model paths
    if (root["modelsDir"])
    {
        registry.modelsDir_ = root["modelsDir"].as<std::string>();
    }
    else
    {
        // Default to the directory containing the YAML file
        registry.modelsDir_ = fs::path(path).parent_path().string();
    }

    if (!root["models"] || !root["models"].IsSequence())
    {
        throw std::runtime_error(
            "ModelRegistry::fromYaml: missing or invalid 'models' sequence in " + path);
    }

    for (const auto& entry : root["models"])
    {
        ModelInfo info;

        if (!entry["resolution"])
            throw std::runtime_error("ModelRegistry::fromYaml: missing 'resolution' field");
        if (!entry["direction"])
            throw std::runtime_error("ModelRegistry::fromYaml: missing 'direction' field");
        if (!entry["path"])
            throw std::runtime_error("ModelRegistry::fromYaml: missing 'path' field");

        info.resolution = entry["resolution"].as<int>();
        info.direction = directionFromName(entry["direction"].as<std::string>());

        std::string modelPath = entry["path"].as<std::string>();
        // If relative, prepend modelsDir
        if (!fs::path(modelPath).is_absolute())
        {
            modelPath = (fs::path(registry.modelsDir_) / modelPath).string();
        }
        info.path = modelPath;

        std::string key = makeKey(info.resolution, info.direction);
        registry.models_[key] = std::move(info);

        Logger::debug("  Registered model: " + key + " -> " + registry.models_[key].path);
    }

    Logger::info("Model registry loaded with " + std::to_string(registry.models_.size()) +
                 " model(s)");
    return registry;
}

// ---------------------------------------------------------------------------
// fromDirectory - scan a directory for .onnx files and auto-detect by filename
//
//   Expected naming patterns:
//     x_80.onnx, y_80.onnx, z_80.onnx
//     x-80.onnx, model_x_80.onnx, etc.
//   The regex looks for an axis letter (x/y/z) followed by a resolution number.
// ---------------------------------------------------------------------------
ModelRegistry ModelRegistry::fromDirectory(const std::string& modelsDir, int resolution)
{
    Logger::info("Scanning models directory: " + modelsDir);

    if (!fs::exists(modelsDir) || !fs::is_directory(modelsDir))
    {
        throw std::runtime_error(
            "ModelRegistry::fromDirectory: directory does not exist: " + modelsDir);
    }

    ModelRegistry registry;
    registry.modelsDir_ = modelsDir;

    // Regex to extract axis letter and resolution from filenames
    // Matches patterns like: x_80, y-80, x80, model_x_80, etc.
    std::regex pattern(R"(([xyz])[_\-]?(\d+))", std::regex::icase);

    for (const auto& entry : fs::recursive_directory_iterator(modelsDir))
    {
        if (!entry.is_regular_file())
            continue;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".onnx")
            continue;

        std::string stem = entry.path().stem().string();
        std::smatch match;
        if (!std::regex_search(stem, match, pattern))
        {
            Logger::warning("  Skipping unrecognized ONNX file: " + entry.path().string());
            continue;
        }

        std::string axisStr = match[1].str();
        // Normalize to lowercase
        std::transform(axisStr.begin(), axisStr.end(), axisStr.begin(), ::tolower);

        int fileResolution = std::stoi(match[2].str());

        // Only register models matching the requested resolution
        if (fileResolution != resolution)
        {
            Logger::debug("  Skipping " + entry.path().string() +
                          " (resolution " + std::to_string(fileResolution) +
                          " != " + std::to_string(resolution) + ")");
            continue;
        }

        ModelInfo info;
        info.path = entry.path().string();
        info.resolution = fileResolution;
        info.direction = directionFromName(axisStr);

        std::string key = makeKey(info.resolution, info.direction);
        registry.models_[key] = std::move(info);

        Logger::info("  Found model: " + key + " -> " + registry.models_[key].path);
    }

    if (registry.models_.empty())
    {
        Logger::warning("ModelRegistry::fromDirectory: no .onnx models found for resolution " +
                        std::to_string(resolution) + " in " + modelsDir);
    }

    return registry;
}

// ---------------------------------------------------------------------------
// getModel
// ---------------------------------------------------------------------------
const ModelInfo& ModelRegistry::getModel(FlowDirection direction, int resolution) const
{
    std::string key = makeKey(resolution, direction);
    auto it = models_.find(key);
    if (it == models_.end())
    {
        throw std::runtime_error(
            "ModelRegistry::getModel: no model found for key '" + key + "'");
    }
    return it->second;
}

// ---------------------------------------------------------------------------
// hasModel
// ---------------------------------------------------------------------------
bool ModelRegistry::hasModel(FlowDirection direction, int resolution) const
{
    return models_.find(makeKey(resolution, direction)) != models_.end();
}

} // namespace fiberfoam
