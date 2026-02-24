#pragma once

#include "common/Types.h"
#include <array>

namespace fiberfoam
{

// The 6 hex face definitions (local vertex indices into sorted 8-vertex array)
// Vertices are sorted by (z, y, x), giving this local layout:
//   0: (-,-,-)  1: (+,-,-)  2: (+,-,+)  3: (-,-,+)
//   4: (-,+,-)  5: (+,+,-)  6: (+,+,+)  7: (-,+,+)
constexpr std::array<std::array<int, 4>, 6> HEX_FACE_DEFS = {{
    {1, 3, 7, 5}, // right  (+x)
    {2, 6, 7, 3}, // back   (+y)
    {4, 5, 7, 6}, // top    (+z)
    {0, 4, 6, 2}, // left   (-x)
    {0, 1, 5, 4}, // front  (-y)
    {0, 2, 3, 1}, // bottom (-z)
}};

} // namespace fiberfoam
