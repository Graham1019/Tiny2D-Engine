#version 330 core
uniform sampler2D source;
uniform sampler2D bloom;
uniform float strength;
in vec2 uv;
out vec4 color;
void main() {
    vec3 hdr = texture(source, uv).rgb;
    if (strength > 0.0) hdr += strength * texture(bloom, uv).rgb;
    vec3 x = hdr / (1.0 + hdr);
    vec3 encoded = mix(1.055 * pow(x, vec3(1.0 / 2.4)) - 0.055, 12.92 * x,
                       lessThanEqual(x, vec3(0.0031308)));
    color = vec4(encoded, 1.0);
}
