#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::string Shader::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[Shader] Could not open " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string Shader::readWithIncludes(const std::string& path) {
    std::string src = readFile(path);
    if (src.empty()) return src;
    std::string dir;
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) dir = path.substr(0, slash + 1);

    std::stringstream in(src), out;
    std::string line;
    while (std::getline(in, line)) {
        size_t p = line.find("#include");
        if (p != std::string::npos) {
            size_t q1 = line.find('"', p), q2 = (q1 == std::string::npos) ? q1 : line.find('"', q1 + 1);
            if (q2 != std::string::npos) {
                out << readFile(dir + line.substr(q1 + 1, q2 - q1 - 1)) << '\n';
                continue;
            }
        }
        out << line << '\n';
    }
    return out.str();
}

GLuint Shader::compile(GLenum type, const std::string& src, const std::string& debugName) {
    GLuint shader = glCreateShader(type);
    const char* csrc = src.c_str();
    glShaderSource(shader, 1, &csrc, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "[Shader] Compile error in " << debugName << ":\n" << log << std::endl;
    }
    return shader;
}

bool Shader::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    std::string vsrc = readWithIncludes(vertPath);
    std::string fsrc = readWithIncludes(fragPath);
    if (vsrc.empty() || fsrc.empty()) return false;

    GLuint vs = compile(GL_VERTEX_SHADER, vsrc, vertPath);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc, fragPath);

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "[Shader] Link error:\n" << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return ok != 0;
}

void Shader::setMat4(const char* name, const Mat4& m) const {
    GLint loc = glGetUniformLocation(program, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, m.data());
}

void Shader::setMat3(const char* name, const Mat3& m) const {
    GLint loc = glGetUniformLocation(program, name);
    glUniformMatrix3fv(loc, 1, GL_FALSE, m.data());
}

void Shader::setVec4(const char* name, float x, float y, float z, float w) const {
    GLint loc = glGetUniformLocation(program, name);
    glUniform4f(loc, x, y, z, w);
}

void Shader::setVec3(const char* name, float x, float y, float z) const {
    GLint loc = glGetUniformLocation(program, name);
    glUniform3f(loc, x, y, z);
}

void Shader::setFloat(const char* name, float v) const {
    GLint loc = glGetUniformLocation(program, name);
    glUniform1f(loc, v);
}

void Shader::setInt(const char* name, int v) const {
    GLint loc = glGetUniformLocation(program, name);
    glUniform1i(loc, v);
}

void Shader::setVec2(const char* name, float x, float y) const {
    GLint loc = glGetUniformLocation(program, name);
    glUniform2f(loc, x, y);
}
