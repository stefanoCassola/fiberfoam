#include <gtest/gtest.h>
#include "io/FoamWriter.h"
#include "mesh/HexMeshBuilder.h"
#include "geometry/VoxelArray.h"

#include <filesystem>

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
