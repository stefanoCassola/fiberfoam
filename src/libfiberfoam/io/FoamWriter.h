#pragma once

#include "common/Types.h"
#include "config/SimulationConfig.h"
#include <string>

namespace fiberfoam
{

class FoamWriter
{
public:
    explicit FoamWriter(const SimulationConfig& config);

    // Write complete OpenFOAM case directory
    // Returns the case directory path
    std::string writeCase(const MeshData& mesh, const std::string& basePath);

private:
    void writePolyMesh(const MeshData& mesh, const std::string& caseDir);
    void writePoints(const MeshData& mesh, const std::string& polyMeshDir);
    void writeFaces(const MeshData& mesh, const std::string& polyMeshDir);
    void writeBoundary(const MeshData& mesh, const std::string& polyMeshDir);
    void writeOwner(const MeshData& mesh, const std::string& polyMeshDir);
    void writeNeighbour(const MeshData& mesh, const std::string& polyMeshDir);
    void writeFaceSets(const MeshData& mesh, const std::string& polyMeshDir);

    void writeVelocityField(const MeshData& mesh, const std::string& caseDir);
    void writePressureField(const MeshData& mesh, const std::string& caseDir);
    void writeControlDict(const std::string& caseDir);
    void writeFvSchemes(const std::string& caseDir);
    void writeFvSolution(const std::string& caseDir);
    void writeTransportProperties(const std::string& caseDir);
    void writeTurbulenceProperties(const std::string& caseDir);
    void writeCreatePatchDict(const MeshData& mesh, const std::string& caseDir);
    void writeBlockMeshDict(const MeshData& mesh, const std::string& caseDir);

    std::string foamHeader(const std::string& className,
                           const std::string& object,
                           const std::string& location = "") const;

    std::string inletPatchName() const;
    std::string outletPatchName() const;
    bool isInletPatch(const std::string& name) const;
    bool isOutletPatch(const std::string& name) const;

    SimulationConfig config_;
};

} // namespace fiberfoam
