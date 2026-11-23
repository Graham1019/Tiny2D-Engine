#version 330 core
uniform sampler2D tile;
uniform float period;
uniform float strength;
in vec2 world;
out vec4 normal;
void main() {
    // R/G point along world +X/+Y. A zero slope encodes as byte 128 exactly.
    vec3 data = texture(tile, world / period).rgb;
    vec2 xy = (data.rg * 255.0 - 128.0) / 127.0;
    normal = vec4(normalize(vec3(xy * strength, data.b)), 1.0);
}
