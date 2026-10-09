#version 330 core
out vec4 FragColor;

in vec3  vWorldPos;
in vec3  vNormal;
in vec2  vUV;
in float vLocalY;
in vec3  vLightD;
in vec3  vLightS;

uniform int uShadingMode;    // 0 Flat, 1 Gouraud, 2 Phong

#include "lighting.glsl"
#include "raytrace.glsl"

// uColor is the object's base colour (the diffuse/ambient albedo).
// uSky.x > 0.5 switches to a vertical gradient (uColor at the horizon ->
// uColor2 at the zenith) used only by the sky dome.
uniform vec4  uColor;
uniform vec4  uColor2;
uniform vec4  uSky;
uniform float uEmit;         // emission strength: colour * uEmit is added (lava, crater glow)

// Procedural textures (generated in code; see Texture.cpp). The texel modulates only the
// albedo (diffuse/ambient), never the specular term or the emission.
uniform sampler2D uTexRock;
uniform sampler2D uTexAsh;
uniform sampler2D uTexWood;
uniform int  uUseTex;         // global switch (key T)
uniform int  uTexId;          // 0 none, 1 rock, 2 ash, 3 wood (per draw)
uniform vec4 uTexScale;       // uv repeat (xy) per draw

// Inverse ray tracing: irradiance deposited by photons fired from the lava (PhotonMap.h).
uniform sampler2D uLightMap;
uniform int   uPhotonOn;
uniform float uPhotonGain;
uniform vec2  uMapOrigin;       // world (x, z) of the map's corner
uniform float uMapSize;         // world size of the map (square)

void main() {
    if (uSky.x > 0.5) {
        float k = smoothstep(0.0, 0.75, vLocalY);
        FragColor = vec4(mix(uColor.rgb, uColor2.rgb, k), 1.0);
        return;
    }
    if (uMat == MAT_UNLIT) {
        FragColor = uColor;
        return;
    }

    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 N;
    if (uShadingMode == 0) {
        // Flat: one normal per triangle, from the screen-space derivatives of the
        // world position. It always faces the viewer, whatever the winding.
        N = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
    } else {
        N = normalize(vNormal);
    }

    // Ray-traced shadows: one shadow ray per light, from just above the surface.
    float shadowCrater = 1.0, shadowSky = 1.0;
    bool rtActive = (uRTMode >= 1) && length(vWorldPos.xz) < 400.0;
    if (rtActive) {
        vec3 o = vWorldPos + N * 0.03;
        bool onVolcano = length(vWorldPos.xz) < uVolcR + 1.5;   // receivers on the cone skip the cone as occluder
        if (dot(N, uSkyL) > 0.0 && uLightMode != 1)
            shadowSky = rtOccluded(o, uSkyL, 1.0e4, false, onVolcano) ? 0.0 : 1.0;
        vec3 toC = uCraterPos - vWorldPos;
        float dC = length(toC);
        if (dot(N, toC) > 0.0 && uLightMode != 2)
            shadowCrater = rtOccluded(o, toC / dC, dC, true, true) ? 0.0 : 1.0;
    }

    vec3 lightD, lightS;
    if (uShadingMode == 1 && uRTMode == 0) {
        // Gouraud: the intensities were computed per vertex and interpolated.
        lightD = vLightD;
        lightS = vLightS;
    } else {
        // Flat and Phong (and Gouraud while ray tracing is on): lit per fragment.
        computeLighting(vWorldPos, N, V, shadowCrater, shadowSky, lightD, lightS);
    }

    // Inverse ray tracing: add the lava's glow from the light map as extra diffuse light.
    if (uPhotonOn == 1 && uTermMode >= 1) {
        vec2 muv = (vWorldPos.xz - uMapOrigin) / uMapSize;
        if (muv.x > 0.0 && muv.y > 0.0 && muv.x < 1.0 && muv.y < 1.0)
            lightD += texture(uLightMap, muv).rgb * uPhotonGain;
    }

    // Lava is self-lit: scale the exposure on its lit part down so the sun doesn't wash it out.
    // Its glossy highlight keeps most of the exposure so the n_s = 140 glint stays vivid.
    float expoD = (uMat == MAT_LAVA) ? 0.4 : uExposure;
    float expoS = (uMat == MAT_LAVA) ? 0.7 * uExposure : uExposure;
    vec3 albedo = uColor.rgb;
    if (uUseTex == 1 && uTexId != 0) {
        vec2 uv = vUV * uTexScale.xy;
        vec3 texel;
        if (uTexId == 1)      texel = texture(uTexRock, uv).rgb;
        else if (uTexId == 2) texel = texture(uTexAsh, uv).rgb;
        else                  texel = texture(uTexWood, uv).rgb;
        albedo *= texel * 2.0;     // textures have mean ~0.5, so the average albedo is unchanged
    }
    vec3 rgb = albedo * uEmit + expoD * albedo * lightD + expoS * lightS;

    // One reflection bounce on reflective surfaces (the lake, the lava).
    if (uRTMode >= 2 && uRefl != 0.0 && rtActive) {
        float F = uRefl;
        if (uRefl < 0.0) F = 0.02 + 0.98 * pow(1.0 - max(dot(N, V), 0.0), 5.0);   // Schlick, F0 = 0.02
        vec3 R = reflect(-V, N);
        vec3 refl = rtReflected(vWorldPos + N * 0.03, R);
        rgb = mix(rgb, refl, F);
    }
    FragColor = vec4(rgb, uColor.a);
}
