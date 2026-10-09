// raytrace.glsl -- camera-ray shadows and one reflection bounce, traced in the
// fragment shader against the small analytic proxy scene (RayScene.h).
// The intersection formulas match the C++ ones in RayScene.h and must agree.
//
// OpenGL 3.3 has no compute shaders and GLSL has no recursion, so this is a
// hybrid: the raster pipeline draws the picture and rays are cast per fragment
// against about a dozen closed-form shapes, to depth 1.

uniform int   uRTMode;          // 0 off, 1 shadows, 2 shadows + reflections
uniform float uRefl;            // surface reflectivity: > 0 constant, < 0 water (Schlick Fresnel), 0 none

uniform int   uNumCones;
uniform vec4  uConeA[8];        // xyz = base, w = r0
uniform vec4  uConeB[8];        // x = r1, y = height, z = tilt about Z, w = 1 if it shadows the crater light
uniform vec3  uConeCol[8];
uniform int   uNumSph;
uniform vec4  uSph[4];          // xyz = centre, w = radius
uniform int   uNumBox;
uniform vec4  uBoxC[3];         // xyz = centre, w = yaw
uniform vec3  uBoxH[3];         // half extents
uniform vec3  uBoxCol[3];
uniform vec3  uGroundCol;
uniform float uVolcR;           // volcano base radius (receivers on the cone skip the volcano occluder)

// Sky used for reflections (the same gradient as the sky dome, plus the sun and moon discs).
uniform vec3  uHorizon;
uniform vec3  uZenith;
uniform vec3  uSunDir;
uniform vec3  uMoonDir;
uniform float uSunVis;
uniform float uMoonVis;

float rtHitSphere(vec3 o, vec3 d, vec4 s) {
    vec3 oc = o - s.xyz;
    float b = dot(oc, d);
    float h = b * b - (dot(oc, oc) - s.w * s.w);
    if (h < 0.0) return -1.0;
    h = sqrt(h);
    float t = -b - h;
    if (t > 1e-4) return t;
    t = -b + h;
    return t > 1e-4 ? t : -1.0;
}

// Cone / cylinder about a vertical axis (tilted about Z by B.z): radius(y) = r0 + k y.
float rtHitCone(vec3 o, vec3 d, vec4 A, vec4 B, out vec3 nOut) {
    float r0 = A.w, r1 = B.x, H = B.y, tilt = B.z;
    vec3 p = o - A.xyz, dd = d;
    if (tilt != 0.0) {
        float cs = cos(-tilt), sn = sin(-tilt);
        p  = vec3(cs * p.x - sn * p.y, sn * p.x + cs * p.y, p.z);
        dd = vec3(cs * d.x - sn * d.y, sn * d.x + cs * d.y, d.z);
    }
    float k = (r1 - r0) / H;
    float q = r0 + k * p.y;
    float a = dd.x * dd.x + dd.z * dd.z - k * k * dd.y * dd.y;
    float b = p.x * dd.x + p.z * dd.z - k * q * dd.y;
    float c = p.x * p.x + p.z * p.z - q * q;
    float best = 1e30;
    vec3 nl = vec3(0.0, 1.0, 0.0);
    if (abs(a) > 1e-9) {
        float disc = b * b - a * c;
        if (disc >= 0.0) {
            float s = sqrt(disc);
            float t1 = (-b - s) / a, t2 = (-b + s) / a;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            for (int i = 0; i < 2; ++i) {
                float t = (i == 0) ? t1 : t2;
                if (t > 1e-4 && t < best) {
                    float y = p.y + t * dd.y;
                    if (y >= 0.0 && y <= H) {
                        best = t;
                        vec3 hp = p + dd * t;
                        nl = normalize(vec3(hp.x, -k * (r0 + k * hp.y), hp.z));
                    }
                }
            }
        }
    }
    if (abs(dd.y) > 1e-9) {
        float t = (H - p.y) / dd.y;                  // top cap
        if (t > 1e-4 && t < best) {
            vec2 xz = p.xz + t * dd.xz;
            if (dot(xz, xz) <= r1 * r1) { best = t; nl = vec3(0.0, 1.0, 0.0); }
        }
        t = -p.y / dd.y;                              // base cap
        if (t > 1e-4 && t < best) {
            vec2 xz = p.xz + t * dd.xz;
            if (dot(xz, xz) <= r0 * r0) { best = t; nl = vec3(0.0, -1.0, 0.0); }
        }
    }
    if (best >= 1e29) return -1.0;
    if (tilt != 0.0) {
        float cs = cos(tilt), sn = sin(tilt);
        nl = vec3(cs * nl.x - sn * nl.y, sn * nl.x + cs * nl.y, nl.z);
    }
    nOut = nl;
    return best;
}

// Oriented box i (rotated about Y by uBoxC[i].w): slab test in the box's local frame.
float rtHitBox(vec3 o, vec3 d, int idx, out vec3 nOut) {
    float yaw = uBoxC[idx].w;
    float cy = cos(-yaw), sy = sin(-yaw);
    vec3 p = o - uBoxC[idx].xyz;
    vec3 pl = vec3(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);
    vec3 dl = vec3(cy * d.x + sy * d.z, d.y, -sy * d.x + cy * d.z);
    float tn = -1e30, tf = 1e30;
    vec3 nl = vec3(0.0, 1.0, 0.0);
    for (int i = 0; i < 3; ++i) {
        float pv = pl[i], dv = dl[i], hv = uBoxH[idx][i];
        if (abs(dv) < 1e-9) {
            if (abs(pv) > hv) return -1.0;
            continue;
        }
        float t1 = (-hv - pv) / dv, t2 = (hv - pv) / dv;
        float sgn = -1.0;
        if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; sgn = 1.0; }
        if (t1 > tn) { tn = t1; nl = vec3(0.0); nl[i] = sgn; }
        tf = min(tf, t2);
    }
    if (tn > tf || tf < 1e-4) return -1.0;
    float t = tn > 1e-4 ? tn : tf;
    float cs = cos(yaw), sn = sin(yaw);
    nOut = vec3(cs * nl.x + sn * nl.z, nl.y, -sn * nl.x + cs * nl.z);
    return t;
}

// Is there an occluder between o and o + d * tmax? `crater` selects the occluders that
// shadow the crater light; `skipVolcano` is set for receivers lying on the volcano itself.
bool rtOccluded(vec3 o, vec3 d, float tmax, bool crater, bool skipVolcano) {
    vec3 n;
    for (int i = 0; i < uNumCones; ++i) {
        if (i == 0 && skipVolcano) continue;
        if (crater && uConeB[i].w < 0.5) continue;
        float t = rtHitCone(o, d, uConeA[i], uConeB[i], n);
        if (t > 0.0 && t < tmax) return true;
    }
    for (int i = 0; i < uNumSph; ++i) {
        float t = rtHitSphere(o, d, uSph[i]);
        if (t > 0.0 && t < tmax) return true;
    }
    for (int i = 0; i < uNumBox; ++i) {
        float t = rtHitBox(o, d, i, n);
        if (t > 0.0 && t < tmax) return true;
    }
    return false;
}

vec3 rtSky(vec3 R) {
    vec3 col = mix(uHorizon, uZenith, smoothstep(0.0, 0.75, R.y));
    float sunD = dot(R, uSunDir), moonD = dot(R, uMoonDir);
    col += vec3(1.0, 0.92, 0.7) * smoothstep(0.9975, 0.9990, sunD) * uSunVis;
    col += vec3(0.93, 0.94, 0.88) * smoothstep(0.9965, 0.9980, moonD) * uMoonVis;
    return col;
}

// One reflection bounce: the nearest proxy hit shaded cheaply (ambient + sky light, no shadow
// ray, no second bounce), or the analytic sky on a miss.
vec3 rtReflected(vec3 o, vec3 d) {
    float best = 1e30;
    vec3 bn = vec3(0.0, 1.0, 0.0), bc = uGroundCol, n;
    for (int i = 0; i < uNumCones; ++i) {
        float t = rtHitCone(o, d, uConeA[i], uConeB[i], n);
        if (t > 0.0 && t < best) { best = t; bn = n; bc = uConeCol[i]; }
    }
    for (int i = 0; i < uNumSph; ++i) {
        float t = rtHitSphere(o, d, uSph[i]);
        if (t > 0.0 && t < best) { best = t; bn = normalize(o + d * t - uSph[i].xyz); bc = vec3(0.8); }
    }
    for (int i = 0; i < uNumBox; ++i) {
        float t = rtHitBox(o, d, i, n);
        if (t > 0.0 && t < best) { best = t; bn = n; bc = uBoxCol[i]; }
    }
    if (d.y < -1e-6) {
        float t = -o.y / d.y;
        if (t > 1e-4 && t < best) { best = t; bn = vec3(0.0, 1.0, 0.0); bc = uGroundCol; }
    }
    if (best >= 1e29) return rtSky(d);
    return bc * (0.2 * uAmbient + uSkyDiffuse * max(dot(bn, uSkyL), 0.0)) * uExposure;
}
