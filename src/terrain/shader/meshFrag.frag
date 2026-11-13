
#version 330 core

in vec3 vNormal;
in float vHeight;

out vec4 FragColor;

void main()
{
    vec3 lowColor  = vec3(0.12, 0.38, 0.12);
    vec3 midColor  = vec3(0.40, 0.40, 0.30);
    vec3 highColor = vec3(0.85, 0.85, 0.82);

    // Transición de colores según elevación.
    float h = clamp(vHeight / 0.6, 0.0, 1.0);

    vec3 terrainColor;

    if (h < 0.5)
        terrainColor = mix(lowColor, midColor, h * 2.0);
    else
        terrainColor = mix(midColor, highColor, (h - 0.5) * 2.0);

    // Iluminación difusa.
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(vec3(-0.4, 0.8, 0.3));

    float diffuse = max(dot(normal, lightDir), 0.0);

    // Luz ambiental + luz difusa.
    float ambient = 0.25;
    float lighting = ambient + 0.75 * diffuse;

    vec3 finalColor = terrainColor * lighting;

    FragColor = vec4(finalColor, 1.0);
}
