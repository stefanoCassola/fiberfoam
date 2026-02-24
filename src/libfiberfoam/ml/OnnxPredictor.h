#pragma once

#include "common/Types.h"
#include "geometry/VoxelArray.h"
#include "ml/ModelRegistry.h"

#include <map>
#include <memory>
#include <vector>

#ifdef FIBERFOAM_HAS_ONNX
#include <onnxruntime_cxx_api.h>
#endif

namespace fiberfoam
{

class OnnxPredictor
{
public:
    explicit OnnxPredictor(const ModelRegistry& registry, int resolution = 80);
    ~OnnxPredictor();

    // Predict velocity field for a direction.
    // Input: geometry at any resolution (resampled to model resolution internally).
    // Output: predicted velocity at model resolution (needs upsampling by caller).
    std::vector<double> predict(const VoxelArray& geometry, FlowDirection direction);

    // Predict all 3 directions
    std::map<FlowDirection, std::vector<double>> predictAll(const VoxelArray& geometry);

private:
    void loadModel(FlowDirection direction);

    ModelRegistry registry_;
    int resolution_;

#ifdef FIBERFOAM_HAS_ONNX
    Ort::Env env_;
    std::map<FlowDirection, std::unique_ptr<Ort::Session>> sessions_;
#endif
};

} // namespace fiberfoam
