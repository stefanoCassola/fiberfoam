#include "io/FoamWriter.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace fiberfoam
{

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FoamWriter::FoamWriter(const SimulationConfig& config)
    : config_(config)
{
}

// ---------------------------------------------------------------------------
// Patch name helpers
// ---------------------------------------------------------------------------

std::string FoamWriter::inletPatchName() const
{
    switch (config_.flowDirections.front())
    {
    case FlowDirection::X:
        return "left_x";
    case FlowDirection::Y:
        return "front_y";
    case FlowDirection::Z:
        return "bottom_z";
    }
    return "left_x";
}

std::string FoamWriter::outletPatchName() const
{
    switch (config_.flowDirections.front())
    {
    case FlowDirection::X:
        return "right_x";
    case FlowDirection::Y:
        return "back_y";
    case FlowDirection::Z:
        return "top_z";
    }
    return "right_x";
}

bool FoamWriter::isInletPatch(const std::string& name) const
{
    return name == inletPatchName();
}

bool FoamWriter::isOutletPatch(const std::string& name) const
{
    return name == outletPatchName();
}

// ---------------------------------------------------------------------------
// OpenFOAM header
// ---------------------------------------------------------------------------

static const char* foamBanner()
{
    return
        "/*--------------------------------*- C++ -*----------------------------------*\\\n"
        "| =========                 |                                                 |\n"
        "|  \\\\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox           |\n"
        "|   \\\\    /   O peration     | Version:  8                                     |\n"
        "|    \\\\  /    A nd           | Web:      www.openfoam.com                      |\n"
        "|     \\\\/     M anipulation  |                                                 |\n"
        "\\*---------------------------------------------------------------------------*/\n";
}

static const char* foamSeparator()
{
    return "// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //";
}

static const char* foamFooter()
{
    return "\n// ************************************************************************* //";
}

std::string FoamWriter::foamHeader(const std::string& className,
                                   const std::string& object,
                                   const std::string& location) const
{
    std::ostringstream oss;
    oss << foamBanner();
    oss << "FoamFile\n";
    oss << "{\n";
    oss << "    version     2.0;\n";
    oss << "    format      ascii;\n";
    oss << "    arch      \"LSB;label=32;scalar=64\";\n";
    oss << "    class       " << className << ";\n";
    if (!location.empty())
    {
        oss << "    location    \"" << location << "\";\n";
    }
    oss << "    object      " << object << ";\n";
    oss << "}\n";
    oss << foamSeparator() << "\n";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Top-level write
// ---------------------------------------------------------------------------

std::string FoamWriter::writeCase(const MeshData& mesh, const std::string& basePath)
{
    FlowDirection dir = config_.flowDirections.front();
    std::string caseDir = basePath + "/" + directionName(dir) + "_dir";

    // Create directory structure
    fs::create_directories(caseDir + "/0");
    fs::create_directories(caseDir + "/constant/polyMesh/sets");
    fs::create_directories(caseDir + "/system");

    writePolyMesh(mesh, caseDir);
    writeVelocityField(mesh, caseDir);
    writePressureField(mesh, caseDir);
    writeControlDict(caseDir);
    writeFvSchemes(caseDir);
    writeFvSolution(caseDir);
    writeTransportProperties(caseDir);
    writeTurbulenceProperties(caseDir);
    writeCreatePatchDict(mesh, caseDir);
    writeBlockMeshDict(mesh, caseDir);

    return caseDir;
}

// ---------------------------------------------------------------------------
// polyMesh files
// ---------------------------------------------------------------------------

void FoamWriter::writePolyMesh(const MeshData& mesh, const std::string& caseDir)
{
    std::string polyMeshDir = caseDir + "/constant/polyMesh";
    writePoints(mesh, polyMeshDir);
    writeFaces(mesh, polyMeshDir);
    writeBoundary(mesh, polyMeshDir);
    writeOwner(mesh, polyMeshDir);
    writeNeighbour(mesh, polyMeshDir);
    writeFaceSets(mesh, polyMeshDir);
}

void FoamWriter::writePoints(const MeshData& mesh, const std::string& polyMeshDir)
{
    std::string path = polyMeshDir + "/points";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    file << foamHeader("vectorField", "points", "constant/polyMesh") << "\n";

    int nPoints = static_cast<int>(mesh.points.size());
    file << nPoints << "\n(\n";

    char buf[128];
    for (const auto& pt : mesh.points)
    {
        std::snprintf(buf, sizeof(buf), "(%.5e %.5e %.5e)", pt.x, pt.y, pt.z);
        file << buf << "\n";
    }

    file << ")\n";
    file << foamFooter();
}

void FoamWriter::writeFaces(const MeshData& mesh, const std::string& polyMeshDir)
{
    std::string path = polyMeshDir + "/faces";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    file << foamHeader("faceList", "faces", "constant/polyMesh") << "\n\n";

    int nFaces = static_cast<int>(mesh.faces.size());
    file << nFaces << "\n(\n";

    for (const auto& face : mesh.faces)
    {
        file << face.size() << "(" << face[0] << " " << face[1]
             << " " << face[2] << " " << face[3] << ")\n";
    }

    file << ")\n";
    file << foamFooter();
}

void FoamWriter::writeBoundary(const MeshData& mesh, const std::string& polyMeshDir)
{
    std::string path = polyMeshDir + "/boundary";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    file << foamHeader("polyBoundaryMesh", "boundary", "constant/polyMesh") << "\n";

    int nBoundaryFaces = static_cast<int>(mesh.faces.size()) - mesh.nInternalFaces;

    file << "1\n";
    file << "(\n";
    file << "    patchName\n";
    file << "    {\n";
    file << "        type            empty;\n";
    file << "        nFaces          " << nBoundaryFaces << ";\n";
    file << "        startFace       " << mesh.nInternalFaces << ";\n";
    file << "    }\n";
    file << ")\n";
    file << foamFooter();
}

void FoamWriter::writeOwner(const MeshData& mesh, const std::string& polyMeshDir)
{
    std::string path = polyMeshDir + "/owner";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    int nPoints = static_cast<int>(mesh.points.size());
    int nCells = mesh.nCells;
    int nFaces = static_cast<int>(mesh.faces.size());
    int nInternal = mesh.nInternalFaces;

    // Build header with note field for mesh counts
    std::ostringstream oss;
    oss << foamBanner();
    oss << "FoamFile\n";
    oss << "{\n";
    oss << "    version     2.0;\n";
    oss << "    format      ascii;\n";
    oss << "    arch      \"LSB;label=32;scalar=64\";\n";
    oss << "    note       \"nPoints:" << nPoints << "  nCells:" << nCells
        << "  nFaces:" << nFaces << "  nInternalFaces:" << nInternal << "\";\n";
    oss << "    class       labelList;\n";
    oss << "    location    \"constant/polyMesh\";\n";
    oss << "    object      owner;\n";
    oss << "}\n";
    oss << foamSeparator() << "\n";

    file << oss.str() << "\n";

    int nOwners = static_cast<int>(mesh.owner.size());
    file << nOwners << "\n(\n";
    for (int o : mesh.owner)
    {
        file << o << "\n";
    }
    file << ")\n";
    file << foamFooter();
}

void FoamWriter::writeNeighbour(const MeshData& mesh, const std::string& polyMeshDir)
{
    std::string path = polyMeshDir + "/neighbour";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    int nPoints = static_cast<int>(mesh.points.size());
    int nCells = mesh.nCells;
    int nFaces = static_cast<int>(mesh.faces.size());
    int nInternal = mesh.nInternalFaces;

    // Build header with note field for mesh counts
    std::ostringstream oss;
    oss << foamBanner();
    oss << "FoamFile\n";
    oss << "{\n";
    oss << "    version     2.0;\n";
    oss << "    format      ascii;\n";
    oss << "    arch      \"LSB;label=32;scalar=64\";\n";
    oss << "    note       \"nPoints:" << nPoints << "  nCells:" << nCells
        << "  nFaces:" << nFaces << "  nInternalFaces:" << nInternal << "\";\n";
    oss << "    class       labelList;\n";
    oss << "    location    \"constant/polyMesh\";\n";
    oss << "    object      neighbour;\n";
    oss << "}\n";
    oss << foamSeparator() << "\n";

    file << oss.str() << "\n";

    // Only internal faces have neighbours
    int nNeighbours = static_cast<int>(mesh.neighbour.size());
    file << nNeighbours << "\n(\n";
    for (int n : mesh.neighbour)
    {
        file << n << "\n";
    }
    file << ")\n";
    file << foamFooter();
}

void FoamWriter::writeFaceSets(const MeshData& mesh, const std::string& polyMeshDir)
{
    std::string setsDir = polyMeshDir + "/sets";
    fs::create_directories(setsDir);

    for (const auto& [patchName, patchInfo] : mesh.boundaryPatches)
    {
        int startFace = patchInfo.first;
        int nFaces = patchInfo.second;

        std::string path = setsDir + "/" + patchName;
        std::ofstream file(path);
        if (!file.is_open())
            throw std::runtime_error("Cannot open file: " + path);

        file << foamHeader("faceSet", patchName, "constant/polyMesh/sets") << "\n";

        file << nFaces << "\n(\n";
        for (int i = 0; i < nFaces; ++i)
        {
            file << (startFace + i) << "\n";
        }
        file << ")\n";
        file << foamFooter();
    }
}

// ---------------------------------------------------------------------------
// Field files (0/)
// ---------------------------------------------------------------------------

void FoamWriter::writeVelocityField(const MeshData& mesh, const std::string& caseDir)
{
    std::string path = caseDir + "/0/U";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    file << foamHeader("volVectorField", "U", "0") << "\n";
    file << "dimensions      [0 1 -1 0 0 0 0];\n\n";

    // Check if cells carry velocity data
    bool hasVelocity = false;
    for (const auto& [id, cell] : mesh.cellMap)
    {
        if (cell.u != 0.0 || cell.v != 0.0 || cell.w != 0.0)
        {
            hasVelocity = true;
            break;
        }
    }

    if (hasVelocity)
    {
        file << "internalField   nonuniform List<vector>\n";
        file << mesh.cellMap.size() << "\n";
        file << "(\n";

        char buf[128];
        for (const auto& [id, cell] : mesh.cellMap)
        {
            std::snprintf(buf, sizeof(buf), "(%.5e %.5e %.5e)", cell.u, cell.v, cell.w);
            file << buf << "\n";
        }

        file << ");\n";
    }
    else
    {
        file << "internalField   uniform (0 0 0);\n";
    }

    file << "boundaryField\n";
    file << "{\n";

    if (!mesh.boundaryPatches.empty())
    {
        for (const auto& [patchName, patchInfo] : mesh.boundaryPatches)
        {
            if (isOutletPatch(patchName))
            {
                file << "    outlet\n";
                file << "    {\n";
                file << "        type            zeroGradient;\n";
                file << "    }\n";
            }
            else if (isInletPatch(patchName))
            {
                file << "    " << patchName << "\n";
                file << "    {\n";
                file << "        type            zeroGradient;\n";
                file << "    }\n";
            }
            else if (patchName == "remaining")
            {
                file << "    " << patchName << "\n";
                file << "    {\n";
                file << "        type            fixedValue;\n";
                file << "        value           uniform (0 0 0);\n";
                file << "    }\n";
            }
            else
            {
                // Periodic patches
                file << "    " << patchName << "\n";
                file << "    {\n";
                file << "        type            cyclicAMI;\n";
                file << "    }\n";
            }
        }
    }
    else
    {
        file << "    patchName\n";
        file << "    {\n";
        file << "        type            empty;\n";
        file << "    }\n";
    }

    file << "}\n";
    file << foamFooter();
}

void FoamWriter::writePressureField(const MeshData& mesh, const std::string& caseDir)
{
    std::string path = caseDir + "/0/p";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    file << foamHeader("volScalarField", "p", "0") << "\n";
    file << "#include \"../constant/transportProperties\"\n\n";
    file << "dimensions      [0 2 -2 0 0 0 0]; "
         << "//[kg m s K kgmol A cd] --> [Mass Length Time Temperature "
         << "Quantity Current Luminous intensiy]\n\n";
    file << "internalField   uniform 0;\n";

    file << "boundaryField\n";
    file << "{\n";

    double pInKinematic = config_.fluid.pressureInlet / config_.fluid.density;

    if (!mesh.boundaryPatches.empty())
    {
        for (const auto& [patchName, patchInfo] : mesh.boundaryPatches)
        {
            if (isOutletPatch(patchName))
            {
                file << "    outlet\n";
                file << "    {\n";
                file << "        type            fixedValue;\n";
                file << "        value           uniform 0;\n";
                file << "    }\n";
            }
            else if (isInletPatch(patchName))
            {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.12g", pInKinematic);
                file << "    " << patchName << "\n";
                file << "    {\n";
                file << "        type            fixedValue;\n";
                file << "        value           uniform " << buf << ";\n";
                file << "    }\n";
            }
            else if (patchName == "remaining")
            {
                file << "    " << patchName << "\n";
                file << "    {\n";
                file << "        type            zeroGradient;\n";
                file << "    }\n";
            }
            else
            {
                // Periodic patches
                file << "    " << patchName << "\n";
                file << "    {\n";
                file << "        type            cyclicAMI;\n";
                file << "    }\n";
            }
        }
    }
    else
    {
        file << "    patchName\n";
        file << "    {\n";
        file << "        type            empty;\n";
        file << "    }\n";
    }

    file << "}\n";
    file << foamFooter();
}

// ---------------------------------------------------------------------------
// system/ files
// ---------------------------------------------------------------------------

void FoamWriter::writeControlDict(const std::string& caseDir)
{
    std::string path = caseDir + "/system/controlDict";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    // Use dictionary header (no arch line needed for system files)
    std::ostringstream hdr;
    hdr << foamBanner();
    hdr << "FoamFile\n";
    hdr << "{\n";
    hdr << "    version     2.0;\n";
    hdr << "    format      ascii;\n";
    hdr << "    class       dictionary;\n";
    hdr << "    location    \"system\";\n";
    hdr << "    object      controlDict;\n";
    hdr << "}\n";
    hdr << foamSeparator() << "\n";

    file << hdr.str() << "\n";
    file << "\n";
    file << "libs        (utilityFunctionObjects);\n";
    file << "\n";
    file << "DebugSwitches\n";
    file << "{\n";
    file << "//    fvScalarMatrix      2;\n";
    file << "//    fvVectorMatrix      2;\n";
    file << "//    fvMatrix            2;\n";
    file << "}\n\n";
    file << "application     " << config_.solverName << ";\n\n";
    file << "startFrom       startTime;\n\n";
    file << "startTime       0;\n\n";
    file << "stopAt          endTime;\n\n";
    file << "endTime         " << config_.maxIterations << ";\n\n";
    file << "deltaT          1;\n\n";
    file << "writeControl    timeStep;\n\n";
    file << "writeInterval   " << config_.writeInterval << ";\n\n";
    file << "purgeWrite      0;\n\n";
    file << "writeFormat     ascii;\n\n";
    file << "writePrecision  6;\n\n";
    file << "writeCompression off;\n\n";
    file << "timeFormat      general;\n\n";
    file << "timePrecision   6;\n\n";
    file << "runTimeModifiable true;\n\n";
    file << "\n";
    file << foamFooter();
}

void FoamWriter::writeFvSchemes(const std::string& caseDir)
{
    std::string path = caseDir + "/system/fvSchemes";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    std::ostringstream hdr;
    hdr << foamBanner();
    hdr << "FoamFile\n";
    hdr << "{\n";
    hdr << "    version     2.0;\n";
    hdr << "    format      ascii;\n";
    hdr << "    class       dictionary;\n";
    hdr << "    location    \"system\";\n";
    hdr << "    object      fVSchemes;\n";
    hdr << "}\n";
    hdr << foamSeparator() << "\n";

    file << hdr.str() << "\n";

    file << "ddtSchemes\n";
    file << "{\n";
    file << "    default         steadyState;\n";
    file << "}\n\n";

    file << "gradSchemes\n";
    file << "{\n";
    file << "    default         Gauss linear;\n";
    file << "    grad(T)         Gauss linear;\n";
    file << "}\n\n";

    file << "divSchemes\n";
    file << "{\n";
    file << "    default         none;\n";
    file << "    div(phi,U)      bounded Gauss linear;\n";
    file << "    div((nuEff*dev2(T(grad(U))))) Gauss linear;\n";
    file << "}\n\n";

    file << "laplacianSchemes\n";
    file << "{\n";
    file << "    default         none;\n";
    file << "    laplacian(DT,T)     Gauss linear corrected;\n";
    file << "    laplacian(DTV,T)    Gauss linear corrected;\n";
    file << "    laplacian(nuEff,U)  Gauss linear corrected;\n";
    file << "    laplacian((1|A(U)),p) Gauss linear corrected;\n";
    file << "    laplacian((1|((1|(1|A(U)))-H(1))),p)    Gauss linear corrected;\n";
    file << "}\n\n";

    file << "interpolationSchemes\n";
    file << "{\n";
    file << "    default         linear;\n";
    file << "}\n\n";

    file << "snGradSchemes\n";
    file << "{\n";
    file << "    default         corrected;\n";
    file << "}\n\n";

    file << foamFooter();
}

void FoamWriter::writeFvSolution(const std::string& caseDir)
{
    std::string path = caseDir + "/system/fvSolution";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    std::ostringstream hdr;
    hdr << foamBanner();
    hdr << "FoamFile\n";
    hdr << "{\n";
    hdr << "    version     2.0;\n";
    hdr << "    format      ascii;\n";
    hdr << "    class       dictionary;\n";
    hdr << "    location    \"system\";\n";
    hdr << "    object      fVSolution;\n";
    hdr << "}\n";
    hdr << foamSeparator() << "\n";

    file << hdr.str() << "\n";

    file << "solvers\n";
    file << "{\n";
    file << "    p\n";
    file << "    {\n";
    file << "        solver          GAMG;\n";
    file << "        smoother        GaussSeidel;\n";
    file << "        tolerance       1e-7;\n";
    file << "        relTol          0;\n";
    file << "    }\n";
    file << "    U\n";
    file << "    {\n";
    file << "        solver          smoothSolver;\n";
    file << "        smoother        GaussSeidel;\n";
    file << "        tolerance       1e-8;\n";
    file << "        relTol          0;\n";
    file << "        nSweeps         1;\n";
    file << "    }\n";
    file << "}\n\n";

    file << "SIMPLE\n";
    file << "{\n";
    file << "    nNonOrthogonalCorrectors 0;\n";
    file << "    consistent true;\n";
    file << "    permeabilityControl\n";
    file << "        {\n";

    file << "        convPermeability        "
         << (config_.convPermeability ? "true" : "false")
         << ";           //enable permeability convergence criteria\n";

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", config_.convSlope);
    file << "        convSlope               " << buf
         << ";           //slope of the regression line that is calculated "
            "based on the last <window> permeabilty --> 0 equals flat/"
            "horizontal line;\n";

    file << "        convWindow              " << config_.convWindow
         << ";             //amount of previous permeability values that is "
            "used to calculated linear regression (also used for linear "
            "prediction of permeability)\n";

    std::snprintf(buf, sizeof(buf), "%g", config_.errorBound);
    file << "        errorBound              " << buf
         << ";           //Error between predicted and calculated "
            "permeability\n";

    file << "        }\n";
    file << "}\n\n";

    file << "relaxationFactors\n";
    file << "{\n";
    file << "    equations\n";
    file << "    {\n";
    file << "        U               0.9;\n";
    file << "    }\n\n";
    file << "    fields\n";
    file << "    {\n";
    file << "        p               0.6;\n";
    file << "    }\n";
    file << "}\n\n";

    file << "cache\n";
    file << "{\n";
    file << "    grad(U);\n";
    file << "}\n\n";

    file << foamFooter();
}

// ---------------------------------------------------------------------------
// constant/ files
// ---------------------------------------------------------------------------

void FoamWriter::writeTransportProperties(const std::string& caseDir)
{
    std::string path = caseDir + "/constant/transportProperties";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    std::ostringstream hdr;
    hdr << foamBanner();
    hdr << "FoamFile\n";
    hdr << "{\n";
    hdr << "    version     2.0;\n";
    hdr << "    format      ascii;\n";
    hdr << "    class       dictionary;\n";
    hdr << "    location    \"constant\";\n";
    hdr << "    object      transportProperties;\n";
    hdr << "}\n";
    hdr << foamSeparator() << "\n";

    file << hdr.str() << "\n";

    file << "//- For laplacianFoam\n";
    file << "DT              4e-05;\n\n";

    file << "//- For simpleFoam\n";
    file << "transportModel  Newtonian;\n";

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.12g", config_.fluid.kinematicViscosity);
    file << "nu              " << buf
         << ";         // [0 2 -1 0 0 0 0] --> kinematic viscosity needs to "
            "be used here (equals dyn. viscosity of "
         << config_.fluid.dynamicViscosity << "kg/ms)\n\n";

    std::snprintf(buf, sizeof(buf), "%g", config_.fluid.density);
    file << "density         " << buf << ";\n";

    std::snprintf(buf, sizeof(buf), "%g", config_.fluid.pressureInlet);
    file << "pIn             " << buf << ";\n";

    std::snprintf(buf, sizeof(buf), "%g", config_.fluid.pressureOutlet);
    file << "pOut            " << buf << ";\n\n";

    file << foamFooter();
}

void FoamWriter::writeTurbulenceProperties(const std::string& caseDir)
{
    std::string path = caseDir + "/constant/turbulenceProperties";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    std::ostringstream hdr;
    hdr << foamBanner();
    hdr << "FoamFile\n";
    hdr << "{\n";
    hdr << "    version     2.0;\n";
    hdr << "    format      ascii;\n";
    hdr << "    class       dictionary;\n";
    hdr << "    location    \"constant\";\n";
    hdr << "    object      turbulenceProperties;\n";
    hdr << "}\n";
    hdr << foamSeparator() << "\n";

    file << hdr.str() << "\n";
    file << "simulationType laminar;\n";
    file << foamFooter();
}

// ---------------------------------------------------------------------------
// createPatchDict
// ---------------------------------------------------------------------------

static std::string cyclicNeighbour(const std::string& name)
{
    if (name == "left_x")
        return "right_x";
    if (name == "right_x")
        return "left_x";
    if (name == "front_y")
        return "back_y";
    if (name == "back_y")
        return "front_y";
    if (name == "bottom_z")
        return "top_z";
    if (name == "top_z")
        return "bottom_z";
    return "";
}

void FoamWriter::writeCreatePatchDict(const MeshData& mesh, const std::string& caseDir)
{
    std::string path = caseDir + "/system/createPatchDict";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    std::ostringstream hdr;
    hdr << foamBanner();
    hdr << "FoamFile\n";
    hdr << "{\n";
    hdr << "    version     2.0;\n";
    hdr << "    format      ascii;\n";
    hdr << "    class       dictionary;\n";
    hdr << "    location    \"system\";\n";
    hdr << "    object      controlDict;\n"; // matches Python original
    hdr << "}\n";
    hdr << foamSeparator() << "\n";

    file << hdr.str() << "\n";
    file << "pointSync true;\n";
    file << "// Patches to create.\n";
    file << "patches\n";
    file << "(\n";

    for (const auto& [patchName, patchInfo] : mesh.boundaryPatches)
    {
        file << "    {\n";

        if (isOutletPatch(patchName))
        {
            file << "    name outlet;\n";
            file << "    patchInfo\n";
            file << "            {\n";
            file << "            type patch;\n";
            file << "            }\n";
        }
        else if (isInletPatch(patchName))
        {
            file << "    name " << patchName << ";\n";
            file << "    patchInfo\n";
            file << "            {\n";
            file << "            type patch;\n";
            file << "            }\n";
        }
        else if (patchName == "remaining")
        {
            file << "    name " << patchName << ";\n";
            file << "    patchInfo\n";
            file << "            {\n";
            file << "            type wall;\n";
            file << "            }\n";
        }
        else
        {
            std::string neighbour = cyclicNeighbour(patchName);
            file << "    name " << patchName << ";\n";
            file << "    patchInfo\n";
            file << "            {\n";
            file << "            type cyclicAMI;\n";
            file << "            matchTolerance  0.01;\n";
            file << "            neighbourPatch  " << neighbour << ";\n";
            file << "            transform       translational;\n";
            file << "            separationVector (0 0 0);\n";
            file << "            AMIMethod       nearestFaceAMI;\n";
            file << "            }\n";
        }

        file << "    constructFrom set;\n";
        file << "    set " << patchName << ";\n";
        file << "    }\n\n";
    }

    file << ");\n";
    file << "\n";
    file << foamFooter();
}

// ---------------------------------------------------------------------------
// blockMeshDict -- mesh bounds and scale for permCalc.H to read
// ---------------------------------------------------------------------------

void FoamWriter::writeBlockMeshDict(const MeshData& mesh, const std::string& caseDir)
{
    std::string path = caseDir + "/system/blockMeshDict";
    std::ofstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);

    std::ostringstream hdr;
    hdr << foamBanner();
    hdr << "FoamFile\n";
    hdr << "{\n";
    hdr << "    version     2.0;\n";
    hdr << "    format      ascii;\n";
    hdr << "    class       dictionary;\n";
    hdr << "    location    \"system\";\n";
    hdr << "    object      blockMeshDict;\n";
    hdr << "}\n";
    hdr << foamSeparator() << "\n";

    file << hdr.str() << "\n";

    // Compute mesh bounding box
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0, zmin = 0, zmax = 0;
    if (!mesh.points.empty())
    {
        xmin = xmax = mesh.points[0].x;
        ymin = ymax = mesh.points[0].y;
        zmin = zmax = mesh.points[0].z;
        for (const auto& pt : mesh.points)
        {
            if (pt.x < xmin) xmin = pt.x;
            if (pt.x > xmax) xmax = pt.x;
            if (pt.y < ymin) ymin = pt.y;
            if (pt.y > ymax) ymax = pt.y;
            if (pt.z < zmin) zmin = pt.z;
            if (pt.z > zmax) zmax = pt.z;
        }
    }

    double scale = config_.voxelSize;

    file << "scale   " << scale << ";\n\n";

    char buf[64];

    std::snprintf(buf, sizeof(buf), "%.12g", xmin);
    file << "Nxmin   " << buf << ";\n";
    std::snprintf(buf, sizeof(buf), "%.12g", xmax);
    file << "Nxmax   " << buf << ";\n";
    std::snprintf(buf, sizeof(buf), "%.12g", ymin);
    file << "Nymin   " << buf << ";\n";
    std::snprintf(buf, sizeof(buf), "%.12g", ymax);
    file << "Nymax   " << buf << ";\n";
    std::snprintf(buf, sizeof(buf), "%.12g", zmin);
    file << "Nzmin   " << buf << ";\n";
    std::snprintf(buf, sizeof(buf), "%.12g", zmax);
    file << "Nzmax   " << buf << ";\n\n";

    // Inlet / outlet buffer lengths (in voxel units, scale applied by reader)
    file << "inlet_length    " << config_.inletBufferLayers << ";\n";
    file << "outlet_length   " << config_.outletBufferLayers << ";\n\n";

    file << foamFooter();
}

} // namespace fiberfoam
