#include "mesh/HexMeshBuilder.h"
#include "mesh/Connectivity.h"
#include "mesh/FaceGenerator.h"
#include "common/Logger.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace fiberfoam
{

// ---------------------------------------------------------------------------
// Helper: hash for a sorted 4-int face key (used to detect shared faces)
// ---------------------------------------------------------------------------
namespace
{

struct FaceKeyHash
{
    std::size_t operator()(const FaceVertices& f) const
    {
        std::size_t h = 0;
        for (int v : f)
        {
            h ^= std::hash<int>()(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

// Make a canonical (sorted) face key from the original 4 vertex indices
FaceVertices makeSortedKey(const FaceVertices& face)
{
    FaceVertices key = face;
    std::sort(key.begin(), key.end());
    return key;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
HexMeshBuilder::HexMeshBuilder(const VoxelArray& geometry, Options opts)
    : geometry_(geometry)
    , opts_(opts)
{
}

// ---------------------------------------------------------------------------
// build()  -- full pipeline
// ---------------------------------------------------------------------------
MeshData HexMeshBuilder::build()
{
    Logger::info("Generating mesh...");

    generateCellMap();

    if (opts_.connectivityCheck)
    {
        filterByConnectivity();
    }

    generatePoints();
    generateFaces();

    if (opts_.autoBoundaryFaceSets)
    {
        classifyBoundaryPatches();
    }

    mesh_.nCells = static_cast<int>(mesh_.cellMap.size());

    Logger::info("Mesh generation complete: "
                 + std::to_string(mesh_.points.size()) + " points, "
                 + std::to_string(mesh_.faces.size()) + " faces ("
                 + std::to_string(mesh_.nInternalFaces) + " internal), "
                 + std::to_string(mesh_.nCells) + " cells");

    return mesh_;
}

// ---------------------------------------------------------------------------
// Step 1: generateCellMap
// Iterate z, y, x over the voxel array. For each non-zero voxel, create
// a CellData entry indexed sequentially.
// ---------------------------------------------------------------------------
void HexMeshBuilder::generateCellMap()
{
    Logger::info("Generating cell map...");

    mesh_.cellMap.clear();
    int nx = geometry_.nx();
    int ny = geometry_.ny();
    int nz = geometry_.nz();
    int cellIndex = 0;

    for (int z = 0; z < nz; ++z)
    {
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                if (geometry_.at(x, y, z) != 0)
                {
                    buildCellEntry(x, y, z, cellIndex);
                    ++cellIndex;
                }
            }
        }
    }

    Logger::info("Cell map has " + std::to_string(mesh_.cellMap.size()) + " cells");
}

void HexMeshBuilder::buildCellEntry(int x, int y, int z, int cellIndex)
{
    CellData cell;
    cell.coord = {x, y, z};

    int nx = geometry_.nx();
    int ny = geometry_.ny();

    // Flat index into the field arrays (x + nx*(y + ny*z)) -- same layout as VoxelArray
    int flatIdx = x + nx * (y + ny * z);

    // Optional velocity field: single-component along flow direction
    if (opts_.velocityField)
    {
        double vel = opts_.velocityField[flatIdx];
        switch (opts_.flowDirection)
        {
        case FlowDirection::X:
            cell.u = vel;
            break;
        case FlowDirection::Y:
            cell.v = vel;
            break;
        case FlowDirection::Z:
            cell.w = vel;
            break;
        }
    }

    // Optional pressure field
    if (opts_.pressureField)
    {
        cell.p = opts_.pressureField[flatIdx];
    }

    // Optional region mask
    if (opts_.regionMask)
    {
        cell.region = static_cast<CellRegion>(opts_.regionMask[flatIdx]);
    }

    mesh_.cellMap[cellIndex] = cell;
}

// ---------------------------------------------------------------------------
// Step 2: filterByConnectivity
// Find the largest 6-connected component via BFS and keep only those cells.
// Re-index from 0.
// ---------------------------------------------------------------------------
void HexMeshBuilder::filterByConnectivity()
{
    Logger::info("Filtering cell map by connectivity...");

    std::set<int> largestComponent = findLargestComponent(mesh_.cellMap, geometry_);
    mesh_.cellMap = filterCellMap(mesh_.cellMap, largestComponent);

    Logger::info("Cell map after connectivity filter: "
                 + std::to_string(mesh_.cellMap.size()) + " cells");
}

// ---------------------------------------------------------------------------
// Step 3: generatePoints
// For each cell compute centre, then 8 hex vertices at +/- voxelSize/2.
// Collect all unique vertices sorted by (z, y, x). Build per-cell
// vertex-index mapping into the global points list.
// ---------------------------------------------------------------------------
void HexMeshBuilder::generateCellVertices(const Point3D& center,
                                          std::array<Point3D, 8>& verts) const
{
    double vs = opts_.voxelSize / 2.0;
    double cx = center.x, cy = center.y, cz = center.z;

    // Same vertex ordering as the Python _generate_cell_vertices:
    //   0: (x-vs, y-vs, z-vs)   1: (x+vs, y-vs, z-vs)
    //   2: (x+vs, y-vs, z+vs)   3: (x-vs, y-vs, z+vs)
    //   4: (x-vs, y+vs, z-vs)   5: (x+vs, y+vs, z-vs)
    //   6: (x+vs, y+vs, z+vs)   7: (x-vs, y+vs, z+vs)
    verts[0] = {cx - vs, cy - vs, cz - vs};
    verts[1] = {cx + vs, cy - vs, cz - vs};
    verts[2] = {cx + vs, cy - vs, cz + vs};
    verts[3] = {cx - vs, cy - vs, cz + vs};
    verts[4] = {cx - vs, cy + vs, cz - vs};
    verts[5] = {cx + vs, cy + vs, cz - vs};
    verts[6] = {cx + vs, cy + vs, cz + vs};
    verts[7] = {cx - vs, cy + vs, cz + vs};
}

void HexMeshBuilder::sortVertices(std::array<Point3D, 8>& verts) const
{
    // Sort by (z, y, x) -- matching the Python _sort_vertices
    std::sort(verts.begin(), verts.end(),
              [](const Point3D& a, const Point3D& b)
              {
                  if (a.z != b.z) return a.z < b.z;
                  if (a.y != b.y) return a.y < b.y;
                  return a.x < b.x;
              });
}

void HexMeshBuilder::generatePoints()
{
    Logger::info("Generating points...");

    int nCells = static_cast<int>(mesh_.cellMap.size());
    double vs = opts_.voxelSize;

    // --- Step 3a: Compute cell centres and sorted vertices per cell ---
    // cellVertices[cellIndex] = 8 sorted Point3D vertices
    std::vector<std::array<Point3D, 8>> cellVertices(nCells);

    for (const auto& [cellIdx, cellData] : mesh_.cellMap)
    {
        Point3D center;
        center.x = cellData.coord[0] * vs + vs / 2.0;
        center.y = cellData.coord[1] * vs + vs / 2.0;
        center.z = cellData.coord[2] * vs + vs / 2.0;

        std::array<Point3D, 8> verts;
        generateCellVertices(center, verts);
        sortVertices(verts);
        cellVertices[cellIdx] = verts;
    }

    // --- Step 3b: Collect unique points, sort by (z, y, x) ---
    // Use a set with Point3D::operator< (which sorts by z, y, x)
    std::set<Point3D> uniquePointsSet;
    for (const auto& verts : cellVertices)
    {
        for (const auto& pt : verts)
        {
            uniquePointsSet.insert(pt);
        }
    }

    // The set is already sorted by (z, y, x) due to Point3D::operator<
    mesh_.points.clear();
    mesh_.points.reserve(uniquePointsSet.size());
    std::map<Point3D, int> pointToIndex;

    int idx = 0;
    for (const auto& pt : uniquePointsSet)
    {
        mesh_.points.push_back(pt);
        pointToIndex[pt] = idx;
        ++idx;
    }

    // --- Step 3c: Build cell-to-vertex-index mapping ---
    cellVertexIndices_.resize(nCells);
    for (const auto& [cellIdx, cellData] : mesh_.cellMap)
    {
        const auto& verts = cellVertices[cellIdx];
        for (int v = 0; v < 8; ++v)
        {
            cellVertexIndices_[cellIdx][v] = pointToIndex.at(verts[v]);
        }
    }

    Logger::info("Generated " + std::to_string(mesh_.points.size()) + " unique points");
}

// ---------------------------------------------------------------------------
// Step 4: generateFaces
// For each cell, generate 6 faces using the HEX_FACE_DEFS local vertex
// indices mapped through cellVertexIndices_. Count face occurrences
// (sorted vertex tuple as key). Count==1 -> boundary, Count==2 -> internal.
// Build owner/neighbour lists with owner < neighbour for internal faces.
// ---------------------------------------------------------------------------
void HexMeshBuilder::generateFaces()
{
    Logger::info("Generating faces...");

    int nCells = static_cast<int>(mesh_.cellMap.size());

    // --- Step 4a: Generate all faces per cell, count occurrences ---
    // Map: sorted face key -> FaceInfo{original face vertices, list of owning cells}
    std::unordered_map<FaceVertices, FaceInfo, FaceKeyHash> faceMap;

    for (const auto& [cellIdx, cellData] : mesh_.cellMap)
    {
        const auto& vertIndices = cellVertexIndices_[cellIdx];

        for (const auto& faceDef : HEX_FACE_DEFS)
        {
            FaceVertices face = {
                vertIndices[faceDef[0]],
                vertIndices[faceDef[1]],
                vertIndices[faceDef[2]],
                vertIndices[faceDef[3]]
            };

            FaceVertices sortedKey = makeSortedKey(face);

            auto it = faceMap.find(sortedKey);
            if (it == faceMap.end())
            {
                FaceInfo info;
                info.vertices = face; // store the original (unsorted) face
                info.cells.push_back(cellIdx);
                faceMap[sortedKey] = std::move(info);
            }
            else
            {
                it->second.cells.push_back(cellIdx);
            }
        }
    }

    // --- Step 4b: Classify faces as internal or boundary ---
    struct InternalFace
    {
        FaceVertices vertices;
        int owner;    // lower cell index
        int neighbour; // higher cell index
    };

    struct BoundaryFace
    {
        FaceVertices vertices;
        int owner;
    };

    std::vector<InternalFace> internalFaces;
    std::vector<BoundaryFace> boundaryFaces;

    for (auto& [key, info] : faceMap)
    {
        if (info.cells.size() == 2)
        {
            InternalFace iface;
            iface.vertices = info.vertices;
            int c0 = info.cells[0];
            int c1 = info.cells[1];
            iface.owner = std::min(c0, c1);
            iface.neighbour = std::max(c0, c1);
            internalFaces.push_back(iface);
        }
        else if (info.cells.size() == 1)
        {
            BoundaryFace bface;
            bface.vertices = info.vertices;
            bface.owner = info.cells[0];
            boundaryFaces.push_back(bface);
        }
        else
        {
            throw std::runtime_error(
                "Face is associated with " + std::to_string(info.cells.size())
                + " cells (expected 1 or 2)");
        }
    }

    // --- Step 4c: Assemble faces list: internal first, then boundary ---
    // Sort internal faces by (owner, neighbour) for OpenFOAM compatibility
    std::sort(internalFaces.begin(), internalFaces.end(),
              [](const InternalFace& a, const InternalFace& b)
              {
                  if (a.owner != b.owner) return a.owner < b.owner;
                  return a.neighbour < b.neighbour;
              });

    mesh_.nInternalFaces = static_cast<int>(internalFaces.size());

    mesh_.faces.clear();
    mesh_.owner.clear();
    mesh_.neighbour.clear();

    mesh_.faces.reserve(internalFaces.size() + boundaryFaces.size());
    mesh_.owner.reserve(internalFaces.size() + boundaryFaces.size());
    mesh_.neighbour.reserve(internalFaces.size());

    for (const auto& iface : internalFaces)
    {
        mesh_.faces.push_back(iface.vertices);
        mesh_.owner.push_back(iface.owner);
        mesh_.neighbour.push_back(iface.neighbour);
    }

    for (const auto& bface : boundaryFaces)
    {
        mesh_.faces.push_back(bface.vertices);
        mesh_.owner.push_back(bface.owner);
    }

    Logger::info("Generated " + std::to_string(internalFaces.size()) + " internal faces, "
                 + std::to_string(boundaryFaces.size()) + " boundary faces");
}

// ---------------------------------------------------------------------------
// Step 5: classifyBoundaryPatches
// Determine mesh bounds. Classify each boundary face by its position:
//   left_x (x_min), right_x (x_max), front_y (y_min), back_y (y_max),
//   bottom_z (z_min), top_z (z_max). Remaining boundary faces -> "walls".
// Then rename inlet/outlet based on flow direction.
// Reorder faces so that internal faces come first, then boundary faces
// grouped by patch, and update owner/neighbour accordingly.
// ---------------------------------------------------------------------------
void HexMeshBuilder::classifyBoundaryPatches()
{
    Logger::info("Classifying boundary patches...");

    if (mesh_.faces.empty())
        return;

    // --- Step 5a: Compute mesh bounds ---
    double xMin = mesh_.points[0].x, xMax = mesh_.points[0].x;
    double yMin = mesh_.points[0].y, yMax = mesh_.points[0].y;
    double zMin = mesh_.points[0].z, zMax = mesh_.points[0].z;

    for (const auto& pt : mesh_.points)
    {
        if (pt.x < xMin) xMin = pt.x;
        if (pt.x > xMax) xMax = pt.x;
        if (pt.y < yMin) yMin = pt.y;
        if (pt.y > yMax) yMax = pt.y;
        if (pt.z < zMin) zMin = pt.z;
        if (pt.z > zMax) zMax = pt.z;
    }

    Logger::info("Mesh bounds: X[" + std::to_string(xMin) + ", " + std::to_string(xMax)
                 + "] Y[" + std::to_string(yMin) + ", " + std::to_string(yMax)
                 + "] Z[" + std::to_string(zMin) + ", " + std::to_string(zMax) + "]");

    double halfVs = opts_.voxelSize / 2.0;

    // Bounding boxes for each boundary side (matching Python _calc_boxes_for_boundaries)
    struct BBox
    {
        double minX, minY, minZ;
        double maxX, maxY, maxZ;
    };

    auto makeBBox = [&](double bMinX, double bMinY, double bMinZ,
                        double bMaxX, double bMaxY, double bMaxZ) -> BBox
    {
        return {bMinX, bMinY, bMinZ, bMaxX, bMaxY, bMaxZ};
    };

    BBox leftBox   = makeBBox(xMin - halfVs, yMin - halfVs, zMin - halfVs,
                              xMin + halfVs, yMax + halfVs, zMax + halfVs);
    BBox rightBox  = makeBBox(xMax - halfVs, yMin - halfVs, zMin - halfVs,
                              xMax + halfVs, yMax + halfVs, zMax + halfVs);
    BBox frontBox  = makeBBox(xMin - halfVs, yMin - halfVs, zMin - halfVs,
                              xMax + halfVs, yMin + halfVs, zMax + halfVs);
    BBox backBox   = makeBBox(xMin - halfVs, yMax - halfVs, zMin - halfVs,
                              xMax + halfVs, yMax + halfVs, zMax + halfVs);
    BBox bottomBox = makeBBox(xMin - halfVs, yMin - halfVs, zMin - halfVs,
                              xMax + halfVs, yMax + halfVs, zMin + halfVs);
    BBox topBox    = makeBBox(xMin - halfVs, yMin - halfVs, zMax - halfVs,
                              xMax + halfVs, yMax + halfVs, zMax + halfVs);

    // Check if all 4 vertices of a face lie within a bounding box
    auto faceInBox = [&](int faceIdx, const BBox& box) -> bool
    {
        const FaceVertices& fv = mesh_.faces[faceIdx];
        for (int vi : fv)
        {
            const Point3D& pt = mesh_.points[vi];
            if (pt.x < box.minX || pt.x > box.maxX ||
                pt.y < box.minY || pt.y > box.maxY ||
                pt.z < box.minZ || pt.z > box.maxZ)
            {
                return false;
            }
        }
        return true;
    };

    // --- Step 5b: Classify boundary faces ---
    // Boundary faces are those at indices [nInternalFaces, nFaces)
    int nTotal = static_cast<int>(mesh_.faces.size());
    int nInt = mesh_.nInternalFaces;

    // Named patch lists, each storing original face indices
    // Order of classification matches the Python: left, right, front, back, bottom, top
    struct PatchEntry
    {
        std::string name;
        std::vector<int> faceIndices;
    };

    // We use a "remaining" set and process patches in order, removing classified faces
    std::set<int> remaining;
    for (int i = nInt; i < nTotal; ++i)
    {
        remaining.insert(i);
    }

    auto classifyPatch = [&](const std::string& patchName, const BBox& box) -> PatchEntry
    {
        PatchEntry patch;
        patch.name = patchName;
        std::set<int> toRemove;
        for (int fi : remaining)
        {
            if (faceInBox(fi, box))
            {
                patch.faceIndices.push_back(fi);
                toRemove.insert(fi);
            }
        }
        for (int fi : toRemove)
        {
            remaining.erase(fi);
        }
        return patch;
    };

    PatchEntry leftPatch   = classifyPatch("left_x",   leftBox);
    PatchEntry rightPatch  = classifyPatch("right_x",  rightBox);
    PatchEntry frontPatch  = classifyPatch("front_y",  frontBox);
    PatchEntry backPatch   = classifyPatch("back_y",   backBox);
    PatchEntry bottomPatch = classifyPatch("bottom_z", bottomBox);
    PatchEntry topPatch    = classifyPatch("top_z",    topBox);

    // Remaining faces become "walls"
    PatchEntry wallsPatch;
    wallsPatch.name = "walls";
    wallsPatch.faceIndices.assign(remaining.begin(), remaining.end());

    // --- Step 5c: Rename patches based on flow direction ---
    // Following the Python convention:
    //   Flow X: left_x -> inlet side, right_x -> outlet side
    //   Flow Y: front_y -> inlet side, back_y -> outlet side
    //   Flow Z: bottom_z -> inlet side, top_z -> outlet side
    //
    // The "inlet side" patch is renamed to the flow-low-side name,
    // the "outlet side" to the flow-high-side name.
    // Non-flow-direction boundary patches keep their positional names.
    //
    // In the Python write_data.py the naming used is simply:
    //   inlet/outlet for the flow ends, and walls for no-slip.
    // We store flow-end patches with their positional names so the
    // downstream writer can map them as desired.

    // Build the ordered list of patches (determines face ordering in the file)
    std::vector<PatchEntry> orderedPatches;

    // Determine which patches correspond to inlet/outlet based on flow direction
    // and label them accordingly. The Python code uses:
    //   left_x/right_x for X flow
    //   front_y/back_y for Y flow
    //   bottom_z/top_z for Z flow
    switch (opts_.flowDirection)
    {
    case FlowDirection::X:
        leftPatch.name = "inlet";
        rightPatch.name = "outlet";
        orderedPatches.push_back(std::move(leftPatch));
        orderedPatches.push_back(std::move(rightPatch));
        orderedPatches.push_back(std::move(frontPatch));
        orderedPatches.push_back(std::move(backPatch));
        orderedPatches.push_back(std::move(bottomPatch));
        orderedPatches.push_back(std::move(topPatch));
        break;
    case FlowDirection::Y:
        frontPatch.name = "inlet";
        backPatch.name = "outlet";
        orderedPatches.push_back(std::move(leftPatch));
        orderedPatches.push_back(std::move(rightPatch));
        orderedPatches.push_back(std::move(frontPatch));
        orderedPatches.push_back(std::move(backPatch));
        orderedPatches.push_back(std::move(bottomPatch));
        orderedPatches.push_back(std::move(topPatch));
        break;
    case FlowDirection::Z:
        bottomPatch.name = "inlet";
        topPatch.name = "outlet";
        orderedPatches.push_back(std::move(leftPatch));
        orderedPatches.push_back(std::move(rightPatch));
        orderedPatches.push_back(std::move(frontPatch));
        orderedPatches.push_back(std::move(backPatch));
        orderedPatches.push_back(std::move(bottomPatch));
        orderedPatches.push_back(std::move(topPatch));
        break;
    }
    orderedPatches.push_back(std::move(wallsPatch));

    // --- Step 5d: Reorder faces: internal first, then boundary grouped by patch ---
    // Sort each patch's boundary faces by owner cell index (matching Python
    // reorder_boundary_patches which sorts by minimum owner).
    for (auto& patch : orderedPatches)
    {
        std::sort(patch.faceIndices.begin(), patch.faceIndices.end(),
                  [this](int a, int b)
                  {
                      return mesh_.owner[a] < mesh_.owner[b];
                  });
    }

    // Build new face/owner/neighbour arrays
    std::vector<FaceVertices> newFaces;
    std::vector<int> newOwner;
    std::vector<int> newNeighbour;

    newFaces.reserve(mesh_.faces.size());
    newOwner.reserve(mesh_.owner.size());
    newNeighbour.reserve(mesh_.neighbour.size());

    // Internal faces first (already in newFaces from indices 0..nInternalFaces-1)
    for (int i = 0; i < nInt; ++i)
    {
        newFaces.push_back(mesh_.faces[i]);
        newOwner.push_back(mesh_.owner[i]);
        newNeighbour.push_back(mesh_.neighbour[i]);
    }

    // Boundary faces grouped by patch
    mesh_.boundaryPatches.clear();
    for (const auto& patch : orderedPatches)
    {
        if (patch.faceIndices.empty())
            continue;

        int startFace = static_cast<int>(newFaces.size());
        int nFaces = static_cast<int>(patch.faceIndices.size());

        for (int fi : patch.faceIndices)
        {
            newFaces.push_back(mesh_.faces[fi]);
            newOwner.push_back(mesh_.owner[fi]);
        }

        mesh_.boundaryPatches[patch.name] = {startFace, nFaces};

        Logger::info("  Patch '" + patch.name + "': startFace="
                     + std::to_string(startFace) + " nFaces="
                     + std::to_string(nFaces));
    }

    // Replace mesh data
    mesh_.faces = std::move(newFaces);
    mesh_.owner = std::move(newOwner);
    mesh_.neighbour = std::move(newNeighbour);

    Logger::info("Boundary patches classified ("
                 + std::to_string(mesh_.boundaryPatches.size()) + " patches)");
}

} // namespace fiberfoam
