#version 330 core
uniform float uTime;
uniform vec2 uResolution;
layout(location = 0) out vec4 fragColor;

float relief(vec2 p, float t) {
    vec2 warp = vec2(sin(2.3 * p.y + .4 * t), cos(2.9 * p.x - .3 * t));
    vec2 q = p + .23 * warp;
    return .48 * sin(4.1 * q.x + .5 * t)
         + .30 * cos(5.7 * q.y - .6 * t)
         + .17 * sin(7.0 * q.x + 3.0 * q.y + .4 * t);
}

// Convert height differences to screen-space distances for legible isolines.
float isoline(float level, float width) {
    float pixels = abs(fract(level + .5) - .5) / max(fwidth(level), .0001);
    return 1.0 - smoothstep(width, width + 1.0, pixels);
}

void main() {
    vec2 p = (gl_FragCoord.xy - .5 * uResolution) / uResolution.y;
    float elevation = relief(p, .34 * uTime);
    float level = 12.0 * elevation;
    float terrace = clamp(floor(level) / 24.0 + .5, 0.0, 1.0);
    vec3 valley = vec3(.015, .007, .032);
    vec3 upland = vec3(.31, .055, .085);
    vec3 light = mix(valley, upland, terrace);
    vec3 fineInk = mix(vec3(.33, .07, .30), vec3(1.1, .32, .11), terrace);
    light += fineInk * isoline(level, .45);
    light += vec3(2.50, 1.13, .27) * isoline(level * .25, 1.10);
    fragColor = vec4(light, 1.0);
}
