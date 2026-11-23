#version 330 core
// Camera2D's orthographic scale (xy) and translation (zw) map world to NDC.
uniform vec4 projection;
uniform vec2 framebufferSize;
uniform float ambient;
uniform int lightCount;
const int capacity = 32;
uniform vec4 centerInvRadius[capacity];
uniform vec4 radiance[capacity];
#ifdef NORMAL_MAPPING
uniform sampler2D normalBuffer;
#endif
out vec4 color;
void main() {
    vec2 ndc = gl_FragCoord.xy * 2.0 / framebufferSize - 1.0;
    vec2 world = (ndc - projection.zw) / projection.xy;
    vec3 irradiance = vec3(ambient);
#ifdef NORMAL_MAPPING
    // RGBA16F rounding can slightly change the stored vector length.
    vec3 n = normalize(texelFetch(normalBuffer, ivec2(gl_FragCoord.xy), 0).xyz);
#endif
    for (int i = 0; i < capacity; ++i) {
        if (i >= lightCount) break;
        vec2 offset = (world - centerInvRadius[i].xy) * centerInvRadius[i].z;
        float falloff = max(0.0, 1.0 - dot(offset, offset));
#ifdef NORMAL_MAPPING
        // Only a point inside the validated radius can contribute or needs a direction.
        if (!(falloff > 0.0)) continue;
        vec3 toLight = vec3(centerInvRadius[i].xy - world, centerInvRadius[i].w);
        irradiance += radiance[i].rgb * (falloff * falloff) * clamp(dot(n, normalize(toLight)), 0.0, 1.0);
#else
        irradiance += radiance[i].rgb * (falloff * falloff);
#endif
    }
    color = vec4(irradiance, 1.0);
}
