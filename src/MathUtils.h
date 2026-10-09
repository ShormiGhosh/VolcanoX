// MathUtils.h
//
// VolcanoX ships its own tiny vector/matrix library instead of GLM.
// Reason: this lab machine has GLFW and GLEW installed system-wide but no
// GLM and no package manager to fetch one, so pulling GLM in would mean
// downloading and vendoring an external library just to do 4x4 matrix
// multiplication. Writing the ~10 functions we actually need is a few dozen
// lines and has the side benefit of making every transformation in the
// scene (translate/rotate/scale, the view matrix, the projection matrix)
// something that can be pointed at and explained line-by-line in the lab
// demo, which is exactly what this stage of the project is graded on.
//
// Storage convention: Mat4 is column-major float[16], matching OpenGL's
// expectation for glUniformMatrix4fv(..., GL_FALSE, ...). Index m[col*4+row].
#pragma once
#include <cmath>

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

inline Vec3 cross3(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}
inline float dot3(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length3(const Vec3& a) { return std::sqrt(dot3(a, a)); }
inline Vec3 normalize3(const Vec3& a) {
    float len = length3(a);
    return (len > 1e-8f) ? a * (1.0f / len) : a;
}
inline float radians(float degrees) { return degrees * 3.14159265358979323846f / 180.0f; }

struct Mat4 {
    float m[16];

    Mat4() { for (int i = 0; i < 16; ++i) m[i] = 0.0f; }

    static Mat4 identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4 translate(const Vec3& t) {
        Mat4 r = identity();
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r = identity();
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }
    static Mat4 scale(float s) { return scale(Vec3(s, s, s)); }

    // Rotation about an arbitrary (assumed normalized) axis, Rodrigues' formula.
    static Mat4 rotate(const Vec3& axisIn, float angleRad) {
        Vec3 axis = normalize3(axisIn);
        float c = std::cos(angleRad), s = std::sin(angleRad), t = 1.0f - c;
        float x = axis.x, y = axis.y, z = axis.z;
        Mat4 r = identity();
        r.m[0] = t * x * x + c;     r.m[4] = t * x * y - s * z; r.m[8]  = t * x * z + s * y;
        r.m[1] = t * x * y + s * z; r.m[5] = t * y * y + c;     r.m[9]  = t * y * z - s * x;
        r.m[2] = t * x * z - s * y; r.m[6] = t * y * z + s * x; r.m[10] = t * z * z + c;
        return r;
    }
    static Mat4 rotateX(float a) { return rotate(Vec3(1, 0, 0), a); }
    static Mat4 rotateY(float a) { return rotate(Vec3(0, 1, 0), a); }
    static Mat4 rotateZ(float a) { return rotate(Vec3(0, 0, 1), a); }

    static Mat4 perspective(float fovyRad, float aspect, float zNear, float zFar) {
        Mat4 r;
        float tanHalf = std::tan(fovyRad * 0.5f);
        r.m[0]  = 1.0f / (aspect * tanHalf);
        r.m[5]  = 1.0f / tanHalf;
        r.m[10] = -(zFar + zNear) / (zFar - zNear);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = normalize3(center - eye);
        Vec3 s = normalize3(cross3(f, up));
        Vec3 u = cross3(s, f);
        Mat4 r = identity();
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot3(s, eye);
        r.m[13] = -dot3(u, eye);
        r.m[14] = dot3(f, eye);
        return r;
    }

    // C = A * B  (apply B to a vector first, then A)
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += m[k * 4 + row] * o.m[col * 4 + k];
                r.m[col * 4 + row] = sum;
            }
        }
        return r;
    }

    const float* data() const { return m; }
};

// 3x3 matrix, column-major (m[col*3+row]), used for the normal matrix.
struct Mat3 {
    float m[9];
    Mat3() { for (int i = 0; i < 9; ++i) m[i] = 0.0f; }
    const float* data() const { return m; }
};

// Normal matrix = inverse-transpose of the model matrix's upper-left 3x3.
// A plain rotation of the model matrix is NOT enough here: the scene uses
// non-uniform scales (flattened hills, the ground box, stretched clouds), which
// would otherwise skew the normals. The inverse-transpose of a 3x3 M is
// cofactor(M) / det(M); we build the cofactor matrix directly.
inline Mat3 normalMatrix(const Mat4& a) {
    // Upper-left 3x3 of the column-major Mat4: element (row r, col c) = a.m[c*4 + r].
    auto e = [&](int r, int c) { return a.m[c * 4 + r]; };
    float c00 = e(1,1) * e(2,2) - e(1,2) * e(2,1);
    float c01 = e(1,2) * e(2,0) - e(1,0) * e(2,2);
    float c02 = e(1,0) * e(2,1) - e(1,1) * e(2,0);
    float c10 = e(0,2) * e(2,1) - e(0,1) * e(2,2);
    float c11 = e(0,0) * e(2,2) - e(0,2) * e(2,0);
    float c12 = e(0,1) * e(2,0) - e(0,0) * e(2,1);
    float c20 = e(0,1) * e(1,2) - e(0,2) * e(1,1);
    float c21 = e(0,2) * e(1,0) - e(0,0) * e(1,2);
    float c22 = e(0,0) * e(1,1) - e(0,1) * e(1,0);
    float det = e(0,0) * c00 + e(0,1) * c01 + e(0,2) * c02;
    float inv = (std::fabs(det) > 1e-12f) ? 1.0f / det : 1.0f;
    // cofactor(M)/det = inverse-transpose(M); store column-major: cofactor (r,c) -> m[c*3+r].
    Mat3 n;
    n.m[0] = c00 * inv; n.m[3] = c01 * inv; n.m[6] = c02 * inv;
    n.m[1] = c10 * inv; n.m[4] = c11 * inv; n.m[7] = c12 * inv;
    n.m[2] = c20 * inv; n.m[5] = c21 * inv; n.m[8] = c22 * inv;
    return n;
}

// Simple deterministic pseudo-random helper so "random-looking" placement
// (rock jitter, tree scatter) is reproducible every run instead of relying
// on stdlib rand() seeding.
inline float hashNoise(int seed) {
    unsigned int n = static_cast<unsigned int>(seed);
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return 1.0f - static_cast<float>(n & 0x7fffffffu) / 1073741824.0f; // [-1,1]
}
