#version 330 core
uniform sampler2D source;
uniform vec2 axis;
in vec2 uv;
out vec4 color;
void main() {
    vec2 stride = axis / vec2(textureSize(source, 0));
    // Radius 8, sigma 3.2. Pair adjacent Gaussian taps using linear sampling.
    const float weight[5] = float[](0.12563068, 0.22298471, 0.13847292, 0.05872534, 0.01700169);
    const float tap[4] = float[](1.46344425, 3.41537306, 5.36886027, 7.32466349);
    vec3 sum = texture(source, uv).rgb * weight[0];
    for (int i = 1; i <= 4; ++i) {
        vec2 offset = stride * tap[i-1];
        sum += weight[i] * (texture(source, uv + offset).rgb + texture(source, uv - offset).rgb);
    }
    color = vec4(sum, 1.0);
}
