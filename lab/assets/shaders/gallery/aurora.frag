#version 330 core
uniform float uTime;
uniform vec2 uResolution;
layout(location = 0) out vec4 fragColor;

float hash(vec2 cell) {
    vec3 p = fract(vec3(cell.xyx) * vec3(.1031, .1030, .0973));
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

void main() {
    vec2 p = (gl_FragCoord.xy - .5 * uResolution) / uResolution.y;
    float t = uTime * .30;
    vec3 light = mix(vec3(.003, .008, .028), vec3(.015, .040, .077),
                     smoothstep(-.45, .45, p.y));

    // Sparse pinpricks behind the folds, anchored in exhibit space on resize.
    vec2 grid = p * 34.0;
    vec2 cell = floor(grid);
    vec2 star = .2 + .6 * vec2(hash(cell + 4.0), hash(cell + 11.0));
    float radius = length(fract(grid) - star);
    light += vec3(.52, .71, 1.0) * step(.83, hash(cell)) * exp(-135.0 * radius * radius);

    for (int i = 0; i < 3; ++i) {
        float layer = float(i);
        float wave = .15 * sin(4.1 * p.x + t + layer * .9)
                   + .051 * sin(9.0 * p.x - .7 * t + layer);
        float altitude = p.y - (wave - .19 + .095 * layer + .11 * p.x);
        float envelope = exp(-max(altitude, 0.0) * (11.0 + layer))
                       * smoothstep(-.022, .012, altitude);
        float rays = .5 + .5 * sin(93.0 * p.x + 8.0 * wave + .8 * t + 3.0 * layer);
        vec3 tint = mix(vec3(.03, 1.54, .87), vec3(.60, .21, 1.18), layer * .48);
        float lip = exp(-120.0 * abs(altitude)) * (.6 + .4 * rays);
        light += tint * (envelope * (.35 + .85 * rays * rays) + 2.7 * lip);
    }

    // A low silhouette gives the moving curtains a stationary horizon.
    float ridge = -.36 + .027 * sin(7.0 * p.x) + .012 * sin(19.0 * p.x + .7);
    float sky = smoothstep(ridge - fwidth(p.y), ridge + fwidth(p.y), p.y);
    fragColor = vec4(mix(vec3(.004, .008, .015), light, sky), 1.0);
}
