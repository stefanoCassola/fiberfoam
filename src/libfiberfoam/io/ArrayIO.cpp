#include "io/ArrayIO.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fiberfoam
{

namespace ArrayIO
{

// ---------------------------------------------------------------------------
// readDatFile -- read flat text file with whitespace-separated values
// ---------------------------------------------------------------------------

std::vector<double> readDatFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open .dat file: " + path);

    std::vector<double> data;
    double val;
    while (file >> val)
    {
        data.push_back(val);
    }

    if (data.empty())
        throw std::runtime_error("Empty or unreadable .dat file: " + path);

    return data;
}

// ---------------------------------------------------------------------------
// writeDatFile
// ---------------------------------------------------------------------------

void writeDatFile(const std::string& path, const std::vector<double>& data)
{
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file for writing: " + path);

    file.precision(15);
    for (size_t i = 0; i < data.size(); ++i)
    {
        file << data[i];
        if (i + 1 < data.size())
            file << "\n";
    }
    file << "\n";
}

// ---------------------------------------------------------------------------
// readNpyHeader -- parse the .npy format header
//
// NumPy .npy format:
//   - 6 byte magic: \x93NUMPY
//   - 1 byte major version
//   - 1 byte minor version
//   - 2 byte (v1) or 4 byte (v2) little-endian header length
//   - ASCII header (Python dict literal), padded with spaces to align to 64
//
// The dict contains: 'descr', 'fortran_order', 'shape'
// ---------------------------------------------------------------------------

NpyHeader readNpyHeader(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot open .npy file: " + path);

    // Read and verify magic number
    char magic[6];
    file.read(magic, 6);
    if (!file || magic[0] != '\x93' || std::strncmp(magic + 1, "NUMPY", 5) != 0)
        throw std::runtime_error("Not a valid .npy file: " + path);

    // Read version
    uint8_t majorVersion = 0;
    uint8_t minorVersion = 0;
    file.read(reinterpret_cast<char*>(&majorVersion), 1);
    file.read(reinterpret_cast<char*>(&minorVersion), 1);

    // Read header length (little-endian)
    uint32_t headerLen = 0;
    if (majorVersion == 1)
    {
        uint16_t len16 = 0;
        file.read(reinterpret_cast<char*>(&len16), 2);
        headerLen = len16;
    }
    else if (majorVersion == 2)
    {
        file.read(reinterpret_cast<char*>(&headerLen), 4);
    }
    else
    {
        throw std::runtime_error("Unsupported .npy version: " +
                                 std::to_string(majorVersion) + "." +
                                 std::to_string(minorVersion));
    }

    // Read the header string
    std::string header(headerLen, '\0');
    file.read(&header[0], headerLen);
    if (!file)
        throw std::runtime_error("Failed to read .npy header from: " + path);

    NpyHeader result;
    result.fortranOrder = false;

    // Parse 'descr' : '<f8' etc.
    {
        size_t pos = header.find("'descr'");
        if (pos == std::string::npos)
            pos = header.find("\"descr\"");
        if (pos != std::string::npos)
        {
            // Find the value after the colon
            size_t colon = header.find(':', pos);
            if (colon != std::string::npos)
            {
                // Find the quoted string
                size_t q1 = header.find_first_of("'\"", colon + 1);
                if (q1 != std::string::npos)
                {
                    char quote = header[q1];
                    size_t q2 = header.find(quote, q1 + 1);
                    if (q2 != std::string::npos)
                    {
                        result.dtype = header.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
        }
    }

    // Parse 'fortran_order'
    {
        size_t pos = header.find("'fortran_order'");
        if (pos == std::string::npos)
            pos = header.find("\"fortran_order\"");
        if (pos != std::string::npos)
        {
            size_t colon = header.find(':', pos);
            if (colon != std::string::npos)
            {
                std::string rest = header.substr(colon + 1);
                if (rest.find("True") != std::string::npos)
                    result.fortranOrder = true;
            }
        }
    }

    // Parse 'shape' : (n1, n2, ...) or (n1,)
    {
        size_t pos = header.find("'shape'");
        if (pos == std::string::npos)
            pos = header.find("\"shape\"");
        if (pos != std::string::npos)
        {
            size_t paren1 = header.find('(', pos);
            size_t paren2 = header.find(')', paren1);
            if (paren1 != std::string::npos && paren2 != std::string::npos)
            {
                std::string shapeStr = header.substr(paren1 + 1, paren2 - paren1 - 1);
                std::istringstream iss(shapeStr);
                std::string token;
                while (std::getline(iss, token, ','))
                {
                    // Trim whitespace
                    size_t start = token.find_first_not_of(" \t");
                    if (start == std::string::npos)
                        continue;
                    size_t end = token.find_last_not_of(" \t");
                    std::string trimmed = token.substr(start, end - start + 1);
                    if (!trimmed.empty())
                    {
                        result.shape.push_back(std::stoi(trimmed));
                    }
                }
            }
        }
    }

    return result;
}

} // namespace ArrayIO

} // namespace fiberfoam
