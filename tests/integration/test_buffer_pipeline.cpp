#include <gtest/gtest.h>
#include "geometry/VoxelArray.h"
#include "geometry/FiberFreeRegion.h"
#include "geometry/RegionTracker.h"
#include "mesh/HexMeshBuilder.h"

using namespace fiberfoam;

class BufferPipelineTest : public ::testing::Test
{
protected:
    VoxelArray loadGeometry3x3x3()
    {
        VoxelArray arr = VoxelArray::fromDatFile("fixtures/geometry_3x3x3.dat", 3);
        arr.invertConvention();
        return arr;
    }

    VoxelArray makeAllFluid(int n)
    {
        std::vector<int8_t> data(n * n * n, 1);
        return VoxelArray(data, n, n, n);
    }

    HexMeshBuilder::Options meshOpts(FlowDirection dir, const int8_t* regionMask = nullptr)
    {
        HexMeshBuilder::Options opts;
        opts.voxelSize = 1e-6;
        opts.flowDirection = dir;
        opts.connectivityCheck = true;
        opts.autoBoundaryFaceSets = true;
        opts.periodic = false;
        opts.regionMask = regionMask;
        return opts;
    }
};

TEST_F(BufferPipelineTest, PadAndBuildMeshXDirection)
{
    VoxelArray geom = makeAllFluid(3);
    int inletLayers = 2;
    int outletLayers = 2;

    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, inletLayers, outletLayers);

    // Verify padded dimensions: 3+2+2 = 7 along X, 3x3 unchanged
    EXPECT_EQ(padded.geometry.nx(), 7);
    EXPECT_EQ(padded.geometry.ny(), 3);
    EXPECT_EQ(padded.geometry.nz(), 3);

    // Build mesh from padded geometry
    auto opts = meshOpts(FlowDirection::X, padded.regionMask.data());
    HexMeshBuilder builder(padded.geometry, opts);
    MeshData mesh = builder.build();

    // All cells should be fluid (7*3*3 = 63)
    EXPECT_EQ(mesh.nCells, 63);
}

TEST_F(BufferPipelineTest, RegionTrackingCorrect)
{
    VoxelArray geom = makeAllFluid(3);
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 2, 2);

    auto opts = meshOpts(FlowDirection::X, padded.regionMask.data());
    HexMeshBuilder builder(padded.geometry, opts);
    MeshData mesh = builder.build();

    // Build region tracker from the mesh
    RegionTracker tracker;
    tracker.buildFromCellMap(mesh.cellMap, padded.regionMask,
                             padded.geometry.nx(), padded.geometry.ny(), padded.geometry.nz());

    // Fibrous: original 3x3x3 = 27 cells
    EXPECT_EQ(tracker.countFibrousCells(), 27);

    // Buffer inlet: 2 layers * 3 * 3 = 18
    EXPECT_EQ(tracker.countBufferInletCells(), 18);

    // Buffer outlet: 2 layers * 3 * 3 = 18
    EXPECT_EQ(tracker.countBufferOutletCells(), 18);

    // Total = 27 + 18 + 18 = 63
    int total = tracker.countFibrousCells() +
                tracker.countBufferInletCells() +
                tracker.countBufferOutletCells();
    EXPECT_EQ(total, mesh.nCells);
}

TEST_F(BufferPipelineTest, PadAndBuildMeshYDirection)
{
    VoxelArray geom = makeAllFluid(3);
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::Y, 2, 2);

    EXPECT_EQ(padded.geometry.nx(), 3);
    EXPECT_EQ(padded.geometry.ny(), 7);
    EXPECT_EQ(padded.geometry.nz(), 3);

    auto opts = meshOpts(FlowDirection::Y, padded.regionMask.data());
    HexMeshBuilder builder(padded.geometry, opts);
    MeshData mesh = builder.build();

    EXPECT_EQ(mesh.nCells, 63);
}

TEST_F(BufferPipelineTest, PadAndBuildMeshZDirection)
{
    VoxelArray geom = makeAllFluid(3);
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::Z, 2, 2);

    EXPECT_EQ(padded.geometry.nx(), 3);
    EXPECT_EQ(padded.geometry.ny(), 3);
    EXPECT_EQ(padded.geometry.nz(), 7);

    auto opts = meshOpts(FlowDirection::Z, padded.regionMask.data());
    HexMeshBuilder builder(padded.geometry, opts);
    MeshData mesh = builder.build();

    EXPECT_EQ(mesh.nCells, 63);
}

TEST_F(BufferPipelineTest, BufferZonesAreFluid)
{
    VoxelArray geom = loadGeometry3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 2, 2);

    // All buffer zone cells should be fluid (1)
    int nx = padded.geometry.nx();
    int ny = padded.geometry.ny();
    int nz = padded.geometry.nz();

    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            // Inlet layers
            for (int x = 0; x < 2; ++x)
            {
                EXPECT_EQ(padded.geometry.at(x, y, z), 1)
                    << "Inlet buffer not fluid at (" << x << "," << y << "," << z << ")";
            }
            // Outlet layers
            for (int x = nx - 2; x < nx; ++x)
            {
                EXPECT_EQ(padded.geometry.at(x, y, z), 1)
                    << "Outlet buffer not fluid at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

TEST_F(BufferPipelineTest, FibrousRegionPreserved)
{
    VoxelArray geom = loadGeometry3x3x3();
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 2, 2);

    // The original geometry data should be preserved in the fibrous region
    // (layers 2..4 in the padded geometry correspond to the original 0..2)
    for (int z = 0; z < 3; ++z)
    {
        for (int y = 0; y < 3; ++y)
        {
            for (int x = 0; x < 3; ++x)
            {
                EXPECT_EQ(padded.geometry.at(x + 2, y, z), geom.at(x, y, z))
                    << "Fibrous region mismatch at original (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

TEST_F(BufferPipelineTest, ExpandedDimensionsMeshValid)
{
    VoxelArray geom = makeAllFluid(3);
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 3, 3);

    // 3+3+3 = 9 along X
    EXPECT_EQ(padded.geometry.nx(), 9);

    auto opts = meshOpts(FlowDirection::X, padded.regionMask.data());
    HexMeshBuilder builder(padded.geometry, opts);
    MeshData mesh = builder.build();

    // 9*3*3 = 81 cells
    EXPECT_EQ(mesh.nCells, 81);

    // Mesh should be valid
    EXPECT_GT(mesh.nInternalFaces, 0);
    EXPECT_GT(static_cast<int>(mesh.faces.size()), mesh.nInternalFaces);

    // Owner/neighbour should be valid
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
}

TEST_F(BufferPipelineTest, AsymmetricBufferTracking)
{
    VoxelArray geom = makeAllFluid(3);
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 1, 3);

    // 3+1+3 = 7 along X
    EXPECT_EQ(padded.geometry.nx(), 7);

    auto opts = meshOpts(FlowDirection::X, padded.regionMask.data());
    HexMeshBuilder builder(padded.geometry, opts);
    MeshData mesh = builder.build();

    RegionTracker tracker;
    tracker.buildFromCellMap(mesh.cellMap, padded.regionMask,
                             padded.geometry.nx(), padded.geometry.ny(), padded.geometry.nz());

    EXPECT_EQ(tracker.countFibrousCells(), 27);       // original 3x3x3
    EXPECT_EQ(tracker.countBufferInletCells(), 9);     // 1 layer * 3*3
    EXPECT_EQ(tracker.countBufferOutletCells(), 27);   // 3 layers * 3*3

    int total = tracker.countFibrousCells() +
                tracker.countBufferInletCells() +
                tracker.countBufferOutletCells();
    EXPECT_EQ(total, mesh.nCells);
}

TEST_F(BufferPipelineTest, NoBufferMatchesOriginal)
{
    VoxelArray geom = makeAllFluid(3);

    // With no buffer layers
    PaddedGeometry padded = FiberFreeRegion::pad(geom, FlowDirection::X, 0, 0);

    EXPECT_EQ(padded.geometry.nx(), 3);
    EXPECT_EQ(padded.geometry.ny(), 3);
    EXPECT_EQ(padded.geometry.nz(), 3);

    auto opts = meshOpts(FlowDirection::X, padded.regionMask.data());
    HexMeshBuilder builder(padded.geometry, opts);
    MeshData mesh = builder.build();

    // Should produce same mesh as without padding
    EXPECT_EQ(mesh.nCells, 27);

    RegionTracker tracker;
    tracker.buildFromCellMap(mesh.cellMap, padded.regionMask,
                             padded.geometry.nx(), padded.geometry.ny(), padded.geometry.nz());

    EXPECT_EQ(tracker.countFibrousCells(), 27);
    EXPECT_EQ(tracker.countBufferInletCells(), 0);
    EXPECT_EQ(tracker.countBufferOutletCells(), 0);
}
