#version 330 core
// HUD fragment shader: the font atlas is a one-channel coverage mask; panels sample
// the atlas's solid cell, so one shader and one draw call serve text and backgrounds.
// uMode 1 draws an RGB image instead (the light-map minimap).
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2D uFont;
uniform sampler2D uImage;
uniform int   uMode;
uniform float uGain;

void main() {
    if (uMode == 1) {
        // sqrt tone-maps the huge range between the crater and the far flows so both stay visible
        FragColor = vec4(sqrt(clamp(texture(uImage, vUV).rgb * uGain, 0.0, 1.0)), 1.0);
        return;
    }
    float cover = texture(uFont, vUV).r;
    FragColor = vec4(vColor.rgb, vColor.a * cover);
}
