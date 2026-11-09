#version 430 core

in vec3 vWorldPos;
in vec3 vNormal;

uniform vec3  uCameraPos;
uniform vec3  uSunDir;         // dirección hacia el sol (normalizada)
uniform vec3  uSunColor;
uniform vec3  uWaterDeep;
uniform vec3  uWaterShallow;
uniform vec3  uBoundsMin;
uniform vec3  uBoundsMax;
uniform float uTime;

out vec4 FragColor;

// ─────────────────────────────────────────────────────────────────────
// Fase 5: micro-ondas. Suma de senos direccionales; devolvemos altura y
// gradiente analítico (por diferencias finitas) para inclinar la normal.
// ─────────────────────────────────────────────────────────────────────
float waveH(vec2 p, float t) {
    float h = 0.0;
    h += 0.0040 * sin(dot(p, vec2( 18.0,  7.0)) + t * 2.1);
    h += 0.0025 * sin(dot(p, vec2( -9.0, 27.0)) + t * 2.9);
    h += 0.0015 * sin(dot(p, vec2( 41.0,-16.0)) + t * 3.7);
    return h;
}

vec2 waveGrad(vec2 p, float t) {
    const float e = 0.02;
    float hx = waveH(p + vec2(e, 0.0), t) - waveH(p - vec2(e, 0.0), t);
    float hz = waveH(p + vec2(0.0, e), t) - waveH(p - vec2(0.0, e), t);
    return vec2(hx, hz) / (2.0 * e);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(uSunDir);

    // Detalle dependiente de la distancia: menos aliasing en la lejanía.
    float dist   = length(uCameraPos - vWorldPos);
    float detail = 1.0 / (1.0 + 0.15 * dist);

    // Perturbación de normal por micro-ondas.
    vec2 g = waveGrad(vWorldPos.xz, uTime);
    N = normalize(N + vec3(-g.x, 0.0, -g.y) * 1.5 * detail);

    // Ruido fino extra para romper la periodicidad de los senos.
    float micro = sin(vWorldPos.x * 41.0 + uTime * 4.1)
                * sin(vWorldPos.z * 37.0 - uTime * 3.3);
    N = normalize(N + vec3(micro, 0.0, micro) * 0.025 * detail);

    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    // Fresnel agresivo (Schlick): centro transparente, bordes reflectantes.
    const float F0 = 0.015;
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    fresnel = clamp(fresnel, 0.0, 1.0);

    // Color por profundidad aproximada dentro de la AABB del agua.
    float heightT = clamp((vWorldPos.y - uBoundsMin.y) /
                          max(uBoundsMax.y - uBoundsMin.y, 0.001), 0.0, 1.0);
    vec3 base = mix(uWaterDeep, uWaterShallow, heightT);

    // Ambiente + difusa.
    vec3 ambient = base * 0.28;
    vec3 diffuse = base * uSunColor * NdotL * 0.50;

    // Especular Blinn-Phong: sun glint más afilado en bordes.
    float shininess = mix(120.0, 600.0, fresnel);
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = uSunColor * spec * (0.50 + 2.0 * fresnel);

    // Reflejo de cielo aproximado, mucho más marcado a ángulos rasantes.
    vec3 skyColor = mix(vec3(0.30, 0.50, 0.82),
                        vec3(0.80, 0.93, 1.00),
                        pow(NdotV, 0.4));
    vec3 color = ambient + diffuse + specular;
    color = mix(color, skyColor, fresnel * 0.85);

    // Brillo sutil en las crestas de las micro-ondas.
    color += uSunColor * max(g.x + g.y, 0.0) * 0.15 * fresnel;

    // Transparencia: el centro deja ver el fondo, los bordes son opacos.
    float alpha = mix(0.30, 0.97, fresnel);
    alpha = mix(alpha, 0.85, heightT * 0.25);

    FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
