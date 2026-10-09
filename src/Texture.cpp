#include "Texture.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>

// ===========================================================================
// Procedural textures
// ===========================================================================
namespace {

const int kTexSize = 256;

// One lattice value in [0,1], wrapped with `period` so the noise tiles seamlessly.
float latticeValue(int ix, int iy, int period, int seed) {
    ix = ((ix % period) + period) % period;
    iy = ((iy % period) + period) % period;
    return 0.5f + 0.5f * hashNoise(seed * 7919 + ix * 57 + iy * 131 + ix * iy * 17);
}

float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

// Tileable value noise: bilinear blend of smoothly-weighted lattice values.
float valueNoise(float x, float y, int period, int seed) {
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float fx = smooth(x - x0), fy = smooth(y - y0);
    float a = latticeValue(x0, y0, period, seed), b = latticeValue(x0 + 1, y0, period, seed);
    float c = latticeValue(x0, y0 + 1, period, seed), d = latticeValue(x0 + 1, y0 + 1, period, seed);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

// Fractal sum of tileable noise: u, v in [0,1) cover one tile.
float fbm(float u, float v, int baseFreq, int octaves, int seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    int freq = baseFreq;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * valueNoise(u * freq, v * freq, freq, seed + o * 31);
        norm += amp;
        amp *= 0.5f;
        freq *= 2;
    }
    return sum / norm;
}

float clamp01f(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

GLuint upload(const std::vector<unsigned char>& rgb) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, kTexSize, kTexSize, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return id;
}

void put(std::vector<unsigned char>& px, int x, int y, float r, float g, float b) {
    size_t i = ((size_t)y * kTexSize + x) * 3;
    px[i] = (unsigned char)(clamp01f(r) * 255.0f + 0.5f);
    px[i + 1] = (unsigned char)(clamp01f(g) * 255.0f + 0.5f);
    px[i + 2] = (unsigned char)(clamp01f(b) * 255.0f + 0.5f);
}

// Rock: multi-octave noise plus darker, ridged crack lines. Cool grey-brown.
GLuint makeRock() {
    std::vector<unsigned char> px((size_t)kTexSize * kTexSize * 3);
    for (int y = 0; y < kTexSize; ++y)
        for (int x = 0; x < kTexSize; ++x) {
            float u = (float)x / kTexSize, v = (float)y / kTexSize;
            float n = fbm(u, v, 4, 5, 11);
            float ridge = 1.0f - std::fabs(2.0f * fbm(u, v, 3, 4, 77) - 1.0f);   // thin bright ridges
            float cracks = std::pow(ridge, 6.0f);
            float val = 0.34f + 0.34f * n - 0.08f * cracks;
            put(px, x, y, val * 1.03f, val * 0.99f, val * 0.96f);
        }
    return upload(px);
}

// Ash: fine noise with darker blotches. Neutral grey.
GLuint makeAsh() {
    std::vector<unsigned char> px((size_t)kTexSize * kTexSize * 3);
    for (int y = 0; y < kTexSize; ++y)
        for (int x = 0; x < kTexSize; ++x) {
            float u = (float)x / kTexSize, v = (float)y / kTexSize;
            float fine = fbm(u, v, 16, 3, 5);
            float blotch = fbm(u, v, 8, 2, 23);   // no low frequencies, so the tiling is not obvious
            float val = 0.38f + 0.22f * fine + 0.14f * blotch;
            put(px, x, y, val, val * 0.98f, val * 0.95f);
        }
    return upload(px);
}

// Wood: stripes along u, bent by noise, with fine fibre noise. Warm brown.
GLuint makeWood() {
    std::vector<unsigned char> px((size_t)kTexSize * kTexSize * 3);
    for (int y = 0; y < kTexSize; ++y)
        for (int x = 0; x < kTexSize; ++x) {
            float u = (float)x / kTexSize, v = (float)y / kTexSize;
            float bend = fbm(u, v, 2, 3, 41);
            float rings = 0.5f + 0.5f * std::sin(6.2831853f * (u * 10.0f + 2.5f * bend));
            float fibre = fbm(u * 0.5f, v, 8, 3, 9);
            float val = 0.30f + 0.28f * rings + 0.22f * fibre;
            put(px, x, y, val * 1.08f, val * 0.97f, val * 0.84f);
        }
    return upload(px);
}

} // namespace

namespace Tex {

void Textures::build() {
    rock = makeRock();
    ash = makeAsh();
    wood = makeWood();
}

void Textures::bind() const {
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rock);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, ash);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, wood);
    glActiveTexture(GL_TEXTURE0);
}

} // namespace Tex

// ===========================================================================
// HUD: a 5x7 bitmap font drawn on screen-space quads
// ===========================================================================
namespace {

// Each glyph is 7 rows of 5 bits (the leftmost pixel is the highest bit).
struct Glyph { char c; uint8_t rows[7]; };
const Glyph kGlyphs[] = {
    {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}}, {'1', {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
    {'2', {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}}, {'3', {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}},
    {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}}, {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
    {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}}, {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}}, {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
    {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}}, {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}}, {'D', {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}},
    {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}}, {'F', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
    {'G', {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}}, {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I', {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}}, {'J', {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
    {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}}, {'L', {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
    {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}}, {'N', {0x11,0x11,0x19,0x15,0x13,0x11,0x11}},
    {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
    {'Q', {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}}, {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S', {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}}, {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'V', {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
    {'W', {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}}, {'X', {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
    {'Y', {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}}, {'Z', {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
    {':', {0x00,0x04,0x04,0x00,0x04,0x04,0x00}}, {'.', {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}},
    {',', {0x00,0x00,0x00,0x00,0x0C,0x04,0x08}}, {'-', {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}},
    {'+', {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}}, {'/', {0x01,0x02,0x02,0x04,0x08,0x08,0x10}},
    {'(', {0x02,0x04,0x08,0x08,0x08,0x04,0x02}}, {')', {0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
    {'[', {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}}, {']', {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}},
    {'%', {0x19,0x1A,0x02,0x04,0x08,0x0B,0x13}}, {'=', {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}},
    {'|', {0x04,0x04,0x04,0x04,0x04,0x04,0x04}}, {'*', {0x00,0x15,0x0E,0x1F,0x0E,0x15,0x00}},
    {'!', {0x04,0x04,0x04,0x04,0x04,0x00,0x04}}, {'?', {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}},
    {'_', {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}}, {'>', {0x10,0x08,0x04,0x02,0x04,0x08,0x10}},
    {'<', {0x01,0x02,0x04,0x08,0x04,0x02,0x01}},
};

// Atlas: 16 x 8 cells of 8x8 pixels (ASCII 0..127). Cell 127 is solid white (panels).
const int kAtlasW = 128, kAtlasH = 64, kCell = 8;

GLuint makeFontAtlas() {
    std::vector<unsigned char> px((size_t)kAtlasW * kAtlasH, 0);
    for (const Glyph& g : kGlyphs) {
        int cx = (g.c % 16) * kCell, cy = (g.c / 16) * kCell;
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (g.rows[row] & (1 << (4 - col))) px[(size_t)(cy + row) * kAtlasW + cx + col] = 255;
    }
    int sx = (127 % 16) * kCell, sy = (127 / 16) * kCell;      // solid cell
    for (int y = 0; y < kCell; ++y)
        for (int x = 0; x < kCell; ++x) px[(size_t)(sy + y) * kAtlasW + sx + x] = 255;

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasW, kAtlasH, 0, GL_RED, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);   // crisp pixel font
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return id;
}

std::string readTextFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "[Hud] Could not open " << path << std::endl; return ""; }
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

GLuint compileStage(GLenum type, const std::string& src, const char* name) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, sizeof(log), nullptr, log); std::cerr << "[Hud] " << name << ":\n" << log << std::endl; }
    return s;
}

} // namespace

bool Hud::init() {
    std::string vs = readTextFile("shaders/hud.vert"), fs = readTextFile("shaders/hud.frag");
    if (vs.empty() || fs.empty()) return false;
    GLuint v = compileStage(GL_VERTEX_SHADER, vs, "hud.vert"), f = compileStage(GL_FRAGMENT_SHADER, fs, "hud.frag");
    program_ = glCreateProgram();
    glAttachShader(program_, v); glAttachShader(program_, f);
    glLinkProgram(program_);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) { std::cerr << "[Hud] link failed" << std::endl; return false; }

    font_ = makeFontAtlas();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);                    // position (pixels)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));  // uv
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));  // colour
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return true;
}

void Hud::begin(int screenW, int screenH) {
    w_ = screenW; h_ = screenH;
    verts_.clear();
}

void Hud::quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
               float r, float g, float b, float a) {
    float vtx[6][8] = {
        { x,     y,     u0, v0, r, g, b, a }, { x + w, y,     u1, v0, r, g, b, a }, { x + w, y + h, u1, v1, r, g, b, a },
        { x,     y,     u0, v0, r, g, b, a }, { x + w, y + h, u1, v1, r, g, b, a }, { x,     y + h, u0, v1, r, g, b, a },
    };
    for (auto& vv : vtx) verts_.insert(verts_.end(), vv, vv + 8);
}

void Hud::panel(float x, float y, float w, float h, float r, float g, float b, float a) {
    float cu = ((127 % 16) * kCell + 4.0f) / kAtlasW, cv = ((127 / 16) * kCell + 4.0f) / kAtlasH;   // inside the solid cell
    quad(x, y, w, h, cu, cv, cu, cv, r, g, b, a);
}

float Hud::textWidth(const std::string& s, float px) {
    return s.size() * (px * 6.0f / 7.0f * 1.0f);   // 5 px glyph + 1 px gap per 7 px of height
}

void Hud::text(float x, float y, const std::string& s, float px, float r, float g, float b, float a) {
    float cell = px / 7.0f;                        // one font pixel
    float advance = 6.0f * cell;
    for (char ch : s) {
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        if (ch != ' ') {
            float u0 = ((ch % 16) * kCell) / (float)kAtlasW, v0 = ((ch / 16) * kCell) / (float)kAtlasH;
            quad(x, y, 8.0f * cell, 8.0f * cell, u0, v0, u0 + kCell / (float)kAtlasW, v0 + kCell / (float)kAtlasH, r, g, b, a);
        }
        x += advance;
    }
}

void Hud::image(GLuint tex, float x, float y, float w, float h, float gain) {
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "uScreen"), (float)w_, (float)h_);
    glUniform1i(glGetUniformLocation(program_, "uFont"), 3);
    glUniform1i(glGetUniformLocation(program_, "uImage"), 5);
    glUniform1i(glGetUniformLocation(program_, "uMode"), 1);
    glUniform1f(glGetUniformLocation(program_, "uGain"), gain);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, tex);
    float q[6][8] = {
        { x,     y,     0, 0, 1, 1, 1, 1 }, { x + w, y,     1, 0, 1, 1, 1, 1 }, { x + w, y + h, 1, 1, 1, 1, 1, 1 },
        { x,     y,     0, 0, 1, 1, 1, 1 }, { x + w, y + h, 1, 1, 1, 1, 1, 1 }, { x,     y + h, 0, 1, 1, 1, 1, 1 },
    };
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUniform1i(glGetUniformLocation(program_, "uMode"), 0);
    glActiveTexture(GL_TEXTURE0);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (cull) glEnable(GL_CULL_FACE);
}

void Hud::flush() {
    if (verts_.empty()) return;
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "uScreen"), (float)w_, (float)h_);
    glUniform1i(glGetUniformLocation(program_, "uFont"), 3);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, font_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts_.size() * sizeof(float), verts_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts_.size() / 8));
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);

    if (depth) glEnable(GL_DEPTH_TEST);
    if (cull) glEnable(GL_CULL_FACE);
}
