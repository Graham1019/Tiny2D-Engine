#version 330 core
// Camera2D's orthographic scale (xy) and translation (zw) map world to NDC.
uniform vec4 projection;
uniform vec2 framebufferSize;
uniform float ambient;
uniform int lightCount;
const int capacity = 32;
uniform vec4 centerInvRadius[capacity];
uniform vec4 radiance[capacity];
out vec4 color;
void main() {
    vec2 ndc = gl_FragCoord.xy * 2.0 / framebufferSize - 1.0;
    vec2 world = (ndc - projection.zw) / projection.xy;
    vec3 irradiance = vec3(ambient);
    for (int i = 0; i < capacity; ++i) {
        if (i >= lightCount) break;
        vec2 offset = (world - centerInvRadius[i].xy) * centerInvRadius[i].z;
        float falloff = max(0.0, 1.0 - dot(offset, offset));
        irradiance += radiance[i].rgb * (falloff * falloff);
    }
    color = vec4(irradiance, 1.0);
}
