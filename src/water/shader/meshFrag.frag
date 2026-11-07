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

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(uSunDir);
    vec3 H = normalize(L + V);

    // Ondulación muy leve para romper el aspecto de superficie rígida.
    float ripple = 0.055
                 * sin(vWorldPos.x * 10.0 + uTime * 1.3)
                 * cos(vWorldPos.z *  8.0 - uTime * 1.1);
    N = normalize(N + vec3(ripple, 0.0, ripple * 0.7));

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    // Fresnel (Schlick) con F0 típico del agua.
    const float F0 = 0.02;
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    // Color por profundidad aproximada dentro de la AABB del agua.
    float heightT = clamp((vWorldPos.y - uBoundsMin.y) /
                          max(uBoundsMax.y - uBoundsMin.y, 0.001), 0.0, 1.0);
    vec3 base = mix(uWaterDeep, uWaterShallow, heightT);

    // Ambiente + difusa.
    vec3 ambient = base * 0.32;
    vec3 diffuse = base * uSunColor * NdotL * 0.55;

    // Especular Blinn-Phong: sun glint.
    float shininess = mix(96.0, 420.0, fresnel);
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = uSunColor * spec * (0.45 + 1.7 * fresnel);

    // Reflejo de cielo aproximado, modulado por Fresnel.
    vec3 skyColor = mix(vec3(0.35, 0.55, 0.85),
                        vec3(0.75, 0.90, 1.00),
                        pow(NdotV, 0.5));
    vec3 color = ambient + diffuse + specular;
    color = mix(color, skyColor, fresnel * 0.65);

    // Brillo sutil en las crestas iluminadas.
    color += uSunColor * max(ripple, 0.0) * fresnel * 0.5;

    // Transparencia: más opaco en ángulos rasantes y en zonas profundas.
    float alpha = mix(0.55, 0.94, fresnel);
    alpha = mix(alpha, 0.96, heightT * 0.35);

    FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
