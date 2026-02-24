#include "common/Logger.h"
#include <iostream>
#include <string>

using namespace fiberfoam;

void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  -tfModel <path>       Path to TensorFlow SavedModel directory\n"
              << "  -output <path>        Output .onnx file path\n"
              << "  -opset <int>          ONNX opset version (default: 13)\n"
              << "  -help                 Show this help message\n"
              << std::endl;
}

int main(int argc, char* argv[])
{
    std::string tfModelPath;
    std::string outputPath;
    int opset = 13;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-tfModel" && i + 1 < argc)
            tfModelPath = argv[++i];
        else if (arg == "-output" && i + 1 < argc)
            outputPath = argv[++i];
        else if (arg == "-opset" && i + 1 < argc)
            opset = std::stoi(argv[++i]);
        else if (arg == "-help" || arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    Logger::info("fiberFoamConvertModel - TensorFlow to ONNX Model Converter");
    Logger::info("");
    Logger::info("This utility provides instructions for converting TensorFlow");
    Logger::info("SavedModel models to ONNX format for use with fiberFoamPredict.");
    Logger::info("");
    Logger::info("Prerequisites:");
    Logger::info("  - Python 3.8+ with pip");
    Logger::info("  - Install tf2onnx: pip install tf2onnx");
    Logger::info("  - Install tensorflow: pip install tensorflow");
    Logger::info("");
    Logger::info("Conversion command:");
    Logger::info("  python -m tf2onnx.convert \\");
    Logger::info("    --saved-model <path_to_saved_model_dir> \\");
    Logger::info("    --output <output_model.onnx> \\");
    Logger::info("    --opset " + std::to_string(opset));
    Logger::info("");

    if (!tfModelPath.empty() && !outputPath.empty())
    {
        Logger::info("For your specific model:");
        Logger::info("  python -m tf2onnx.convert \\");
        Logger::info("    --saved-model " + tfModelPath + " \\");
        Logger::info("    --output " + outputPath + " \\");
        Logger::info("    --opset " + std::to_string(opset));
        Logger::info("");
    }

    Logger::info("After conversion, place the .onnx files in a models directory");
    Logger::info("with the following naming convention:");
    Logger::info("  models/");
    Logger::info("    velocity_x_res80.onnx");
    Logger::info("    velocity_y_res80.onnx");
    Logger::info("    velocity_z_res80.onnx");
    Logger::info("    scaling_factors.json");
    Logger::info("");
    Logger::info("Then use fiberFoamPredict -modelsDir models/ to run predictions.");

    return 0;
}
