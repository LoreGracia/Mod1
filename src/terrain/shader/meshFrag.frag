#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;
in float vHeight;
in float vMapHeight;

uniform float uMinHeight;
uniform float uMaxHeight;
uniform float uTime;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(vNormal);
    float h = clamp((vHeight - uMinHeight) / max(uMaxHeight - uMinHeight, 0.001), 0.0, 1.0);
    float slope = 1.0 - clamp(N.y, 0.0, 1.0);

    // Valle si el mapa original tiene picos bajos (< ~800 en escala parse)
    // Sierra si los picos son altos (> ~1000).
    float sierra = smoothstep(0.28, 0.42, vMapHeight);

    // --- Paleta valle: solo verdes y algo de roca ---
    vec3 grass  = vec3(0.18, 0.42, 0.15);
    vec3 forest = vec3(0.10, 0.28, 0.08);
    vec3 rock   = vec3(0.42, 0.40, 0.38);
    vec3 valleyColor = mix(grass, forest, smoothstep(0.2, 0.8, h));
    valleyColor = mix(valleyColor, rock, smoothstep(0.6, 0.9, slope) * 0.5);

    // --- Paleta sierra: arena → pasto → bosque → roca → nieve ---
    vec3 sand   = vec3(0.78, 0.70, 0.45);
    vec3 snow   = vec3(0.95, 0.97, 1.00);
    vec3 sierraColor = mix(sand, grass, smoothstep(0.05, 0.25, h));
    sierraColor = mix(sierraColor, forest, smoothstep(0.25, 0.55, h));
    sierraColor = mix(sierraColor, rock,   smoothstep(0.55, 0.78, h));
    sierraColor = mix(sierraColor, snow,   smoothstep(0.82, 0.95, h));
    sierraColor = mix(sierraColor, rock,   smoothstep(0.45, 0.75, slope));

    vec3 color = mix(valleyColor, sierraColor, sierra);

    // Iluminación
    float sunAngle = uTime * 0.1;
    vec3 L = normalize(vec3(cos(sunAngle) * 0.8, 0.8, sin(sunAngle) * 0.5));
    float diff = max(dot(N, L), 0.0);

    // Ambiente más alto en valle (evita "nieve falsa" por sobreexposición)
    float ambient = mix(0.45, 0.22, sierra);
    float lighting = ambient + (1.0 - ambient) * diff;

    // AO: suave en valle, marcado en sierra
    float aoLow  = mix(0.85, 1.00, smoothstep(0.1, 0.4, h));
    float aoHigh = mix(0.60, 1.00, smoothstep(0.1, 0.4, h));
    float ao = mix(aoLow, aoHigh, sierra);

    FragColor = vec4(color * lighting * ao, 1.0);
}