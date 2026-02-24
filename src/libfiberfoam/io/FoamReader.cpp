#include "io/FoamReader.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace fiberfoam
{

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FoamReader::FoamReader(const std::string& casePath)
    : casePath_(casePath)
{
}

// ---------------------------------------------------------------------------
// findLatestTime
// ---------------------------------------------------------------------------

std::string FoamReader::findLatestTime() const
{
    double maxTime = -1.0;
    std::string latest;

    for (const auto& entry : fs::directory_iterator(casePath_))
    {
        if (!entry.is_directory())
            continue;

        std::string name = entry.path().filename().string();

        // Try to parse as a number
        char* end = nullptr;
        double t = std::strtod(name.c_str(), &end);

        // Must consume entire string and be a valid positive number
        if (end != name.c_str() && *end == '\0' && t >= 0.0)
        {
            if (t > maxTime)
            {
                maxTime = t;
                latest = name;
            }
        }
    }

    if (latest.empty())
        throw std::runtime_error("No time directories found in: " + casePath_);

    return latest;
}

// ---------------------------------------------------------------------------
// Helper: skip to a keyword line and return remaining content
// ---------------------------------------------------------------------------

static std::string readFieldFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

// Find position after a keyword in the content
static size_t findKeyword(const std::string& content, const std::string& keyword)
{
    size_t pos = content.find(keyword);
    if (pos == std::string::npos)
        return std::string::npos;
    return pos + keyword.size();
}

// ---------------------------------------------------------------------------
// readVelocity
// ---------------------------------------------------------------------------

std::vector<std::array<double, 3>> FoamReader::readVelocity(const std::string& timeDir)
{
    std::string resolvedTime = (timeDir == "latestTime") ? findLatestTime() : timeDir;
    std::string path = casePath_ + "/" + resolvedTime + "/U";

    std::string content = readFieldFile(path);

    std::vector<std::array<double, 3>> result;

    // Find "internalField" section
    size_t pos = findKeyword(content, "internalField");
    if (pos == std::string::npos)
        throw std::runtime_error("No internalField found in: " + path);

    // Check if uniform or nonuniform
    std::string after = content.substr(pos);

    // Skip whitespace
    size_t i = 0;
    while (i < after.size() && (after[i] == ' ' || after[i] == '\t'))
        ++i;

    if (after.substr(i, 7) == "uniform")
    {
        // uniform (ux uy uz)
        size_t paren = after.find('(', i);
        if (paren == std::string::npos)
            return result;

        double ux = 0, uy = 0, uz = 0;
        std::istringstream iss(after.substr(paren + 1));
        iss >> ux >> uy >> uz;

        // For uniform fields we return a single entry
        result.push_back({ux, uy, uz});
        return result;
    }

    // nonuniform List<vector>
    // Find the count
    size_t nlPos = after.find("List<vector>", i);
    if (nlPos == std::string::npos)
        return result;

    size_t countStart = nlPos + 12; // length of "List<vector>"
    while (countStart < after.size() &&
           (after[countStart] == ' ' || after[countStart] == '\n' ||
            after[countStart] == '\r' || after[countStart] == '\t'))
        ++countStart;

    int count = 0;
    {
        std::istringstream iss(after.substr(countStart));
        iss >> count;
    }

    if (count <= 0)
        return result;

    result.reserve(count);

    // Find the opening parenthesis of the list
    size_t listStart = after.find('(', countStart);
    if (listStart == std::string::npos)
        return result;

    // Parse each vector entry: (ux uy uz)
    size_t cursor = listStart + 1;
    for (int idx = 0; idx < count; ++idx)
    {
        // Find next '('
        size_t vStart = after.find('(', cursor);
        if (vStart == std::string::npos)
            break;

        size_t vEnd = after.find(')', vStart);
        if (vEnd == std::string::npos)
            break;

        std::string vecStr = after.substr(vStart + 1, vEnd - vStart - 1);
        double ux = 0, uy = 0, uz = 0;
        std::istringstream iss(vecStr);
        iss >> ux >> uy >> uz;

        result.push_back({ux, uy, uz});
        cursor = vEnd + 1;
    }

    return result;
}

// ---------------------------------------------------------------------------
// readPressure
// ---------------------------------------------------------------------------

std::vector<double> FoamReader::readPressure(const std::string& timeDir)
{
    std::string resolvedTime = (timeDir == "latestTime") ? findLatestTime() : timeDir;
    std::string path = casePath_ + "/" + resolvedTime + "/p";

    std::string content = readFieldFile(path);

    std::vector<double> result;

    size_t pos = findKeyword(content, "internalField");
    if (pos == std::string::npos)
        throw std::runtime_error("No internalField found in: " + path);

    std::string after = content.substr(pos);

    size_t i = 0;
    while (i < after.size() && (after[i] == ' ' || after[i] == '\t'))
        ++i;

    if (after.substr(i, 7) == "uniform")
    {
        // uniform scalar
        size_t valStart = i + 7;
        while (valStart < after.size() &&
               (after[valStart] == ' ' || after[valStart] == '\t'))
            ++valStart;

        double val = 0;
        std::istringstream iss(after.substr(valStart));
        iss >> val;
        result.push_back(val);
        return result;
    }

    // nonuniform List<scalar>
    size_t nlPos = after.find("List<scalar>", i);
    if (nlPos == std::string::npos)
        return result;

    size_t countStart = nlPos + 12;
    while (countStart < after.size() &&
           (after[countStart] == ' ' || after[countStart] == '\n' ||
            after[countStart] == '\r' || after[countStart] == '\t'))
        ++countStart;

    int count = 0;
    {
        std::istringstream iss(after.substr(countStart));
        iss >> count;
    }

    if (count <= 0)
        return result;

    result.reserve(count);

    size_t listStart = after.find('(', countStart);
    if (listStart == std::string::npos)
        return result;

    std::istringstream listStream(after.substr(listStart + 1));
    for (int idx = 0; idx < count; ++idx)
    {
        double val = 0;
        if (!(listStream >> val))
            break;
        result.push_back(val);
    }

    return result;
}

// ---------------------------------------------------------------------------
// readOutletFlux
// ---------------------------------------------------------------------------

double FoamReader::readOutletFlux(const std::string& timeDir)
{
    std::string resolvedTime = (timeDir == "latestTime") ? findLatestTime() : timeDir;
    std::string path = casePath_ + "/" + resolvedTime + "/phi";

    std::string content = readFieldFile(path);

    // Look for "outlet" in boundaryField section
    size_t bfPos = findKeyword(content, "boundaryField");
    if (bfPos == std::string::npos)
        throw std::runtime_error("No boundaryField found in phi file: " + path);

    std::string bfContent = content.substr(bfPos);

    // Find "outlet" patch
    size_t outletPos = bfContent.find("outlet");
    if (outletPos == std::string::npos)
        throw std::runtime_error("No outlet patch found in phi file: " + path);

    std::string outletContent = bfContent.substr(outletPos);

    // Find the value field within this patch
    // Look for "value" keyword then parse the list
    size_t valuePos = outletContent.find("value");
    if (valuePos == std::string::npos)
        return 0.0;

    std::string valueContent = outletContent.substr(valuePos);

    // Check if uniform or nonuniform
    if (valueContent.find("uniform") < valueContent.find("nonuniform") ||
        valueContent.find("nonuniform") == std::string::npos)
    {
        // uniform scalar - return the value directly
        size_t uniPos = valueContent.find("uniform");
        if (uniPos != std::string::npos)
        {
            std::istringstream iss(valueContent.substr(uniPos + 7));
            double val = 0;
            iss >> val;
            return val;
        }
    }

    // nonuniform List<scalar> -- sum all face fluxes
    size_t listPos = valueContent.find("List<scalar>");
    if (listPos == std::string::npos)
        return 0.0;

    size_t countStart = listPos + 12;
    while (countStart < valueContent.size() &&
           (valueContent[countStart] == ' ' || valueContent[countStart] == '\n' ||
            valueContent[countStart] == '\r' || valueContent[countStart] == '\t'))
        ++countStart;

    int count = 0;
    {
        std::istringstream iss(valueContent.substr(countStart));
        iss >> count;
    }

    if (count <= 0)
        return 0.0;

    size_t parenPos = valueContent.find('(', countStart);
    if (parenPos == std::string::npos)
        return 0.0;

    double totalFlux = 0.0;
    std::istringstream listStream(valueContent.substr(parenPos + 1));
    for (int idx = 0; idx < count; ++idx)
    {
        double val = 0;
        if (!(listStream >> val))
            break;
        totalFlux += val;
    }

    return totalFlux;
}

} // namespace fiberfoam
