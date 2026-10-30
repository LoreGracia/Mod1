#version 330 core
in float vDensity;

uniform vec3  uColorLow;
uniform vec3  uColorHigh;
uniform float uDensityMin;
uniform float uDensityMax;
uniform float uAlpha;

out vec4 FragColor;

void main() {
    // Recorte circular
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(uv, uv);
    if (r2 > 1.0) discard;
    float edge = smoothstep(1.0, 0.85, r2);

    // Color por densidad
    float t = clamp((vDensity - uDensityMin) /
                    max(uDensityMax - uDensityMin, 0.001), 0.0, 1.0);
    vec3 color = mix(uColorLow, uColorHigh, t);

    FragColor = vec4(color, uAlpha * edge);
}
