#include "Geometry.h"
#include <cmath>
#include <map>
#include <tuple>
#include <utility>

namespace Geometry {

static const float kPi = 3.14159265358979323846f;

// One vertex before normals are computed. `ref` is a rough "outward" direction
// used only to orient each triangle's normal (the winding of the original
// generators is not consistent), and `group` is the smoothing group: normals
// are averaged only between vertices of the same group at the same position.
struct GV {
    Vec3 p;
    Vec3 ref;
    float u = 0.0f, v = 0.0f;
    int group = 0;
};

static GV gv(const Vec3& p, const Vec3& ref, float u, float v, int group) {
    GV g; g.p = p; g.ref = ref; g.u = u; g.v = v; g.group = group;
    return g;
}

// Turn a list of triangles (3 consecutive GV per triangle) into the
// interleaved vertex list, averaging normals within smoothing groups.
// Triangles whose winding disagrees with the outward reference are flipped, so every
// mesh ends up counter-clockwise when seen from outside and back-face culling works.
static VertexList finalizeTriangles(std::vector<GV> tris) {
    const size_t triCount = tris.size() / 3;
    std::vector<Vec3> faceN(triCount);
    using Key = std::tuple<int, int, int, int>;
    std::map<Key, Vec3> sums;

    auto keyOf = [](const GV& g) {
        return Key(g.group, (int)std::lround(g.p.x * 10000.0f), (int)std::lround(g.p.y * 10000.0f),
                   (int)std::lround(g.p.z * 10000.0f));
    };

    for (size_t t = 0; t < triCount; ++t) {
        const GV& a = tris[t * 3];
        const GV& b = tris[t * 3 + 1];
        const GV& c = tris[t * 3 + 2];
        Vec3 n = cross3(b.p - a.p, c.p - a.p);
        float len = length3(n);
        if (len < 1e-9f) { faceN[t] = Vec3(0, 0, 0); continue; } // degenerate (a pole): contributes nothing
        n = n * (1.0f / len);
        Vec3 ref = a.ref + b.ref + c.ref;
        if (dot3(n, ref) < 0.0f) {             // orient outward: flip the normal and the winding
            n = -n;
            std::swap(tris[t * 3 + 1], tris[t * 3 + 2]);
        }
        faceN[t] = n;
        // N_v = (sum N_i) / ||sum N_i||: accumulate face normals per shared vertex.
        for (int k = 0; k < 3; ++k) sums[keyOf(tris[t * 3 + k])] += n;
    }

    VertexList out;
    out.reserve(tris.size() * kVertexFloats);
    for (size_t t = 0; t < triCount; ++t) {
        for (int k = 0; k < 3; ++k) {
            const GV& g = tris[t * 3 + k];
            Vec3 n = normalize3(sums[keyOf(g)]);
            if (length3(n) < 0.5f) n = normalize3(g.ref); // degenerate fallback
            out.push_back(g.p.x); out.push_back(g.p.y); out.push_back(g.p.z);
            out.push_back(n.x);   out.push_back(n.y);   out.push_back(n.z);
            out.push_back(g.u);   out.push_back(g.v);
        }
    }
    return out;
}

// Positions-only list (lines): pad normals and UVs with zeros.
static void pushLineVertex(VertexList& v, const Vec3& p) {
    v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
    for (int i = 0; i < 5; ++i) v.push_back(0.0f);
}

// ---------------------------------------------------------------- cone
VertexList cone(float baseRadius, float topRadius, float height, int segments, bool topCap) {
    std::vector<GV> t;
    if (segments < 3) segments = 3;
    const int kSide = 0, kBase = 1, kTop = 2;
    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * kPi * i / segments;
        float a1 = 2.0f * kPi * (i + 1) / segments;
        float u0 = float(i) / segments, u1 = float(i + 1) / segments;

        Vec3 b0(baseRadius * std::cos(a0), 0.0f, baseRadius * std::sin(a0));
        Vec3 b1(baseRadius * std::cos(a1), 0.0f, baseRadius * std::sin(a1));
        Vec3 t0(topRadius * std::cos(a0), height, topRadius * std::sin(a0));
        Vec3 t1(topRadius * std::cos(a1), height, topRadius * std::sin(a1));
        Vec3 r0(std::cos(a0), 0.0f, std::sin(a0)), r1(std::cos(a1), 0.0f, std::sin(a1));

        // Side wall: two triangles per segment (quad b0,b1,t1,t0).
        t.push_back(gv(b0, r0, u0, 0, kSide)); t.push_back(gv(b1, r1, u1, 0, kSide)); t.push_back(gv(t1, r1, u1, 1, kSide));
        t.push_back(gv(b0, r0, u0, 0, kSide)); t.push_back(gv(t1, r1, u1, 1, kSide)); t.push_back(gv(t0, r0, u0, 1, kSide));

        // Base cap (skip if baseRadius is ~0).
        if (baseRadius > 1e-5f) {
            Vec3 down(0, -1, 0);
            t.push_back(gv(Vec3(0, 0, 0), down, 0.5f, 0.5f, kBase));
            t.push_back(gv(b1, down, 0.5f + 0.5f * std::cos(a1), 0.5f + 0.5f * std::sin(a1), kBase));
            t.push_back(gv(b0, down, 0.5f + 0.5f * std::cos(a0), 0.5f + 0.5f * std::sin(a0), kBase));
        }
        // Top cap (skip if topRadius is ~0, i.e. a sharp point).
        if (topCap && topRadius > 1e-5f) {
            Vec3 up(0, 1, 0);
            t.push_back(gv(Vec3(0, height, 0), up, 0.5f, 0.5f, kTop));
            t.push_back(gv(t0, up, 0.5f + 0.5f * std::cos(a0), 0.5f + 0.5f * std::sin(a0), kTop));
            t.push_back(gv(t1, up, 0.5f + 0.5f * std::cos(a1), 0.5f + 0.5f * std::sin(a1), kTop));
        }
    }
    return finalizeTriangles(t);
}

// ---------------------------------------------------------------- crater bowl
VertexList craterBowl(float rimRadius, float floorRadius, float depth, int segments) {
    std::vector<GV> t;
    if (segments < 3) segments = 3;
    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * kPi * i / segments, a1 = 2.0f * kPi * (i + 1) / segments;
        float u0 = float(i) / segments, u1 = float(i + 1) / segments;
        Vec3 f0(floorRadius * std::cos(a0), 0.0f, floorRadius * std::sin(a0));
        Vec3 f1(floorRadius * std::cos(a1), 0.0f, floorRadius * std::sin(a1));
        Vec3 r0(rimRadius * std::cos(a0), depth, rimRadius * std::sin(a0));
        Vec3 r1(rimRadius * std::cos(a1), depth, rimRadius * std::sin(a1));
        // The reference direction is INWARD, so the finished normals face the crater's axis.
        Vec3 in0(-std::cos(a0), 0.0f, -std::sin(a0)), in1(-std::cos(a1), 0.0f, -std::sin(a1));
        t.push_back(gv(f0, in0, u0, 0, 0)); t.push_back(gv(f1, in1, u1, 0, 0)); t.push_back(gv(r1, in1, u1, 1, 0));
        t.push_back(gv(f0, in0, u0, 0, 0)); t.push_back(gv(r1, in1, u1, 1, 0)); t.push_back(gv(r0, in0, u0, 1, 0));
    }
    return finalizeTriangles(t);
}

// ---------------------------------------------------------------- sphere
static std::vector<GV> sphereTriangles(float radius, int latSegments, int lonSegments) {
    std::vector<GV> t;
    if (latSegments < 2) latSegments = 2;
    if (lonSegments < 3) lonSegments = 3;

    for (int i = 0; i < latSegments; ++i) {
        float theta0 = kPi * i / latSegments;       // 0..pi
        float theta1 = kPi * (i + 1) / latSegments;
        for (int j = 0; j < lonSegments; ++j) {
            float phi0 = 2.0f * kPi * j / lonSegments;
            float phi1 = 2.0f * kPi * (j + 1) / lonSegments;

            auto pt = [&](float theta, float phi, float u, float v) {
                Vec3 dir(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
                return gv(dir * radius, dir, u, v, 0);
            };

            float u0 = float(j) / lonSegments, u1 = float(j + 1) / lonSegments;
            float v0 = float(i) / latSegments, v1 = float(i + 1) / latSegments;
            GV p00 = pt(theta0, phi0, u0, v0);
            GV p01 = pt(theta0, phi1, u1, v0);
            GV p10 = pt(theta1, phi0, u0, v1);
            GV p11 = pt(theta1, phi1, u1, v1);

            // Interior rings are quads (two triangles). The top and bottom rings collapse to one
            // triangle each: tri 1 = (p00,p10,p11) is degenerate on the last row, where p10 = p11
            // is the south pole; tri 2 = (p00,p11,p01) is degenerate on the first row, where
            // p00 = p01 is the north pole.
            if (i != latSegments - 1) { t.push_back(p00); t.push_back(p10); t.push_back(p11); }
            if (i != 0) { t.push_back(p00); t.push_back(p11); t.push_back(p01); }
        }
    }
    return t;
}

VertexList sphere(float radius, int latSegments, int lonSegments) {
    return finalizeTriangles(sphereTriangles(radius, latSegments, lonSegments));
}

// ---------------------------------------------------------------- box
VertexList box(float sx, float sy, float sz) {
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    Vec3 c[8] = {
        Vec3(-hx, -hy, -hz), Vec3(hx, -hy, -hz), Vec3(hx, hy, -hz), Vec3(-hx, hy, -hz), // back face
        Vec3(-hx, -hy,  hz), Vec3(hx, -hy,  hz), Vec3(hx, hy,  hz), Vec3(-hx, hy,  hz), // front face
    };
    std::vector<GV> t;
    // Each face: its own group (so normals are flat), outward reference, and 0..1 UVs.
    auto face = [&](int a, int b, int cc, int d, const Vec3& outward, int group) {
        GV A = gv(c[a], outward, 0, 0, group), B = gv(c[b], outward, 1, 0, group);
        GV C = gv(c[cc], outward, 1, 1, group), D = gv(c[d], outward, 0, 1, group);
        t.push_back(A); t.push_back(B); t.push_back(C);
        t.push_back(A); t.push_back(C); t.push_back(D);
    };
    face(4, 5, 6, 7, Vec3(0, 0, 1), 0);   // front (+z)
    face(1, 0, 3, 2, Vec3(0, 0, -1), 1);  // back (-z)
    face(0, 4, 7, 3, Vec3(-1, 0, 0), 2);  // left (-x)
    face(5, 1, 2, 6, Vec3(1, 0, 0), 3);   // right (+x)
    face(7, 6, 2, 3, Vec3(0, 1, 0), 4);   // top (+y)
    face(0, 1, 5, 4, Vec3(0, -1, 0), 5);  // bottom (-y)
    return finalizeTriangles(t);
}

// ---------------------------------------------------------------- disc
VertexList disc(float radius, int segments) {
    std::vector<GV> t;
    if (segments < 3) segments = 3;
    Vec3 up(0, 1, 0);
    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * kPi * i / segments;
        float a1 = 2.0f * kPi * (i + 1) / segments;
        Vec3 p0(radius * std::cos(a0), 0.0f, radius * std::sin(a0));
        Vec3 p1(radius * std::cos(a1), 0.0f, radius * std::sin(a1));
        t.push_back(gv(Vec3(0, 0, 0), up, 0.5f, 0.5f, 0));
        t.push_back(gv(p0, up, 0.5f + 0.5f * std::cos(a0), 0.5f + 0.5f * std::sin(a0), 0));
        t.push_back(gv(p1, up, 0.5f + 0.5f * std::cos(a1), 0.5f + 0.5f * std::sin(a1), 0));
    }
    return finalizeTriangles(t);
}

// ---------------------------------------------------------------- ring
VertexList ring(float innerFraction, int segments) {
    std::vector<GV> t;
    if (segments < 3) segments = 3;
    Vec3 up(0, 1, 0);
    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * kPi * i / segments, a1 = 2.0f * kPi * (i + 1) / segments;
        Vec3 i0(innerFraction * std::cos(a0), 0, innerFraction * std::sin(a0)), i1(innerFraction * std::cos(a1), 0, innerFraction * std::sin(a1));
        Vec3 o0(std::cos(a0), 0, std::sin(a0)), o1(std::cos(a1), 0, std::sin(a1));
        float u0 = float(i) / segments, u1 = float(i + 1) / segments;
        t.push_back(gv(i0, up, u0, 0, 0)); t.push_back(gv(o0, up, u0, 1, 0)); t.push_back(gv(o1, up, u1, 1, 0));
        t.push_back(gv(i0, up, u0, 0, 0)); t.push_back(gv(o1, up, u1, 1, 0)); t.push_back(gv(i1, up, u1, 0, 0));
    }
    return finalizeTriangles(t);
}

// ---------------------------------------------------------------- rock
VertexList irregularRock(float radius, int seed, float jitterAmount) {
    // Start from a coarse sphere, then push each vertex outward/inward by a
    // deterministic hash of its DIRECTION, so vertices shared between
    // triangles move together and the surface stays closed.
    std::vector<GV> t = sphereTriangles(1.0f, 5, 7);
    for (GV& g : t) {
        Vec3 dir = g.p;
        int ix = (int)std::lround(dir.x * 500.0f), iy = (int)std::lround(dir.y * 500.0f), iz = (int)std::lround(dir.z * 500.0f);
        int key = seed * 10007 + ix * 73 + iy * 151 + iz * 311;
        float factor = 1.0f + hashNoise(key) * jitterAmount;
        g.p = dir * (radius * factor);
    }
    return finalizeTriangles(t);
}

// ---------------------------------------------------------------- lines
VertexList line(const Vec3& a, const Vec3& b) {
    VertexList v;
    pushLineVertex(v, a);
    pushLineVertex(v, b);
    return v;
}

VertexList lightningBolt(const Vec3& start, const Vec3& end, int seed, int iterations, float displace) {
    // Fractal midpoint displacement: begin with a single segment, and at
    // each iteration split every segment in half, kicking the new midpoint
    // sideways by a shrinking, hash-seeded random amount. This is the
    // standard way to build a convincing jagged bolt from pure math.
    std::vector<Vec3> pts = { start, end };
    float amount = displace;
    for (int it = 0; it < iterations; ++it) {
        std::vector<Vec3> next;
        next.reserve(pts.size() * 2);
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            Vec3 a = pts[i], b = pts[i + 1];
            Vec3 mid = (a + b) * 0.5f;

            Vec3 dir = normalize3(b - a);
            // Pick a sideways axis roughly perpendicular to the segment.
            Vec3 up(0, 1, 0);
            Vec3 perp = cross3(dir, up);
            if (length3(perp) < 1e-3f) perp = Vec3(1, 0, 0);
            perp = normalize3(perp);

            float n = hashNoise(seed * 7919 + (int)i * 131 + it * 97);
            mid = mid + perp * (n * amount);

            next.push_back(a);
            next.push_back(mid);
        }
        next.push_back(pts.back());
        pts = next;
        amount *= 0.5f;
    }

    VertexList v;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        pushLineVertex(v, pts[i]);
        pushLineVertex(v, pts[i + 1]);
    }
    return v;
}

} // namespace Geometry
