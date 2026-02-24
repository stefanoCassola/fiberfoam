#pragma once

#include "common/Types.h"
#include <string>

namespace fiberfoam
{

class CsvWriter
{
public:
    static void writePermeability(const PermeabilityResult& result, const std::string& path);
};

} // namespace fiberfoam
