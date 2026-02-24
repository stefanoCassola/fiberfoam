#include "ml/Scaling.h"
#include "common/Logger.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace fiberfoam
{

// ---------------------------------------------------------------------------
// makeKey - build lookup key from resolution and direction
// ---------------------------------------------------------------------------
std::string ScalingFactors::makeKey(int resolution, FlowDirection direction)
{
    return "res" + std::to_string(resolution) + "_" + directionName(direction);
}

// ---------------------------------------------------------------------------
// fromJson - parse the scaling factors JSON file
//
//   Expected format (matches Python get_scaling_factor.py):
//   {
//       "res80": [
//           {"x": {"max velocity": 1.234}},
//           {"y": {"max velocity": 2.345}},
//           {"z": {"max velocity": 3.456}}
//       ],
//       "res160": [ ... ],
//       "res320": [ ... ]
//   }
// ---------------------------------------------------------------------------
ScalingFactors ScalingFactors::fromJson(const std::string& path)
{
    Logger::info("Loading scaling factors from " + path);

    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        throw std::runtime_error(
            "ScalingFactors::fromJson: cannot open file: " + path);
    }

    nlohmann::json root;
    try
    {
        ifs >> root;
    }
    catch (const nlohmann::json::exception& e)
    {
        throw std::runtime_error(
            "ScalingFactors::fromJson: JSON parse error in " + path + ": " + e.what());
    }

    ScalingFactors factors;

    // Iterate over resolution keys like "res80", "res160", "res320"
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        const std::string& resKey = it.key(); // e.g. "res80"

        // Extract numeric resolution from key
        // Expected format: "res{N}"
        if (resKey.substr(0, 3) != "res")
        {
            Logger::warning("ScalingFactors::fromJson: skipping unrecognized key '" +
                            resKey + "'");
            continue;
        }

        int resolution = 0;
        try
        {
            resolution = std::stoi(resKey.substr(3));
        }
        catch (const std::exception&)
        {
            Logger::warning("ScalingFactors::fromJson: cannot parse resolution from '" +
                            resKey + "'");
            continue;
        }

        if (!it->is_array())
        {
            Logger::warning("ScalingFactors::fromJson: '" + resKey +
                            "' is not an array, skipping");
            continue;
        }

        // Each array element is an object like {"x": {"max velocity": val}}
        for (const auto& entry : *it)
        {
            if (!entry.is_object())
                continue;

            for (auto axisIt = entry.begin(); axisIt != entry.end(); ++axisIt)
            {
                const std::string& axisName = axisIt.key(); // "x", "y", or "z"

                // Validate axis name
                if (axisName != "x" && axisName != "y" && axisName != "z")
                {
                    Logger::warning("ScalingFactors::fromJson: skipping unknown axis '" +
                                    axisName + "'");
                    continue;
                }

                FlowDirection dir = directionFromName(axisName);

                if (!axisIt->is_object() || !axisIt->contains("max velocity"))
                {
                    Logger::warning("ScalingFactors::fromJson: missing 'max velocity' for " +
                                    resKey + "/" + axisName);
                    continue;
                }

                double maxVelocity = (*axisIt)["max velocity"].get<double>();

                std::string key = makeKey(resolution, dir);
                factors.factors_[key] = maxVelocity;

                Logger::debug("  Scaling factor: " + key + " = " +
                              std::to_string(maxVelocity));
            }
        }
    }

    Logger::info("Loaded " + std::to_string(factors.factors_.size()) + " scaling factor(s)");
    return factors;
}

// ---------------------------------------------------------------------------
// getFactor
// ---------------------------------------------------------------------------
double ScalingFactors::getFactor(FlowDirection direction, int resolution) const
{
    std::string key = makeKey(resolution, direction);
    auto it = factors_.find(key);
    if (it == factors_.end())
    {
        throw std::runtime_error(
            "ScalingFactors::getFactor: no scaling factor found for '" + key + "'");
    }
    return it->second;
}

// ---------------------------------------------------------------------------
// hasFactor
// ---------------------------------------------------------------------------
bool ScalingFactors::hasFactor(FlowDirection direction, int resolution) const
{
    return factors_.find(makeKey(resolution, direction)) != factors_.end();
}

} // namespace fiberfoam
