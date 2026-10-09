// Geometry.h
//
// Pure-math procedural mesh generators. Every function returns a flat list of
// INTERLEAVED vertices -- x, y, z, nx, ny, nz, u, v (8 floats per vertex) --
// built from closed-form parametric formulas. No files are read and no
// external mesh data is ever touched. This is the single place that defines
// what a "cone", "cylinder", "sphere", "box" etc. means for the whole project;
// every scene object is assembled by transforming and drawing copies of these
// primitives.
//
// Normals use the L9 slide-20 vertex-normal averaging
//     N_v = (sum of N_i) / || sum of N_i ||
// over the triangles that meet at a shared vertex, but only inside a
// "smoothing group" (so a cone's side wall is smooth while its cap edge stays
// sharp, and a box face is flat). UVs are the natural parametrisation of each
// shape (angle/height for cones, latitude/longitude for spheres, 0..1 per box
// face, planar for discs). The line meshes carry zero normals and UVs.
#pragma once
#include <vector>
#include "MathUtils.h"

using VertexList = std::vector<float>; // interleaved x,y,z, nx,ny,nz, u,v

constexpr int kVertexFloats = 8;

namespace Geometry {

// Cone / frustum: base radius at y=0, top radius at y=height (0 = a sharp
// point, matching the volcano cone and tree-trunk tapers).
// `topCap = false` leaves the top open (the volcano's crater mouth).
VertexList cone(float baseRadius, float topRadius, float height, int segments, bool topCap = true);

// The inside of the crater: an inverted frustum from the floor (y = 0, floorRadius) up to the
// rim (y = depth, rimRadius), with normals pointing inward, toward the axis. No floor: the
// crater glow disc is the floor.
VertexList craterBowl(float rimRadius, float floorRadius, float depth, int segments);

// Cylinder is a cone with equal top/base radius.
inline VertexList cylinder(float radius, float height, int segments) {
    return cone(radius, radius, height, segments);
}

// UV sphere centered at the origin.
VertexList sphere(float radius, int latSegments, int lonSegments);

// Axis-aligned box centered at the origin, full extents (sx,sy,sz). Each face
// has its own four vertices so its normal is flat (24 vertices' worth).
VertexList box(float sx, float sy, float sz);

// Flat disc in the XZ plane, centered at the origin, y=0. Normal is +Y.
VertexList disc(float radius, int segments);

// A flat ring in the XZ plane, centered at the origin: outer radius 1, inner radius `innerFraction`.
// Normal is +Y. Used for the shockwave and the water ripples.
VertexList ring(float innerFraction, int segments);

// A lumpy, non-uniform rock built by jittering a low-res sphere's vertices
// with a deterministic hash of each vertex's DIRECTION, so shared vertices
// move together (the mesh stays watertight and its normals can be averaged)
// but every rock still looks different and is reproducible frame to frame.
VertexList irregularRock(float radius, int seed, float jitterAmount = 0.35f);

// A single line segment (two points) -- draw with GL_LINES.
VertexList line(const Vec3& a, const Vec3& b);

// A jagged lightning bolt from `start` to `end`, built by recursively
// displacing the midpoint sideways (the standard fractal midpoint-displacement
// construction for lightning), returned as a GL_LINES vertex list.
VertexList lightningBolt(const Vec3& start, const Vec3& end, int seed, int iterations = 4, float displace = 0.9f);

} // namespace Geometry
