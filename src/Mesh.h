// Mesh.h
//
// Thin wrapper around one VAO/VBO pair holding interleaved triangle or line
// data: position (location 0), normal (location 1), uv (location 2). Every scene object reuses one of a handful of Mesh instances
// (see Geometry.h) and only varies its transform + color at draw time --
// this keeps the "unique meshes" count low, per the project's optimization
// section (mesh reuse instead of one-off geometry per object).
#pragma once
#include <GL/glew.h>
#include "Geometry.h"

class Mesh {
public:
    Mesh() = default;
    ~Mesh() { destroy(); }

    // primitiveMode: GL_TRIANGLES, GL_LINES, or GL_LINE_STRIP.
    void upload(const VertexList& verts, GLenum primitiveMode) {
        destroy();
        mode = primitiveMode;
        vertexCount = static_cast<GLsizei>(verts.size() / kVertexFloats);

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        const GLsizei stride = kVertexFloats * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }

    // Number of draw calls issued since the counter was last reset (HUD / benchmark).
    static int& drawCalls() { static int n = 0; return n; }

    void draw() const {
        if (vao == 0) return;
        ++drawCalls();
        glBindVertexArray(vao);
        glDrawArrays(mode, 0, vertexCount);
        glBindVertexArray(0);
    }

    void destroy() {
        if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
        vertexCount = 0;
    }

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

private:
    GLuint vao = 0, vbo = 0;
    GLsizei vertexCount = 0;
    GLenum mode = GL_TRIANGLES;
};
