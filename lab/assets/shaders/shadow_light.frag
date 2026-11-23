#version 330 core
uniform vec4 projection;
uniform vec2 framebufferSize;
uniform vec4 centerInvRadius;
uniform vec4 radiance;
uniform sampler2D visibility;
#ifdef NORMAL_MAPPING
uniform sampler2D normalBuffer;
#endif
out vec4 color;
void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec2 ndc = gl_FragCoord.xy * 2.0 / framebufferSize - 1.0;
    vec2 world = (ndc - projection.zw) / projection.xy;
    vec2 offset = (world - centerInvRadius.xy) * centerInvRadius.z;
    float falloff = max(0.0, 1.0 - dot(offset, offset));
    if (!(falloff > 0.0)) {
        color = vec4(0.0);
        return;
    }
    float weight = falloff * falloff * texelFetch(visibility, pixel, 0).r;
#ifdef NORMAL_MAPPING
    vec3 toLight = vec3(centerInvRadius.xy - world, centerInvRadius.w);
    vec3 n = normalize(texelFetch(normalBuffer, pixel, 0).xyz);
    weight *= clamp(dot(n, normalize(toLight)), 0.0, 1.0);
#endif
    color = vec4(radiance.rgb * weight, 0.0);
}
