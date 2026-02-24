#pragma once

#include "common/Types.h"
#include <map>
#include <string>

namespace fiberfoam
{

struct ModelInfo
{
    std::string path;        // path to .onnx file
    int resolution;          // model resolution (e.g. 80)
    FlowDirection direction; // x, y, or z
};

class ModelRegistry
{
public:
    // Load registry from YAML config
    static ModelRegistry fromYaml(const std::string& path);

    // Load from a models directory (auto-detect files)
    static ModelRegistry fromDirectory(const std::string& modelsDir, int resolution = 80);

    // Get model info for a direction
    const ModelInfo& getModel(FlowDirection direction, int resolution = 80) const;

    bool hasModel(FlowDirection direction, int resolution = 80) const;

    const std::string& modelsDir() const { return modelsDir_; }

private:
    std::string modelsDir_;
    // key = "res{N}_{dir}" e.g. "res80_x"
    std::map<std::string, ModelInfo> models_;

    static std::string makeKey(int resolution, FlowDirection direction);
};

} // namespace fiberfoam
