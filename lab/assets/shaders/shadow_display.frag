#version 330 core
uniform sampler2D source;
in vec2 uv;
out vec4 color;
void main() {
    color = vec4(vec3(texture(source, uv).r), 1.0);
}
