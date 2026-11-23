#version 330 core
uniform float uTime;
uniform vec2 uResolution;
layout(location = 0) out vec4 fragColor;

mat2 rotation(float angle) {
    float s = sin(angle), c = cos(angle);
    return mat2(c, s, -s, c);
}

void main() {
    vec2 p = (gl_FragCoord.xy - .5 * uResolution) / uResolution.y;
    float pixel = 1.0 / uResolution.y;
    float r = length(p);
    vec3 light = vec3(.007, .010, .018) + vec3(.052, .022, .006) * exp(-4.0 * r);

    // Each inclined ellipse carries a moving body at the same parametric phase.
    for (int i = 0; i < 4; ++i) {
        float n = float(i);
        vec2 axes = vec2(.20 + .088 * n, .086 + .029 * n);
        vec2 q = rotation(.52 - .45 * n) * p;
        vec2 unit = q / axes;
        float implicitCurve = dot(unit, unit) - 1.0;
        float distanceToOrbit = abs(implicitCurve) / max(length(2.0 * unit / axes), .001);
        vec3 tint = mix(vec3(1.80, .52, .11), vec3(.05, .68, 1.34), n / 3.0);
        float line = 1.0 - smoothstep(.0008, .0015 + pixel, distanceToOrbit);
        light += tint * (.42 * line + .085 * exp(-130.0 * distanceToOrbit));

        float phase = uTime * (.33 + .10 * n) + 1.77 * n;
        vec2 body = axes * vec2(cos(phase), sin(phase));
        float d = length(q - body);
        float core = 1.0 - smoothstep(.007 + n * .001, .009 + n * .001 + pixel, d);
        light += tint * (2.9 * core + exp(-110.0 * d));
    }

    // The central disc and its corona emit linear energy above the bloom threshold.
    float disc = 1.0 - smoothstep(.043, .043 + 1.5 * pixel, r);
    float halo = exp(-32.0 * abs(r - .044));
    vec3 star = mix(vec3(3.9, .87, .17), vec3(7.0, 3.1, .70),
                    smoothstep(-.045, .03, -.55 * p.x + p.y));
    light += star * disc + vec3(1.8, .50, .08) * halo;
    fragColor = vec4(light, 1.0);
}
