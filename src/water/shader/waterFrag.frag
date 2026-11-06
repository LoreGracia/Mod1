#version 430 core

in vec3 vWorldPos;
out vec4 FragColor;

void main() {
    // Calcular normal (Flat Shading)
    vec3 dx = dFdx(vWorldPos);
    vec3 dy = dFdy(vWorldPos);
    vec3 normal = normalize(cross(dx, dy));

    // Direcciones de luz y cámara
    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.0));
    vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0)); // Asumiendo cámara en +Z

    // 1. Luz Difusa (Color base)
    float diff = max(dot(normal, lightDir), 0.0);
    
    // 2. Brillo Especular (Reflejo del sol en el agua)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0); // 64 = qué tan brillante es

    // 3. Color del agua
    vec3 waterColor = vec3(0.1, 0.4, 0.8);
    vec3 ambient = waterColor * 0.3;
    vec3 diffuse = waterColor * diff * 0.7;
    vec3 specular = vec3(1.0) * spec * 0.9; // Brillo blanco puro

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}