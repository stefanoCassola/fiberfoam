#include "geometry/VoxelArray.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace fiberfoam
{

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
VoxelArray::VoxelArray(std::vector<int8_t> data, int nx, int ny, int nz)
    : data_(std::move(data)), nx_(nx), ny_(ny), nz_(nz)
{
    if (static_cast<int>(data_.size()) != nx_ * ny_ * nz_)
    {
        throw std::runtime_error(
            "VoxelArray: data size (" + std::to_string(data_.size()) +
            ") does not match dimensions (" + std::to_string(nx_) + " x " +
            std::to_string(ny_) + " x " + std::to_string(nz_) + " = " +
            std::to_string(static_cast<long long>(nx_) * ny_ * nz_) + ")");
    }
}

// ---------------------------------------------------------------------------
// fromDatFile - load flat text of 0s and 1s, invert convention, reshape
// ---------------------------------------------------------------------------
VoxelArray VoxelArray::fromDatFile(const std::string& path, int resolution)
{
    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        throw std::runtime_error("VoxelArray::fromDatFile: cannot open file: " + path);
    }

    // Read all integer values from the file (space / newline separated)
    std::vector<int8_t> raw;
    raw.reserve(static_cast<size_t>(resolution) * resolution * resolution);

    int val = 0;
    while (ifs >> val)
    {
        raw.push_back(static_cast<int8_t>(val));
    }

    if (raw.empty())
    {
        throw std::runtime_error("VoxelArray::fromDatFile: no data read from " + path);
    }

    // Invert convention: the Python code does
    //   geom_array[geom_array == 0] = 3
    //   geom_array[geom_array == 1] = 0
    //   geom_array[geom_array == 3] = 1
    // Net effect: 0 -> 1, 1 -> 0  (swap 0 and 1)
    for (auto& v : raw)
    {
        if (v == 0)
            v = 1;
        else if (v == 1)
            v = 0;
    }

    // Reshape to (resolution, resolution, -1)
    // nx = ny = resolution, nz = total / (resolution * resolution)
    int nx = resolution;
    int ny = resolution;
    int total = static_cast<int>(raw.size());
    if (total % (nx * ny) != 0)
    {
        throw std::runtime_error(
            "VoxelArray::fromDatFile: total count (" + std::to_string(total) +
            ") is not divisible by resolution^2 (" + std::to_string(nx * ny) + ")");
    }
    int nz = total / (nx * ny);

    return VoxelArray(std::move(raw), nx, ny, nz);
}

// ---------------------------------------------------------------------------
// fromNpy - load NumPy .npy binary file
// ---------------------------------------------------------------------------
VoxelArray VoxelArray::fromNpy(const std::string& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
    {
        throw std::runtime_error("VoxelArray::fromNpy: cannot open file: " + path);
    }

    // --- Magic number: \x93NUMPY ---
    char magic[6];
    ifs.read(magic, 6);
    if (magic[0] != '\x93' || magic[1] != 'N' || magic[2] != 'U' ||
        magic[3] != 'M' || magic[4] != 'P' || magic[5] != 'Y')
    {
        throw std::runtime_error("VoxelArray::fromNpy: invalid magic bytes in " + path);
    }

    // --- Version ---
    uint8_t majorVersion = 0, minorVersion = 0;
    ifs.read(reinterpret_cast<char*>(&majorVersion), 1);
    ifs.read(reinterpret_cast<char*>(&minorVersion), 1);

    // --- Header length ---
    uint32_t headerLen = 0;
    if (majorVersion == 1)
    {
        uint16_t hl = 0;
        ifs.read(reinterpret_cast<char*>(&hl), 2);
        headerLen = hl;
    }
    else if (majorVersion == 2 || majorVersion == 3)
    {
        ifs.read(reinterpret_cast<char*>(&headerLen), 4);
    }
    else
    {
        throw std::runtime_error(
            "VoxelArray::fromNpy: unsupported npy version " +
            std::to_string(majorVersion) + "." + std::to_string(minorVersion));
    }

    // --- Read header string (Python dict literal) ---
    std::string header(headerLen, '\0');
    ifs.read(&header[0], headerLen);

    // --- Parse 'fortran_order' ---
    bool fortranOrder = false;
    {
        auto pos = header.find("'fortran_order'");
        if (pos == std::string::npos)
            pos = header.find("\"fortran_order\"");
        if (pos != std::string::npos)
        {
            auto tpos = header.find("True", pos);
            auto fpos = header.find("False", pos);
            if (tpos != std::string::npos &&
                (fpos == std::string::npos || tpos < fpos))
            {
                fortranOrder = true;
            }
        }
    }

    // --- Parse 'descr' (dtype string) ---
    std::string dtype;
    {
        auto pos = header.find("'descr'");
        if (pos == std::string::npos)
            pos = header.find("\"descr\"");
        if (pos == std::string::npos)
        {
            throw std::runtime_error("VoxelArray::fromNpy: cannot find 'descr' in header");
        }
        // Find the dtype string value, e.g. '<i1', '|u1', '<i4', '<f8'
        auto q1 = header.find('\'', pos + 7);
        if (q1 == std::string::npos)
            q1 = header.find('"', pos + 7);
        if (q1 == std::string::npos)
        {
            throw std::runtime_error("VoxelArray::fromNpy: malformed descr in header");
        }
        char quoteChar = header[q1];
        auto q2 = header.find(quoteChar, q1 + 1);
        if (q2 == std::string::npos)
        {
            throw std::runtime_error("VoxelArray::fromNpy: malformed descr in header");
        }
        dtype = header.substr(q1 + 1, q2 - q1 - 1);
    }

    // --- Parse 'shape' tuple ---
    std::vector<int> shape;
    {
        auto pos = header.find("'shape'");
        if (pos == std::string::npos)
            pos = header.find("\"shape\"");
        if (pos == std::string::npos)
        {
            throw std::runtime_error("VoxelArray::fromNpy: cannot find 'shape' in header");
        }
        auto paren1 = header.find('(', pos);
        auto paren2 = header.find(')', paren1);
        if (paren1 == std::string::npos || paren2 == std::string::npos)
        {
            throw std::runtime_error("VoxelArray::fromNpy: malformed shape in header");
        }
        std::string shapeStr = header.substr(paren1 + 1, paren2 - paren1 - 1);
        std::istringstream ss(shapeStr);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t\n\r"));
            token.erase(token.find_last_not_of(" \t\n\r") + 1);
            if (!token.empty())
            {
                shape.push_back(std::stoi(token));
            }
        }
    }

    if (shape.size() != 3)
    {
        throw std::runtime_error(
            "VoxelArray::fromNpy: expected 3D shape, got " +
            std::to_string(shape.size()) + "D");
    }

    int nx = shape[0];
    int ny = shape[1];
    int nz = shape[2];
    int total = nx * ny * nz;

    // --- Determine element size and read raw data ---
    // Supported dtypes: |i1, |u1, <i1, <u1, <i4, <i8, <f8 etc.
    // We strip the byte-order character and parse type+size
    std::string dtypeCore = dtype;
    if (!dtypeCore.empty() && (dtypeCore[0] == '<' || dtypeCore[0] == '>' ||
                                dtypeCore[0] == '|' || dtypeCore[0] == '='))
    {
        dtypeCore = dtypeCore.substr(1);
    }

    bool bigEndian = (!dtype.empty() && dtype[0] == '>');

    std::vector<int8_t> result(total);

    if (dtypeCore == "i1" || dtypeCore == "b1")
    {
        // int8 - read directly
        ifs.read(reinterpret_cast<char*>(result.data()), total);
    }
    else if (dtypeCore == "u1")
    {
        // uint8 - read and cast
        ifs.read(reinterpret_cast<char*>(result.data()), total);
    }
    else if (dtypeCore == "i4")
    {
        // int32 - read and convert
        std::vector<int32_t> buf(total);
        ifs.read(reinterpret_cast<char*>(buf.data()), total * 4);
        if (bigEndian)
        {
            for (auto& v : buf)
            {
                char* p = reinterpret_cast<char*>(&v);
                std::swap(p[0], p[3]);
                std::swap(p[1], p[2]);
            }
        }
        for (int i = 0; i < total; ++i)
        {
            result[i] = static_cast<int8_t>(buf[i]);
        }
    }
    else if (dtypeCore == "i8")
    {
        // int64 - read and convert
        std::vector<int64_t> buf(total);
        ifs.read(reinterpret_cast<char*>(buf.data()), total * 8);
        if (bigEndian)
        {
            for (auto& v : buf)
            {
                char* p = reinterpret_cast<char*>(&v);
                std::swap(p[0], p[7]);
                std::swap(p[1], p[6]);
                std::swap(p[2], p[5]);
                std::swap(p[3], p[4]);
            }
        }
        for (int i = 0; i < total; ++i)
        {
            result[i] = static_cast<int8_t>(buf[i]);
        }
    }
    else if (dtypeCore == "f8")
    {
        // float64 - read and convert
        std::vector<double> buf(total);
        ifs.read(reinterpret_cast<char*>(buf.data()), total * 8);
        if (bigEndian)
        {
            for (auto& v : buf)
            {
                char* p = reinterpret_cast<char*>(&v);
                std::swap(p[0], p[7]);
                std::swap(p[1], p[6]);
                std::swap(p[2], p[5]);
                std::swap(p[3], p[4]);
            }
        }
        for (int i = 0; i < total; ++i)
        {
            result[i] = static_cast<int8_t>(std::lround(buf[i]));
        }
    }
    else if (dtypeCore == "f4")
    {
        // float32 - read and convert
        std::vector<float> buf(total);
        ifs.read(reinterpret_cast<char*>(buf.data()), total * 4);
        if (bigEndian)
        {
            for (auto& v : buf)
            {
                char* p = reinterpret_cast<char*>(&v);
                std::swap(p[0], p[3]);
                std::swap(p[1], p[2]);
            }
        }
        for (int i = 0; i < total; ++i)
        {
            result[i] = static_cast<int8_t>(std::lround(buf[i]));
        }
    }
    else
    {
        throw std::runtime_error(
            "VoxelArray::fromNpy: unsupported dtype '" + dtype + "'");
    }

    // Handle Fortran-order (column-major) by transposing to C-order (row-major)
    if (fortranOrder)
    {
        std::vector<int8_t> transposed(total);
        for (int iz = 0; iz < nz; ++iz)
        {
            for (int iy = 0; iy < ny; ++iy)
            {
                for (int ix = 0; ix < nx; ++ix)
                {
                    // Fortran-order index: ix + nx * (iy + ny * iz) is the same
                    // as C-order for shape (nx, ny, nz), but Fortran stores as
                    // column-major: index = ix + nx * iy + nx * ny * iz
                    // which is actually the same linear layout for our indexing.
                    // Fortran order for shape (nx,ny,nz) means the first index
                    // varies fastest, which is: idx_f = ix + nx*(iy + ny*iz)
                    // C order for shape (nx,ny,nz): idx_c = nz*ny*ix + nz*iy + iz
                    // We need to map: result_c[idx_c] = result_f[idx_f]
                    int idxF = ix + nx * (iy + ny * iz);
                    int idxC = ix + nx * (iy + ny * iz);
                    // Actually for our VoxelArray indexing at(x,y,z) = data[x + nx*(y + ny*z)]
                    // this IS Fortran order (first index varies fastest).
                    // So if the file is Fortran order, the data is already in
                    // our desired layout. If C-order, we need to transpose.
                    // NumPy C-order for shape (nx,ny,nz): idx = ny*nz*ix + nz*iy + iz
                    // Our layout: idx = ix + nx*iy + nx*ny*iz (first index fastest)
                    // So our layout matches Fortran order.
                    (void)idxF;
                    (void)idxC;
                    transposed[ix + nx * (iy + ny * iz)] = result[ix + nx * (iy + ny * iz)];
                }
            }
        }
        // fortran_order=True means data is already in our layout, no transpose needed
        // (our at(x,y,z) has x as the fastest-varying index, matching Fortran order)
    }
    else
    {
        // C-order: the LAST index varies fastest in the file
        // File layout for shape (nx,ny,nz) in C-order: idx_file = ny*nz*ix + nz*iy + iz
        // Our layout: idx_mem = ix + nx*iy + nx*ny*iz
        // We need to rearrange.
        std::vector<int8_t> reordered(total);
        for (int ix = 0; ix < nx; ++ix)
        {
            for (int iy = 0; iy < ny; ++iy)
            {
                for (int iz = 0; iz < nz; ++iz)
                {
                    int idxC = ny * nz * ix + nz * iy + iz;
                    int idxMem = ix + nx * (iy + ny * iz);
                    reordered[idxMem] = result[idxC];
                }
            }
        }
        result = std::move(reordered);
    }

    return VoxelArray(std::move(result), nx, ny, nz);
}

// ---------------------------------------------------------------------------
// fluidFraction
// ---------------------------------------------------------------------------
double VoxelArray::fluidFraction() const
{
    if (data_.empty())
        return 0.0;

    int count = 0;
    for (auto v : data_)
    {
        if (v != 0)
            ++count;
    }
    return static_cast<double>(count) / static_cast<double>(data_.size());
}

// ---------------------------------------------------------------------------
// invertConvention - swap 0 and 1
// ---------------------------------------------------------------------------
void VoxelArray::invertConvention()
{
    for (auto& v : data_)
    {
        if (v == 0)
            v = 1;
        else if (v == 1)
            v = 0;
    }
}

// ---------------------------------------------------------------------------
// resample - nearest-neighbor resampling (like scipy.ndimage.zoom)
// ---------------------------------------------------------------------------
VoxelArray VoxelArray::resample(int targetRes) const
{
    if (nx_ == 0 || ny_ == 0 || nz_ == 0)
    {
        throw std::runtime_error("VoxelArray::resample: cannot resample empty array");
    }

    // Compute new dimensions preserving aspect ratio
    // Scale factor based on the first axis (nx) -> targetRes
    double scaleX = static_cast<double>(targetRes) / static_cast<double>(nx_);
    double scaleY = static_cast<double>(targetRes) / static_cast<double>(ny_);
    // For nz, scale proportionally to nx
    double scaleZ = static_cast<double>(targetRes) / static_cast<double>(nx_);

    int newNx = targetRes;
    int newNy = std::max(1, static_cast<int>(std::round(ny_ * scaleY)));
    int newNz = std::max(1, static_cast<int>(std::round(nz_ * scaleZ)));

    std::vector<int8_t> newData(static_cast<size_t>(newNx) * newNy * newNz);

    for (int iz = 0; iz < newNz; ++iz)
    {
        // Map new coord to old coord (nearest neighbor)
        int ozRaw = static_cast<int>(std::floor((iz + 0.5) / scaleZ));
        int oz = std::min(std::max(ozRaw, 0), nz_ - 1);

        for (int iy = 0; iy < newNy; ++iy)
        {
            int oyRaw = static_cast<int>(std::floor((iy + 0.5) / scaleY));
            int oy = std::min(std::max(oyRaw, 0), ny_ - 1);

            for (int ix = 0; ix < newNx; ++ix)
            {
                int oxRaw = static_cast<int>(std::floor((ix + 0.5) / scaleX));
                int ox = std::min(std::max(oxRaw, 0), nx_ - 1);

                newData[ix + newNx * (iy + newNy * iz)] = at(ox, oy, oz);
            }
        }
    }

    return VoxelArray(std::move(newData), newNx, newNy, newNz);
}

} // namespace fiberfoam
