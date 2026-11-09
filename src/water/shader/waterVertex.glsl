#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aDensity;

uniform mat4  uViewProj;
uniform float uPointSize;
uniform float uDensityMin;
uniform float uDensityMax;
// Fase 3.7: las gotas por debajo de esta altura ya son charco (o están
// dentro de la lámina) y no deben dibujarse como puntos.
uniform float uHideBelowY;

out float vDensity;

void main() {
    vDensity = aDensity;

    // Si la gota ya está en la lámina de agua, la mandamos fuera del clip
    // y con tamaño 0. Así desaparecen los puntos blancos dentro de la malla.
    if (aPos.y < uHideBelowY) {
        gl_Position  = vec4(2.0, 2.0, 2.0, 1.0); // fuera de NDC
        gl_PointSize = 0.0;
        return;
    }

    gl_Position = uViewProj * vec4(aPos, 1.0);

    // Tamaño modulado por densidad (partículas más densas = más grandes)
    float t = clamp((aDensity - uDensityMin) /
                    max(uDensityMax - uDensityMin, 0.001), 0.0, 1.0);
    gl_PointSize = uPointSize * (0.7 + 0.6 * t);
}
