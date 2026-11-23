#version 330 core
uniform sampler2D albedo;
uniform sampler2D illumination;
uniform sampler2D emission;
out vec4 color;
void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec3 a = texelFetch(albedo, pixel, 0).rgb;
    vec3 l = texelFetch(illumination, pixel, 0).rgb;
    vec3 e = texelFetch(emission, pixel, 0).rgb;
    color = vec4(a * l + e, 1.0);
}
