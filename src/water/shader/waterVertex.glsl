#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aDensity;
layout(location = 2) in vec3  aVel;   // Fase 4: velocidad para salpicaduras

uniform mat4  uViewProj;
uniform float uPointSize;
uniform float uDensityMin;
uniform float uDensityMax;
// Fase 3.7/4: las gotas por debajo de esta altura ya son charco y no
// deberían dibujarse como puntos... salvo que estén salpicando.
uniform float uHideBelowY;
uniform float uSplashSpeed; // |v| >= esto => sigue visible (salpicadura)

out float vDensity;

void main() {
    vDensity = aDensity;

    // Oculta solo si está dentro de la lámina Y casi en reposo. Si la
    // velocidad es alta (rebote/salpicadura) se mantiene visible.
    bool insidePool = aPos.y < uHideBelowY;
    bool atRest     = length(aVel) < uSplashSpeed;
    if (insidePool && atRest) {
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
