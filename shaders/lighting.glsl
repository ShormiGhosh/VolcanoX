// lighting.glsl -- the L8 Phong illumination equation, shared by the vertex
// shader (Gouraud mode) and the fragment shader (Flat and Phong modes).
// Expanded into both shaders by Shader::readWithIncludes().
//
//   I = I_e                                              (emission: lava only; added by the caller)
//     + k_a * I_a_global                                 (ambient)
//     + sum over lights  f_att(d) * [ k_d * I_d * max(0, N.L)       (diffuse)
//                                   + k_s * I_s * max(0, R.V)^n_s ] (specular)
//   R = reflect(-L, N);   Blinn-Phong mode uses (N.H)^n_s with H = normalize(L + V)
//
// The result is returned as two vectors so a texture can modulate only the
// diffuse/ambient part:   colour = emission + albedo * texel * lightD + lightS
//   lightD = kaRatio * I_a + sum f * I_d * max(0, N.L)      (albedo factored out)
//   lightS = sum f * k_s * I_s * spec

// Material presets (selected per draw call with uMat).
const int MAT_MATTE = 0, MAT_ROCK = 1, MAT_LAVA = 2, MAT_ASH = 3, MAT_UNLIT = 4;

uniform int  uMat;
uniform vec3 uViewPos;

// Light 0: crater glow, a point light with radial attenuation 1/(a0 + a1 d + a2 d^2).
uniform vec3 uCraterPos;
uniform vec3 uCraterDiffuse;
uniform vec3 uCraterSpecular;
uniform vec3 uAtten;            // (a0, a1, a2)
uniform float uCraterBoost;     // 1 normally; rises during an eruption burst (the vent flares)

// Light 1: sky light, directional: the moon at night, the sun by day.
uniform vec3 uSkyL;             // unit vector from the surface toward the light
uniform vec3 uSkyDiffuse;
uniform vec3 uSkySpecular;
uniform vec3 uAmbient;          // global ambient I_a

uniform int  uLightMode;        // 0 both, 1 crater only, 2 sky light only
uniform int  uTermMode;         // 0 ambient only, 1 ambient+diffuse, 2 specular only, 3 full
uniform int  uBlinn;            // 0 Phong (R.V), 1 Blinn-Phong (N.H)
uniform float uExposure;        // camera exposure applied to the lit part

void materialParams(out float kaRatio, out vec3 ks, out float ns) {
    if (uMat == MAT_LAVA)      { kaRatio = 0.33; ks = vec3(0.95, 0.85, 0.60); ns = 140.0; }
    else if (uMat == MAT_ASH)  { kaRatio = 0.23; ks = vec3(0.05);             ns = 2.0;   }
    else                       { kaRatio = 0.20; ks = vec3(0.05);             ns = 4.0;   } // rock and default matte
}

void accumulateLight(vec3 N, vec3 V, vec3 L, vec3 Id, vec3 Is, float att, vec3 ks, float ns,
                     inout vec3 diffuse, inout vec3 specular) {
    float nl = max(dot(N, L), 0.0);
    diffuse += att * Id * nl;
    if (nl > 0.0) {
        float s;
        if (uBlinn == 1) {
            vec3 H = normalize(L + V);
            s = pow(max(dot(N, H), 0.0), ns);
        } else {
            vec3 R = reflect(-L, N);
            s = pow(max(dot(R, V), 0.0), ns);
        }
        specular += att * ks * Is * s;
    }
}

// shadowCrater / shadowSky are 1.0 when the light reaches the point and 0.0 when a ray-traced
// shadow ray is blocked (ambient is never shadowed).
void computeLighting(vec3 P, vec3 N, vec3 V, float shadowCrater, float shadowSky,
                     out vec3 lightD, out vec3 lightS) {
    float kaRatio; vec3 ks; float ns;
    materialParams(kaRatio, ks, ns);

    vec3 diffuse = vec3(0.0), specular = vec3(0.0);

    if (uLightMode != 2) {                       // crater point light, attenuated with distance
        vec3 toL = uCraterPos - P;
        float d = length(toL);
        float att = uCraterBoost * shadowCrater / (uAtten.x + uAtten.y * d + uAtten.z * d * d);
        accumulateLight(N, V, toL / max(d, 1e-4), uCraterDiffuse, uCraterSpecular, att, ks, ns, diffuse, specular);
    }
    if (uLightMode != 1) {                       // directional sky light, no attenuation
        accumulateLight(N, V, uSkyL, uSkyDiffuse, uSkySpecular, shadowSky, ks, ns, diffuse, specular);
    }

    vec3 ambient = kaRatio * uAmbient;
    if (uTermMode == 0)      { lightD = ambient;           lightS = vec3(0.0); }
    else if (uTermMode == 1) { lightD = ambient + diffuse; lightS = vec3(0.0); }
    else if (uTermMode == 2) { lightD = vec3(0.0);         lightS = specular;  }
    else                     { lightD = ambient + diffuse; lightS = specular;  }
}
