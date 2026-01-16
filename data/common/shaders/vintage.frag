uniform sampler2D tColor;
uniform vec2 resolution;
uniform float time;

varying vec2 vUv;

float hash(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p.x + p.y) * 43758.5453);
}

void main() {
    vec2 uv = vUv;

    // Screen wobble.
    float wobble = sin(time * 1.3) * 0.018;
    uv += vec2(wobble, -wobble * 0.9);

    // Chunky pixelation: snap to a coarse grid for choppiness.
    vec2 grid = max(resolution * 0.12, vec2(4.0));
    uv = floor(uv * grid) / grid;

    // Random jumpy jitter.
    float n = hash(floor(uv * resolution * 0.8) + time * 2.3);
    uv += (n - 0.5) * 0.02;

    // Clamp to avoid sampling outside.
    uv = clamp(uv, 0.01, 0.99);

    vec2 texel = 1.0 / max(resolution, vec2(1.0));
    vec2 ca = vec2(3.6, -3.6) * texel;

    // Heavier smeared blur: more taps, wider offsets.
    vec3 accum = vec3(0.0);
    float wsum = 0.0;
    const vec2 taps[13] = vec2[13](
        vec2(0.0, 0.0), vec2(1.5, 0.8), vec2(-1.5, -0.8),
        vec2(2.8, -1.4), vec2(-2.8, 1.4), vec2(1.0, -2.6),
        vec2(-1.0, 2.6), vec2(3.2, 2.2), vec2(-3.2, -2.2),
        vec2(4.0, -0.5), vec2(-4.0, 0.5), vec2(0.5, 4.0), vec2(-0.5, -4.0)
    );
    for (int i = 0; i < 13; ++i) {
        vec2 o = taps[i] * texel * 4.5;
        vec3 c;
        c.r = texture2D(tColor, uv + ca + o).r;
        c.g = texture2D(tColor, uv + o).g;
        c.b = texture2D(tColor, uv - ca + o).b;
        accum += c;
        wsum += 1.0;
    }
    vec3 color = accum / max(wsum, 1.0);

    // Grain for motion.
    float grain = (hash(uv * resolution + time * 13.7) - 0.5) * 0.045;
    color += grain;

    gl_FragColor = vec4(color, 1.0);
}
