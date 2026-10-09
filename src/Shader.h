// Shader.h -- minimal GLSL program loader (compile + link + uniform setters).
#pragma once
#include <GL/glew.h>
#include <string>
#include "MathUtils.h"

class Shader {
public:
    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);
    void use() const { glUseProgram(program); }
    GLuint id() const { return program; }

    void setMat4(const char* name, const Mat4& m) const;
    void setVec4(const char* name, float x, float y, float z, float w) const;
    void setMat3(const char* name, const Mat3& m) const;
    void setVec2(const char* name, float x, float y) const;
    void setVec3(const char* name, float x, float y, float z) const;
    void setFloat(const char* name, float v) const;
    void setInt(const char* name, int v) const;

    ~Shader() { if (program) glDeleteProgram(program); }

private:
    GLuint program = 0;
    static std::string readFile(const std::string& path);
    // Reads a GLSL file and expands lines of the form  #include "file.glsl"  (relative to the
    // same directory), since GLSL 3.30 has no #include of its own.
    static std::string readWithIncludes(const std::string& path);
    static GLuint compile(GLenum type, const std::string& src, const std::string& debugName);
};
