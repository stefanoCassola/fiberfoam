#include <gtest/gtest.h>
#include "io/FoamWriter.h"
#include "mesh/HexMeshBuilder.h"
#include "geometry/VoxelArray.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace fiberfoam;

class FoamWriterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tmpDir_ = std::filesystem::temp_directory_path() / "fiberfoam_test_foam_writer";
        std::filesystem::create_directories(tmpDir_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(tmpDir_);
    }

    MeshData buildSimpleMesh()
    {
        std::vector<int8_t> data(8, 1);
        VoxelArray geom(data, 2, 2, 2);

        HexMeshBuilder::Options opts;
        opts.voxelSize = 1e-6;
        opts.flowDirection = FlowDirection::X;
        opts.connectivityCheck = false;
        opts.autoBoundaryFaceSets = true;
        opts.periodic = false;

        HexMeshBuilder builder(geom, opts);
        return builder.build();
    }

    SimulationConfig makeConfig()
    {
        SimulationConfig cfg;
        cfg.voxelResolution = 2;
        cfg.voxelSize = 1e-6;
        cfg.flowDirections = {FlowDirection::X};
        cfg.outputPath = tmpDir_.string();
        cfg.solverName = "simpleFoamMod";
        cfg.maxIterations = 1000;
        cfg.writeInterval = 100;
        return cfg;
    }

    std::filesystem::path tmpDir_;
};

TEST_F(FoamWriterTest, WriteCaseCreatesDirectory)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());

    EXPECT_TRUE(std::filesystem::exists(caseDir));
    EXPECT_TRUE(std::filesystem::is_directory(caseDir));
}

TEST_F(FoamWriterTest, PolyMeshDirectoryExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path polyMesh = std::filesystem::path(caseDir) / "constant" / "polyMesh";

    EXPECT_TRUE(std::filesystem::exists(polyMesh));
}

TEST_F(FoamWriterTest, PointsFileExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path pointsFile =
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "points";

    EXPECT_TRUE(std::filesystem::exists(pointsFile));
}

TEST_F(FoamWriterTest, FacesFileExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path facesFile =
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "faces";

    EXPECT_TRUE(std::filesystem::exists(facesFile));
}

TEST_F(FoamWriterTest, OwnerFileExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path ownerFile =
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "owner";

    EXPECT_TRUE(std::filesystem::exists(ownerFile));
}

TEST_F(FoamWriterTest, NeighbourFileExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path neighbourFile =
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "neighbour";

    EXPECT_TRUE(std::filesystem::exists(neighbourFile));
}

TEST_F(FoamWriterTest, BoundaryFileExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path boundaryFile =
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "boundary";

    EXPECT_TRUE(std::filesystem::exists(boundaryFile));
}

TEST_F(FoamWriterTest, ControlDictExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path controlDict =
        std::filesystem::path(caseDir) / "system" / "controlDict";

    EXPECT_TRUE(std::filesystem::exists(controlDict));
}

TEST_F(FoamWriterTest, FvSchemesExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path fvSchemes =
        std::filesystem::path(caseDir) / "system" / "fvSchemes";

    EXPECT_TRUE(std::filesystem::exists(fvSchemes));
}

TEST_F(FoamWriterTest, FvSolutionExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path fvSolution =
        std::filesystem::path(caseDir) / "system" / "fvSolution";

    EXPECT_TRUE(std::filesystem::exists(fvSolution));
}

TEST_F(FoamWriterTest, VelocityFieldExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path uField = std::filesystem::path(caseDir) / "0" / "U";

    EXPECT_TRUE(std::filesystem::exists(uField));
}

TEST_F(FoamWriterTest, PressureFieldExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path pField = std::filesystem::path(caseDir) / "0" / "p";

    EXPECT_TRUE(std::filesystem::exists(pField));
}

TEST_F(FoamWriterTest, TransportPropertiesExists)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path transport =
        std::filesystem::path(caseDir) / "constant" / "transportProperties";

    EXPECT_TRUE(std::filesystem::exists(transport));
}

// Helper to read a file into a string
static std::string readFileContents(const std::filesystem::path& p)
{
    std::ifstream f(p);
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

TEST_F(FoamWriterTest, BoundaryPatchNamesMatchPython)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::string boundary = readFileContents(
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "boundary");

    // Inlet keeps positional name "left_x" for X flow
    EXPECT_NE(boundary.find("left_x"), std::string::npos);
    // Outlet is named "outlet"
    EXPECT_NE(boundary.find("outlet"), std::string::npos);
    // Should NOT have old-style "inlet" as a patch name (note: "inlet" may appear
    // in other contexts like "inlet_length", so check as a patch entry)
    EXPECT_EQ(boundary.find("    inlet\n"), std::string::npos);
    EXPECT_EQ(boundary.find("    walls\n"), std::string::npos);
}

TEST_F(FoamWriterTest, FaceSetsUsePositionalNames)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path setsDir =
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "sets";

    // Face sets should use positional names
    EXPECT_TRUE(std::filesystem::exists(setsDir / "left_x"));
    EXPECT_TRUE(std::filesystem::exists(setsDir / "right_x"));  // not "outlet"
    // Should NOT have display-name file for outlet
    EXPECT_FALSE(std::filesystem::exists(setsDir / "outlet"));
}

TEST_F(FoamWriterTest, CreatePatchDictUsesPositionalSetNames)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::string cpd = readFileContents(
        std::filesystem::path(caseDir) / "system" / "createPatchDict");

    // The 'set' field for the outlet should reference the positional name
    EXPECT_NE(cpd.find("set right_x"), std::string::npos);
    // The 'name' field should use the display name
    EXPECT_NE(cpd.find("name outlet"), std::string::npos);
}

TEST_F(FoamWriterTest, PressureFieldUsesPositionalInletName)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::string pFile = readFileContents(
        std::filesystem::path(caseDir) / "0" / "p");

    // Inlet patch in pressure file should be "left_x" (positional)
    EXPECT_NE(pFile.find("left_x"), std::string::npos);
    // Should not have "inlet" as a standalone patch entry
    EXPECT_EQ(pFile.find("    inlet\n"), std::string::npos);
}

TEST_F(FoamWriterTest, VelocityFieldUsesPositionalInletName)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::string uFile = readFileContents(
        std::filesystem::path(caseDir) / "0" / "U");

    // Inlet patch in velocity file should be "left_x" (positional)
    EXPECT_NE(uFile.find("left_x"), std::string::npos);
    // Should not have "inlet" or "walls" as standalone patch entries
    EXPECT_EQ(uFile.find("    inlet\n"), std::string::npos);
    EXPECT_EQ(uFile.find("    walls\n"), std::string::npos);
}

TEST_F(FoamWriterTest, BoundaryWallHasInGroups)
{
    // Use a geometry where some voxels are solid so the mesh has a "remaining" wall patch.
    // A 3x3x3 grid with a solid voxel in the interior creates internal wall faces.
    std::vector<int8_t> data(27, 1);
    data[13] = 0;  // solid voxel at (1,1,1) centre
    VoxelArray geom(data, 3, 3, 3);

    HexMeshBuilder::Options opts;
    opts.voxelSize = 1e-6;
    opts.flowDirection = FlowDirection::X;
    opts.connectivityCheck = false;
    opts.autoBoundaryFaceSets = true;
    opts.periodic = false;

    HexMeshBuilder builder(geom, opts);
    MeshData mesh = builder.build();

    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::string boundary = readFileContents(
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "boundary");

    // Wall patch should have inGroups
    EXPECT_NE(boundary.find("inGroups        1(wall)"), std::string::npos);
}

TEST_F(FoamWriterTest, BoundaryCyclicAMIHasInGroupsAndAttributes)
{
    // Build a mesh with periodic boundaries to get cyclicAMI patches
    std::vector<int8_t> data(8, 1);
    VoxelArray geom(data, 2, 2, 2);

    HexMeshBuilder::Options opts;
    opts.voxelSize = 1e-6;
    opts.flowDirection = FlowDirection::X;
    opts.connectivityCheck = false;
    opts.autoBoundaryFaceSets = true;
    opts.periodic = true;

    HexMeshBuilder builder(geom, opts);
    MeshData mesh = builder.build();

    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::string boundary = readFileContents(
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "boundary");

    // CyclicAMI patches should have inGroups
    EXPECT_NE(boundary.find("inGroups        1(cyclicAMI)"), std::string::npos);
    // Should have separationVector and requireMatch
    EXPECT_NE(boundary.find("separationVector (0 0 0)"), std::string::npos);
    EXPECT_NE(boundary.find("requireMatch    0"), std::string::npos);
}

TEST_F(FoamWriterTest, BoundaryCyclicAMIAttributeOrder)
{
    // Build a mesh with periodic boundaries
    std::vector<int8_t> data(8, 1);
    VoxelArray geom(data, 2, 2, 2);

    HexMeshBuilder::Options opts;
    opts.voxelSize = 1e-6;
    opts.flowDirection = FlowDirection::X;
    opts.connectivityCheck = false;
    opts.autoBoundaryFaceSets = true;
    opts.periodic = true;

    HexMeshBuilder builder(geom, opts);
    MeshData mesh = builder.build();

    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::string boundary = readFileContents(
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "boundary");

    // nFaces/startFace must come before matchTolerance (after type/inGroups)
    auto posNFaces = boundary.find("nFaces");
    auto posMatchTol = boundary.find("matchTolerance");
    ASSERT_NE(posNFaces, std::string::npos);
    ASSERT_NE(posMatchTol, std::string::npos);
    EXPECT_LT(posNFaces, posMatchTol)
        << "nFaces should appear before matchTolerance in cyclicAMI patches";
}

TEST_F(FoamWriterTest, PointsUseCompactFormat)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::string points = readFileContents(
        std::filesystem::path(caseDir) / "constant" / "polyMesh" / "points");

    // Should NOT contain verbose scientific notation like "0.00000e+00"
    EXPECT_EQ(points.find("0.00000e+00"), std::string::npos)
        << "Points should use compact %g format, not verbose %.5e";
    // Should contain compact format (e.g., "0" or "1e-06")
    EXPECT_NE(points.find("("), std::string::npos);
}

TEST_F(FoamWriterTest, VelocityZeroFormat)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    // Set some cells to have mixed zero/nonzero velocities
    if (!mesh.cellMap.empty())
    {
        auto it = mesh.cellMap.begin();
        it->second.u = 1.5e-3;
        it->second.v = 0.0;
        it->second.w = 0.0;
    }

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::string uFile = readFileContents(
        std::filesystem::path(caseDir) / "0" / "U");

    // Zero components should be "0.0e+00" not "0.00000e+00"
    if (uFile.find("nonuniform") != std::string::npos)
    {
        EXPECT_NE(uFile.find("0.0e+00"), std::string::npos)
            << "Zero velocity components should use '0.0e+00' format";
        EXPECT_EQ(uFile.find("0.00000e+00"), std::string::npos)
            << "Zero velocity components should NOT use '0.00000e+00' format";
    }
}

TEST_F(FoamWriterTest, NoBlockMeshDictGenerated)
{
    SimulationConfig cfg = makeConfig();
    FoamWriter writer(cfg);
    MeshData mesh = buildSimpleMesh();

    std::string caseDir = writer.writeCase(mesh, tmpDir_.string());
    std::filesystem::path blockMeshDict =
        std::filesystem::path(caseDir) / "system" / "blockMeshDict";

    EXPECT_FALSE(std::filesystem::exists(blockMeshDict))
        << "blockMeshDict should not be generated (reference case doesn't have one)";
}
