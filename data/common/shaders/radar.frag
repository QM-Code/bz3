uniform float playerY;
uniform float jumpHeight;   
varying vec3 vWorldPos;
const float MAX_DY = 10.0;
const float MAX_DY_DOWN = 5.0;
const float MAX_DOWN_ALPHA = 0.5;

void main() {
    float dy = vWorldPos.y - playerY;

    if (dy == 0.0) discard;

    float alpha = clamp(abs(dy) / MAX_DY, 0.0, 1.0);
    vec3 color;

    if (dy > 0.0) {
        if (dy < jumpHeight) {
            float t = clamp(dy / jumpHeight, 0.0, 1.0);
            color = vec3(1.0);
            alpha = t;
        } else {
            float t = clamp((dy - jumpHeight) / (MAX_DY - jumpHeight), 0.0, 1.0);
            color = mix(vec3(1.0), vec3(0.0, 0.0, 1.0), t);
            alpha = 1.0;
        }
    } else {
        color = vec3(0.0);
        alpha = clamp(clamp((abs(dy) / MAX_DY_DOWN), 0, 1) * MAX_DOWN_ALPHA, 0.0, 1.0);
    }

    gl_FragColor = vec4(color, alpha);
}