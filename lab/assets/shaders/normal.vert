#version 330 core
uniform vec4 projection;
uniform vec4 floorRect; // world origin and dimensions
out vec2 world;
void main() {
    vec2 corner = vec2(gl_VertexID & 1, gl_VertexID >> 1);
    world = floorRect.xy + corner * floorRect.zw;
    gl_Position = vec4(world * projection.xy + projection.zw, 0.0, 1.0);
}
