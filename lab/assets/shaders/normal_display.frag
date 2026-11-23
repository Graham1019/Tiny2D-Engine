#version 330 core
uniform sampler2D source;
in vec2 uv;
out vec4 color;
void main() {
    color = vec4(0.5 * (texture(source, uv).xyz + 1.0), 1.0);
}
