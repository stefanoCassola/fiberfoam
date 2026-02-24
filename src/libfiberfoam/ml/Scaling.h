#pragma once

#include "common/Types.h"
#include <map>
#include <string>

namespace fiberfoam
{

class ScalingFactors
{
public:
    // Load from JSON file
    //   Format: { "res80": [{"x": {"max velocity": val}},
    //                        {"y": {"max velocity": val}},
    //                        {"z": {"max velocity": val}}] }
    static ScalingFactors fromJson(const std::string& path);

    // Get scaling factor for a direction and resolution
    double getFactor(FlowDirection direction, int resolution = 80) const;

    bool hasFactor(FlowDirection direction, int resolution = 80) const;

private:
    // key = "res{N}_{dir}" e.g. "res80_x"
    std::map<std::string, double> factors_;

    static std::string makeKey(int resolution, FlowDirection direction);
};

} // namespace fiberfoam
