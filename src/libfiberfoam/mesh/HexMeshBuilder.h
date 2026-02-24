#pragma once

#include "common/Types.h"
#include "geometry/VoxelArray.h"
#include <array>
#include <map>
#include <vector>

namespace fiberfoam
{

class HexMeshBuilder
{
public:
    struct Options
    {
        double voxelSize;
        FlowDirection flowDirection;
        bool connectivityCheck = true;
        bool autoBoundaryFaceSets = true;
        bool periodic = false;
        const double* velocityField = nullptr;
        const double* pressureField = nullptr;
        const int8_t* regionMask = nullptr;
    };

    explicit HexMeshBuilder(const VoxelArray& geometry, Options opts);

    void generateCellMap();
    void filterByConnectivity();
    void generatePoints();
    void generateFaces();
    void classifyBoundaryPatches();

    // Full pipeline
    MeshData build();

    const MeshData& meshData() const { return mesh_; }

private:
    void buildCellEntry(int x, int y, int z, int cellIndex);
    void generateCellVertices(const Point3D& center, std::array<Point3D, 8>& verts) const;
    void sortVertices(std::array<Point3D, 8>& verts) const;

    VoxelArray geometry_;
    Options opts_;
    MeshData mesh_;

    // Internal: cell -> 8 vertex indices in global points list
    std::vector<std::array<int, 8>> cellVertexIndices_;
    // For face generation: sorted face tuple -> original face + cell list
    struct FaceInfo
    {
        FaceVertices vertices;
        std::vector<int> cells;
    };
};

} // namespace fiberfoam
