#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fiberfoam
{

enum class FlowDirection
{
    X = 0,
    Y = 1,
    Z = 2
};

inline FlowDirection secondaryDirection(FlowDirection d)
{
    switch (d)
    {
    case FlowDirection::X:
        return FlowDirection::Y;
    case FlowDirection::Y:
        return FlowDirection::Z;
    case FlowDirection::Z:
        return FlowDirection::X;
    }
    return FlowDirection::X;
}

inline FlowDirection tertiaryDirection(FlowDirection d)
{
    return secondaryDirection(secondaryDirection(d));
}

inline int axisIndex(FlowDirection d) { return static_cast<int>(d); }

inline std::string directionName(FlowDirection d)
{
    switch (d)
    {
    case FlowDirection::X:
        return "x";
    case FlowDirection::Y:
        return "y";
    case FlowDirection::Z:
        return "z";
    }
    return "x";
}

inline FlowDirection directionFromName(const std::string& name)
{
    if (name == "x" || name == "X")
        return FlowDirection::X;
    if (name == "y" || name == "Y")
        return FlowDirection::Y;
    if (name == "z" || name == "Z")
        return FlowDirection::Z;
    throw std::runtime_error("Invalid direction name: " + name);
}

enum class CellRegion : int8_t
{
    Fibrous = 0,
    BufferInlet = 1,
    BufferOutlet = 2
};

struct FluidProperties
{
    double kinematicViscosity = 7.934782609e-05; // m^2/s
    double density = 920.0;                       // kg/m^3
    double dynamicViscosity = 0.073;              // Pa.s
    double pressureInlet = 1.0;                   // Pa (kinematic: p/rho)
    double pressureOutlet = 0.0;
};

struct Point3D
{
    double x = 0, y = 0, z = 0;

    bool operator<(const Point3D& o) const
    {
        if (z != o.z)
            return z < o.z;
        if (y != o.y)
            return y < o.y;
        return x < o.x;
    }

    bool operator==(const Point3D& o) const { return x == o.x && y == o.y && z == o.z; }

    bool operator!=(const Point3D& o) const { return !(*this == o); }
};

using VoxelCoord = std::array<int, 3>;
using FaceVertices = std::array<int, 4>;

struct CellData
{
    VoxelCoord coord = {0, 0, 0};
    double u = 0, v = 0, w = 0, p = 0;
    CellRegion region = CellRegion::Fibrous;
};

struct MeshData
{
    std::vector<Point3D> points;
    std::vector<FaceVertices> faces;
    std::vector<int> owner;
    std::vector<int> neighbour;
    int nInternalFaces = 0;
    std::map<std::string, std::pair<int, int>> boundaryPatches; // name -> (startFace, nFaces)
    std::map<int, CellData> cellMap;
    int nCells = 0;
};

struct PermeabilityResult
{
    FlowDirection direction = FlowDirection::X;
    double permVolAvgMain = 0;
    double permVolAvgSecondary = 0;
    double permVolAvgTertiary = 0;
    double permFlowRate = 0;
    double fiberVolumeContent = 0;
    double flowLength = 0;
    double crossSectionArea = 0;
    int iterationsToConverge = 0;
};

} // namespace fiberfoam
