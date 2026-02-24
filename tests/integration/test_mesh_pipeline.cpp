#include <gtest/gtest.h>
#include "geometry/VoxelArray.h"
#include "mesh/HexMeshBuilder.h"

using namespace fiberfoam;

class MeshPipelineTest : public ::testing::Test
{
protected:
    VoxelArray loadGeometry5x5x5()
    {
        // Load the 5x5x5 fixture: center column (x=2) is fluid after inversion
        VoxelArray arr = VoxelArray::fromDatFile("fixtures/geometry_5x5x5.dat", 5);
        arr.invertConvention();
        return arr;
    }

    HexMeshBuilder::Options defaultOpts()
    {
        HexMeshBuilder::Options opts;
        opts.voxelSize = 1e-6;
        opts.flowDirection = FlowDirection::X;
        opts.connectivityCheck = true;
        opts.autoBoundaryFaceSets = true;
        opts.periodic = false;
        return opts;
    }
};

TEST_F(MeshPipelineTest, LoadAndBuildMesh)
{
    VoxelArray geom = loadGeometry5x5x5();

    EXPECT_EQ(geom.nx(), 5);
    EXPECT_EQ(geom.ny(), 5);
    EXPECT_EQ(geom.nz(), 5);

    // After inversion: x=2 column is fluid (value 1), rest is solid (value 0)
    EXPECT_EQ(geom.at(2, 0, 0), 1);
    EXPECT_EQ(geom.at(0, 0, 0), 0);

    HexMeshBuilder builder(geom, defaultOpts());
    MeshData mesh = builder.build();

    // Should have cells for the fluid channel
    EXPECT_GT(mesh.nCells, 0);

    // 25 fluid cells (x=2 for all 5*5=25 y,z combinations)
    EXPECT_EQ(mesh.nCells, 25);
}

TEST_F(MeshPipelineTest, FacesConsistent)
{
    VoxelArray geom = loadGeometry5x5x5();
    HexMeshBuilder builder(geom, defaultOpts());
    MeshData mesh = builder.build();

    // Total faces = internal + boundary
    int totalFaces = static_cast<int>(mesh.faces.size());
    EXPECT_GT(totalFaces, 0);
    EXPECT_GE(mesh.nInternalFaces, 0);
    EXPECT_GT(totalFaces, mesh.nInternalFaces);

    // Owner size matches total faces
    EXPECT_EQ(static_cast<int>(mesh.owner.size()), totalFaces);

    // Neighbour size matches internal faces
    EXPECT_EQ(static_cast<int>(mesh.neighbour.size()), mesh.nInternalFaces);
}

TEST_F(MeshPipelineTest, OwnerNeighbourValid)
{
    VoxelArray geom = loadGeometry5x5x5();
    HexMeshBuilder builder(geom, defaultOpts());
    MeshData mesh = builder.build();

    for (int o : mesh.owner)
    {
        EXPECT_GE(o, 0);
        EXPECT_LT(o, mesh.nCells);
    }

    for (int n : mesh.neighbour)
    {
        EXPECT_GE(n, 0);
        EXPECT_LT(n, mesh.nCells);
    }

    // Owner < neighbour for internal faces
    for (int i = 0; i < mesh.nInternalFaces; ++i)
    {
        EXPECT_LT(mesh.owner[i], mesh.neighbour[i]);
    }
}

TEST_F(MeshPipelineTest, BoundaryPatchesDefined)
{
    VoxelArray geom = loadGeometry5x5x5();
    HexMeshBuilder builder(geom, defaultOpts());
    MeshData mesh = builder.build();

    EXPECT_FALSE(mesh.boundaryPatches.empty());

    // Boundary patches should cover all faces after internal faces
    int boundaryFaceCount = 0;
    for (const auto& [name, range] : mesh.boundaryPatches)
    {
        auto [startFace, nFaces] = range;
        EXPECT_GE(startFace, mesh.nInternalFaces);
        EXPECT_GT(nFaces, 0);
        boundaryFaceCount += nFaces;
    }

    int totalBoundaryFaces = static_cast<int>(mesh.faces.size()) - mesh.nInternalFaces;
    EXPECT_EQ(boundaryFaceCount, totalBoundaryFaces);
}

TEST_F(MeshPipelineTest, PointsInPhysicalRange)
{
    VoxelArray geom = loadGeometry5x5x5();
    double voxelSize = 1e-6;
    auto opts = defaultOpts();
    opts.voxelSize = voxelSize;

    HexMeshBuilder builder(geom, opts);
    MeshData mesh = builder.build();

    double maxExtent = 5.0 * voxelSize;
    for (const auto& pt : mesh.points)
    {
        EXPECT_GE(pt.x, -1e-15);
        EXPECT_LE(pt.x, maxExtent + 1e-15);
        EXPECT_GE(pt.y, -1e-15);
        EXPECT_LE(pt.y, maxExtent + 1e-15);
        EXPECT_GE(pt.z, -1e-15);
        EXPECT_LE(pt.z, maxExtent + 1e-15);
    }
}

TEST_F(MeshPipelineTest, CellMapCoordinatesValid)
{
    VoxelArray geom = loadGeometry5x5x5();
    HexMeshBuilder builder(geom, defaultOpts());
    MeshData mesh = builder.build();

    for (const auto& [idx, cd] : mesh.cellMap)
    {
        EXPECT_GE(cd.coord[0], 0);
        EXPECT_LT(cd.coord[0], geom.nx());
        EXPECT_GE(cd.coord[1], 0);
        EXPECT_LT(cd.coord[1], geom.ny());
        EXPECT_GE(cd.coord[2], 0);
        EXPECT_LT(cd.coord[2], geom.nz());

        // Should only contain fluid voxels
        EXPECT_EQ(geom.at(cd.coord[0], cd.coord[1], cd.coord[2]), 1);
    }
}

TEST_F(MeshPipelineTest, AllDirections)
{
    VoxelArray geom = loadGeometry5x5x5();

    for (FlowDirection dir : {FlowDirection::X, FlowDirection::Y, FlowDirection::Z})
    {
        auto opts = defaultOpts();
        opts.flowDirection = dir;

        HexMeshBuilder builder(geom, opts);
        MeshData mesh = builder.build();

        // Mesh topology should be the same regardless of flow direction
        // (flow direction only affects boundary patch naming)
        EXPECT_EQ(mesh.nCells, 25);
        EXPECT_GT(static_cast<int>(mesh.faces.size()), 0);
    }
}
