#version 330 core
layout(location=0) in vec2 corner;
layout(location=1) in vec4 positionSizeAge;
layout(location=2) in vec4 colorEmission;
uniform mat4 viewProjection;
out vec2 local;
flat out vec3 radiance;
void main() {
    local = corner;
    vec2 world = positionSizeAge.xy + corner * (0.5 * positionSizeAge.z);
    gl_Position = viewProjection * vec4(world, 0.0, 1.0);
    float fade = 1.0 - smoothstep(0.55, 1.0, positionSizeAge.w);
    radiance = colorEmission.rgb * (colorEmission.a * fade);
}
