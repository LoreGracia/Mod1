#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aDensity;

uniform mat4  uViewProj;
uniform float uPointSize;
uniform float uDensityMin;
uniform float uDensityMax;

out float vDensity;

void main() {
    vDensity    = aDensity;
    gl_Position = uViewProj * vec4(aPos, 1.0);

    // Tamaño modulado por densidad (partículas más densas = más grandes)
    float t = clamp((aDensity - uDensityMin) /
                    max(uDensityMax - uDensityMin, 0.001), 0.0, 1.0);
    gl_PointSize = uPointSize * (0.7 + 0.6 * t);
}
