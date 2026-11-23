#version 330 core
layout(location=0) in vec2 world;
uniform vec4 projection;
void main() {
    gl_Position = vec4(world * projection.xy + projection.zw, 0.0, 1.0);
}
