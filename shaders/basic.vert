#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMat;     // inverse-transpose of the model matrix's 3x3

uniform int uShadingMode;    // 0 Flat, 1 Gouraud, 2 Phong
uniform int uRTMode;         // ray tracing on (fragment-level lighting is then required)

#include "lighting.glsl"

out vec3  vWorldPos;
out vec3  vNormal;
out vec2  vUV;
out float vLocalY;           // model-space height, used for the sky-dome gradient
out vec3  vLightD;           // Gouraud only: lighting evaluated per vertex
out vec3  vLightS;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = normalize(uNormalMat * aNormal);
    vUV = aUV;
    vLocalY = aPos.y;

    vLightD = vec3(0.0);
    vLightS = vec3(0.0);
    if (uShadingMode == 1 && uMat != MAT_UNLIT && uRTMode == 0) {
        // Gouraud: run the whole illumination equation here, at the vertex,
        // and let the rasteriser interpolate the resulting intensities.
        vec3 V = normalize(uViewPos - world.xyz);
        computeLighting(world.xyz, vNormal, V, 1.0, 1.0, vLightD, vLightS);
    }

    gl_Position = uProjection * uView * world;
}
