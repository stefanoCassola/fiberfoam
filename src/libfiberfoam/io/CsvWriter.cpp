#include "io/CsvWriter.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace fiberfoam
{

void CsvWriter::writePermeability(const PermeabilityResult& result, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file for writing: " + path);

    // Format matching writeFile.H output:
    // parameter;value;unit
    char buf[128];

    std::snprintf(buf, sizeof(buf), "%.12g", result.flowLength);
    file << "flowLength;" << buf << ";m\n";

    std::snprintf(buf, sizeof(buf), "%.12g", result.crossSectionArea);
    // Use the UTF-8 superscript 2 for m^2 to match the original format
    file << "flowCrossArea;" << buf << ";m2\n";

    std::snprintf(buf, sizeof(buf), "%.12g", result.fiberVolumeContent);
    file << "fiberVolumeContent;" << buf << ";-\n";

    std::string dirName = directionName(result.direction);
    FlowDirection secDir = secondaryDirection(result.direction);
    FlowDirection terDir = tertiaryDirection(result.direction);
    std::string secName = directionName(secDir);
    std::string terName = directionName(terDir);

    std::snprintf(buf, sizeof(buf), "%.12g", result.permVolAvgMain);
    file << "permVolAvg_" << dirName << dirName << ";" << buf << ";m2\n";

    std::snprintf(buf, sizeof(buf), "%.12g", result.permVolAvgSecondary);
    file << "permVolAvg_" << dirName << secName << ";" << buf << ";m2\n";

    std::snprintf(buf, sizeof(buf), "%.12g", result.permVolAvgTertiary);
    file << "permVolAvg_" << dirName << terName << ";" << buf << ";m2\n";

    std::snprintf(buf, sizeof(buf), "%.12g", result.permFlowRate);
    file << "permFlowRate_" << dirName << dirName << ";" << buf << ";m2\n";

    file << "iterationsToConverge;" << result.iterationsToConverge << ";-\n";
}

} // namespace fiberfoam
