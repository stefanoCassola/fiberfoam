#pragma once

#include "common/Types.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace fiberfoam
{

class VoxelArray
{
public:
    // Load from flat text file (.dat) and reshape to nx x ny x nz
    static VoxelArray fromDatFile(const std::string& path, int resolution);

    // Load from .npy format (NumPy binary)
    static VoxelArray fromNpy(const std::string& path);

    // Create from raw data
    VoxelArray(std::vector<int8_t> data, int nx, int ny, int nz);

    // Default (empty)
    VoxelArray() : nx_(0), ny_(0), nz_(0) {}

    int nx() const { return nx_; }
    int ny() const { return ny_; }
    int nz() const { return nz_; }
    int size() const { return nx_ * ny_ * nz_; }

    int8_t& at(int x, int y, int z) { return data_[x + nx_ * (y + ny_ * z)]; }
    int8_t at(int x, int y, int z) const { return data_[x + nx_ * (y + ny_ * z)]; }

    double fluidFraction() const;

    // Invert convention (0<->1) - in original Python code: 0=solid, 1=fluid -> swap
    void invertConvention();

    // Resample to target resolution using nearest-neighbor
    VoxelArray resample(int targetRes) const;

    const std::vector<int8_t>& data() const { return data_; }
    std::vector<int8_t>& data() { return data_; }

private:
    std::vector<int8_t> data_;
    int nx_, ny_, nz_;
};

} // namespace fiberfoam
