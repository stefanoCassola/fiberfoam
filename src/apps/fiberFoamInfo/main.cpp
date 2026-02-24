#include "common/Logger.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using namespace fiberfoam;

#ifndef FIBERFOAM_VERSION
#define FIBERFOAM_VERSION "0.1.0-dev"
#endif

#ifndef FIBERFOAM_BUILD_TYPE
#define FIBERFOAM_BUILD_TYPE "Unknown"
#endif

#ifndef FIBERFOAM_COMPILER
#define FIBERFOAM_COMPILER "Unknown"
#endif

std::string detectOpenFOAMInstallation()
{
    const char* wmProjectDir = std::getenv("WM_PROJECT_DIR");
    if (wmProjectDir)
        return std::string(wmProjectDir);

    const char* foamEtc = std::getenv("FOAM_ETC");
    if (foamEtc)
        return std::string(foamEtc) + "/..";

    return "Not detected (source OpenFOAM environment first)";
}

std::string detectOpenFOAMVersion()
{
    const char* wmProjectVersion = std::getenv("WM_PROJECT_VERSION");
    if (wmProjectVersion)
        return std::string(wmProjectVersion);
    return "Unknown";
}

std::string detectOnnxRuntime()
{
#ifdef FIBERFOAM_HAS_ONNX
    return "Available (compiled with ONNX Runtime support)";
#else
    return "Not available (compiled without ONNX Runtime support)";
#endif
}

std::string getUserAppBin()
{
    const char* foamUserAppbin = std::getenv("FOAM_USER_APPBIN");
    if (foamUserAppbin)
        return std::string(foamUserAppbin);
    return "Not set";
}

void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  -version              Print version only\n"
              << "  -openfoam             Print OpenFOAM info only\n"
              << "  -models <dir>         Check models directory\n"
              << "  -help                 Show this help message\n"
              << std::endl;
}

int main(int argc, char* argv[])
{
    bool versionOnly = false;
    bool openfoamOnly = false;
    std::string modelsDir;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-version")
            versionOnly = true;
        else if (arg == "-openfoam")
            openfoamOnly = true;
        else if (arg == "-models" && i + 1 < argc)
            modelsDir = argv[++i];
        else if (arg == "-help" || arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (versionOnly)
    {
        std::cout << FIBERFOAM_VERSION << std::endl;
        return 0;
    }

    if (openfoamOnly)
    {
        std::cout << "OpenFOAM: " << detectOpenFOAMInstallation() << std::endl;
        std::cout << "Version: " << detectOpenFOAMVersion() << std::endl;
        return 0;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  fiberFoam - Information" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::cout << "Version:          " << FIBERFOAM_VERSION << std::endl;
    std::cout << "Build type:       " << FIBERFOAM_BUILD_TYPE << std::endl;
    std::cout << "Compiler:         " << FIBERFOAM_COMPILER << std::endl;
    std::cout << std::endl;

    std::cout << "--- OpenFOAM Environment ---" << std::endl;
    std::cout << "Installation:     " << detectOpenFOAMInstallation() << std::endl;
    std::cout << "Version:          " << detectOpenFOAMVersion() << std::endl;
    std::cout << "User appbin:      " << getUserAppBin() << std::endl;
    std::cout << std::endl;

    std::cout << "--- ML Support ---" << std::endl;
    std::cout << "ONNX Runtime:     " << detectOnnxRuntime() << std::endl;
    std::cout << std::endl;

    std::cout << "--- Available Commands ---" << std::endl;
    std::cout << "  fiberFoamMesh          Generate hex mesh from voxel geometry" << std::endl;
    std::cout << "  fiberFoamPredict       ML-accelerated velocity prediction" << std::endl;
    std::cout << "  fiberFoamPostProcess   Compute permeability from results" << std::endl;
    std::cout << "  fiberFoamRun           Full pipeline orchestrator" << std::endl;
    std::cout << "  fiberFoamConvertModel  TF to ONNX model conversion guide" << std::endl;
    std::cout << "  fiberFoamInfo          This information utility" << std::endl;
    std::cout << "  simpleFoamMod          Modified OpenFOAM solver" << std::endl;
    std::cout << std::endl;

    if (!modelsDir.empty())
    {
        std::cout << "--- Models Directory: " << modelsDir << " ---" << std::endl;
        // Check if directory exists and list .onnx files
        std::string checkCmd = "ls -la " + modelsDir + "/*.onnx 2>/dev/null";
        int ret = std::system(checkCmd.c_str());
        if (ret != 0)
        {
            std::cout << "  No .onnx files found in " << modelsDir << std::endl;
        }
        std::cout << std::endl;
    }

    return 0;
}
