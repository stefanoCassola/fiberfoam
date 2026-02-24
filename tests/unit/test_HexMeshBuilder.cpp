#include <gtest/gtest.h>
#include "mesh/HexMeshBuilder.h"
#include "geometry/VoxelArray.h"

using namespace fiberfoam;

class HexMeshBuilderTest : public ::testing::Test
{
protected:
    // Create a 2x2x2 all-fluid geometry
    VoxelArray makeAllFluid2x2x2()
    {
        std::vector<int8_t> data(8, 1);
        return VoxelArray(data, 2, 2, 2);
    }

    // Create a 3x3x3 geometry with center column fluid (x=1 for all y,z)
    VoxelArray makeCenterColumn3x3x3()
    {
        std::vector<int8_t> data(27, 0);
        for (int z = 0; z < 3; ++z)
            for (int y = 0; y < 3; ++y)
                data[1 + 3 * (y + 3 * z)] = 1;
        return VoxelArray(data, 3, 3, 3);
    }

    HexMeshBuilder::Options defaultOpts(double voxelSize = 1.0)
    {
        HexMeshBuilder::Options opts;
        opts.voxelSize = voxelSize;
        opts.flowDirection = FlowDirection::X;
        opts.connectivityCheck = false; // skip for basic tests
        opts.autoBoundaryFaceSets = true;
        opts.periodic = false;
        return opts;
    }
};

TEST_F(HexMeshBuilderTest, AllFluid2x2x2CellCount)
{
    VoxelArray geom = makeAllFluid2x2x2();
    HexMeshBuilder builder(geom, defaultOpts());

    MeshData mesh = builder.build();

    // 2x2x2 all-fluid = 8 cells
    EXPECT_EQ(mesh.nCells, 8);
}

TEST_F(HexMeshBuilderTest, AllFluid2x2x2PointCount)
{
    VoxelArray geom = makeAllFluid2x2x2();
    HexMeshBuilder builder(geom, defaultOpts());

    MeshData mesh = builder.build();

    // 2x2x2 hex mesh -> (2+1)^3 = 27 unique points
    EXPECT_EQ(static_cast<int>(mesh.points.size()), 27);
}

TEST_F(HexMeshBuilderTest, AllFluid2x2x2FaceCounts)
{
    VoxelArray geom = makeAllFluid2x2x2();
    HexMeshBuilder builder(geom, defaultOpts());

    MeshData mesh = builder.build();

    // Total faces = internal + boundary
    int totalFaces = static_cast<int>(mesh.faces.size());
    EXPECT_GT(totalFaces, 0);
    EXPECT_GT(mesh.nInternalFaces, 0);

    // Internal faces: shared faces between adjacent cells
    // In a 2x2x2 grid: 12 internal faces (4 in each direction: 2*1*2 + 2*2*1 + 1*2*2 = 4+4+4)
    EXPECT_EQ(mesh.nInternalFaces, 12);

    // Boundary faces: 6 outer faces of the cube, each is 2x2 = 4 faces => 24 boundary faces
    int nBoundaryFaces = totalFaces - mesh.nInternalFaces;
    EXPECT_EQ(nBoundaryFaces, 24);
}

TEST_F(HexMeshBuilderTest, OwnerNeighbourConsistency)
{
    VoxelArray geom = makeAllFluid2x2x2();
    HexMeshBuilder builder(geom, defaultOpts());

    MeshData mesh = builder.build();

    // Owner array should have same size as faces
    EXPECT_EQ(static_cast<int>(mesh.owner.size()), static_cast<int>(mesh.faces.size()));

    // Neighbour array should have same size as internal faces
    EXPECT_EQ(static_cast<int>(mesh.neighbour.size()), mesh.nInternalFaces);

    // All owner indices should be valid cell indices
    for (int o : mesh.owner)
    {
        EXPECT_GE(o, 0);
        EXPECT_LT(o, mesh.nCells);
    }

    // All neighbour indices should be valid cell indices
    for (int n : mesh.neighbour)
    {
        EXPECT_GE(n, 0);
        EXPECT_LT(n, mesh.nCells);
    }

    // Owner should be less than neighbour for internal faces
    for (int i = 0; i < mesh.nInternalFaces; ++i)
    {
        EXPECT_LT(mesh.owner[i], mesh.neighbour[i]);
    }
}

TEST_F(HexMeshBuilderTest, BoundaryPatchesExist)
{
    VoxelArray geom = makeAllFluid2x2x2();
    HexMeshBuilder builder(geom, defaultOpts());

    MeshData mesh = builder.build();

    // Should have boundary patches
    EXPECT_FALSE(mesh.boundaryPatches.empty());
}

TEST_F(HexMeshBuilderTest, CenterColumnMesh)
{
    VoxelArray geom = makeCenterColumn3x3x3();
    HexMeshBuilder builder(geom, defaultOpts());

    MeshData mesh = builder.build();

    // 9 fluid cells (x=1 for 3*3=9 positions)
    EXPECT_EQ(mesh.nCells, 9);
}

TEST_F(HexMeshBuilderTest, CellMapPopulated)
{
    VoxelArray geom = makeAllFluid2x2x2();
    HexMeshBuilder builder(geom, defaultOpts());

    MeshData mesh = builder.build();

    EXPECT_EQ(static_cast<int>(mesh.cellMap.size()), mesh.nCells);
}

TEST_F(HexMeshBuilderTest, VoxelSizeAffectsPoints)
{
    VoxelArray geom = makeAllFluid2x2x2();

    HexMeshBuilder builder1(geom, defaultOpts(1.0));
    MeshData mesh1 = builder1.build();

    HexMeshBuilder builder2(geom, defaultOpts(0.5));
    MeshData mesh2 = builder2.build();

    // Same topology
    EXPECT_EQ(mesh1.nCells, mesh2.nCells);
    EXPECT_EQ(mesh1.points.size(), mesh2.points.size());

    // But different point coordinates (scaled by voxelSize)
    // Find a non-origin point and verify scaling
    bool foundDifference = false;
    for (size_t i = 0; i < mesh1.points.size(); ++i)
    {
        if (mesh1.points[i].x != 0 || mesh1.points[i].y != 0 || mesh1.points[i].z != 0)
        {
            EXPECT_NEAR(mesh2.points[i].x, mesh1.points[i].x * 0.5, 1e-12);
            EXPECT_NEAR(mesh2.points[i].y, mesh1.points[i].y * 0.5, 1e-12);
            EXPECT_NEAR(mesh2.points[i].z, mesh1.points[i].z * 0.5, 1e-12);
            foundDifference = true;
            break;
        }
    }
    EXPECT_TRUE(foundDifference);
}

TEST_F(HexMeshBuilderTest, SingleCellMesh)
{
    std::vector<int8_t> data = {1};
    VoxelArray geom(data, 1, 1, 1);
    HexMeshBuilder builder(geom, defaultOpts());

    MeshData mesh = builder.build();

    EXPECT_EQ(mesh.nCells, 1);
    EXPECT_EQ(static_cast<int>(mesh.points.size()), 8); // single hex = 8 vertices
    EXPECT_EQ(mesh.nInternalFaces, 0);                   // no internal faces
    EXPECT_EQ(static_cast<int>(mesh.faces.size()), 6);   // 6 boundary faces
}

TEST_F(HexMeshBuilderTest, FaceVerticesValid)
{
    VoxelArray geom = makeAllFluid2x2x2();
    HexMeshBuilder builder(geom, defaultOpts());

    MeshData mesh = builder.build();

    int nPoints = static_cast<int>(mesh.points.size());
    for (const auto& face : mesh.faces)
    {
        for (int vi : face)
        {
            EXPECT_GE(vi, 0);
            EXPECT_LT(vi, nPoints);
        }
    }
}
