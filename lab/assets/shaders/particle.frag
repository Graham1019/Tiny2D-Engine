#version 330 core
in vec2 local;
flat in vec3 radiance;
out vec4 color;
void main() {
    float falloff = max(0.0, 1.0 - dot(local, local));
    color = vec4(radiance * (falloff * falloff), falloff);
}
