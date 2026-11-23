#version 330 core
uniform sampler2D source;
in vec2 uv;
out vec4 color;
void main() {
    vec3 hdr = texture(source, uv).rgb;
    float y = dot(hdr, vec3(0.2126, 0.7152, 0.0722));
    color = vec4(hdr * max(y - 1.0, 0.0) / max(y, 0.0001), 1.0);
}
