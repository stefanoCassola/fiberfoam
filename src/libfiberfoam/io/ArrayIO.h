#pragma once

#include "geometry/VoxelArray.h"
#include <string>
#include <vector>

namespace fiberfoam
{

namespace ArrayIO
{
    // Read flat text file (.dat) with values
    std::vector<double> readDatFile(const std::string& path);

    // Write flat text file
    void writeDatFile(const std::string& path, const std::vector<double>& data);

    // Read .npy file header to get shape and dtype
    struct NpyHeader
    {
        std::vector<int> shape;
        std::string dtype;
        bool fortranOrder;
    };
    NpyHeader readNpyHeader(const std::string& path);
} // namespace ArrayIO

} // namespace fiberfoam
