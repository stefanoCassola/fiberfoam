#include <gtest/gtest.h>
#include "mesh/FaceGenerator.h"

#include <set>

using namespace fiberfoam;

TEST(FaceGeneratorTest, HexFaceDefsHas6Faces)
{
    EXPECT_EQ(HEX_FACE_DEFS.size(), 6u);
}

TEST(FaceGeneratorTest, EachFaceHas4Vertices)
{
    for (const auto& face : HEX_FACE_DEFS)
    {
        // Each face definition is an array of 4 vertex indices
        EXPECT_EQ(face.size(), 4u);
    }
}

TEST(FaceGeneratorTest, VertexIndicesInRange)
{
    // All vertex indices should be in [0,7] for a hex with 8 vertices
    for (const auto& face : HEX_FACE_DEFS)
    {
        for (int vi : face)
        {
            EXPECT_GE(vi, 0);
            EXPECT_LE(vi, 7);
        }
    }
}

TEST(FaceGeneratorTest, AllVerticesUsed)
{
    // Every vertex (0-7) should appear in at least one face
    std::set<int> usedVertices;
    for (const auto& face : HEX_FACE_DEFS)
    {
        for (int vi : face)
        {
            usedVertices.insert(vi);
        }
    }

    for (int i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(usedVertices.count(i) > 0) << "Vertex " << i << " not used in any face";
    }
}

TEST(FaceGeneratorTest, EachVertexAppearsInExactly3Faces)
{
    // In a hexahedron, each vertex belongs to exactly 3 faces
    std::map<int, int> vertexFaceCount;
    for (const auto& face : HEX_FACE_DEFS)
    {
        for (int vi : face)
        {
            vertexFaceCount[vi]++;
        }
    }

    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(vertexFaceCount[i], 3)
            << "Vertex " << i << " appears in " << vertexFaceCount[i] << " faces, expected 3";
    }
}

TEST(FaceGeneratorTest, NoDuplicateVerticesPerFace)
{
    for (size_t f = 0; f < HEX_FACE_DEFS.size(); ++f)
    {
        std::set<int> verts(HEX_FACE_DEFS[f].begin(), HEX_FACE_DEFS[f].end());
        EXPECT_EQ(verts.size(), 4u)
            << "Face " << f << " has duplicate vertex indices";
    }
}

TEST(FaceGeneratorTest, OppositeFacesShareNoVertices)
{
    // Opposite face pairs: (0,3), (1,4), (2,5)
    // +x and -x; +y and -y; +z and -z
    std::array<std::pair<int, int>, 3> oppositePairs = {{{0, 3}, {1, 4}, {2, 5}}};

    for (const auto& [f1, f2] : oppositePairs)
    {
        std::set<int> verts1(HEX_FACE_DEFS[f1].begin(), HEX_FACE_DEFS[f1].end());
        std::set<int> verts2(HEX_FACE_DEFS[f2].begin(), HEX_FACE_DEFS[f2].end());

        std::vector<int> intersection;
        std::set_intersection(verts1.begin(), verts1.end(),
                              verts2.begin(), verts2.end(),
                              std::back_inserter(intersection));

        EXPECT_TRUE(intersection.empty())
            << "Opposite faces " << f1 << " and " << f2 << " share vertices";
    }
}

TEST(FaceGeneratorTest, AdjacentFacesShare2Vertices)
{
    // Non-opposite face pairs should share exactly 2 vertices (one edge)
    for (size_t i = 0; i < HEX_FACE_DEFS.size(); ++i)
    {
        for (size_t j = i + 1; j < HEX_FACE_DEFS.size(); ++j)
        {
            // Skip opposite pairs
            if ((i == 0 && j == 3) || (i == 1 && j == 4) || (i == 2 && j == 5))
                continue;

            std::set<int> v1(HEX_FACE_DEFS[i].begin(), HEX_FACE_DEFS[i].end());
            std::set<int> v2(HEX_FACE_DEFS[j].begin(), HEX_FACE_DEFS[j].end());

            std::vector<int> intersection;
            std::set_intersection(v1.begin(), v1.end(), v2.begin(), v2.end(),
                                  std::back_inserter(intersection));

            EXPECT_EQ(static_cast<int>(intersection.size()), 2)
                << "Adjacent faces " << i << " and " << j
                << " share " << intersection.size() << " vertices, expected 2";
        }
    }
}

TEST(FaceGeneratorTest, TotalVertexReferences)
{
    // 6 faces * 4 vertices = 24 total vertex references
    int total = 0;
    for (const auto& face : HEX_FACE_DEFS)
    {
        total += static_cast<int>(face.size());
    }
    EXPECT_EQ(total, 24);
}
