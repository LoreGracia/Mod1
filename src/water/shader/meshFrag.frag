#version 430 core

in vec3 vWorldPos;
out vec4 FragColor;

void main() {
    vec3 dx = dFdx(vWorldPos);
    vec3 dy = dFdy(vWorldPos);
    vec3 normal = normalize(cross(dx, dy));

    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.0));
    vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0)); // Cámara mirando al frente

    float diff = max(dot(normal, lightDir), 0.0);
    
    // Reflejo especular (el brillo del sol)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 128.0);
    
    // Efecto Fresnel: cuando miras el agua en ángulo, es más opaca/reflectante
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0);

    // Color base del agua
    vec3 waterColor = vec3(0.05, 0.35, 0.7); 
    
    // Mezclamos iluminación
    vec3 finalColor = (waterColor * diff) + vec3(spec);
    
    // El alpha (transparencia) depende del Fresnel. 
    // En el centro del charco mirando desde arriba, será más transparente.
    float alpha = mix(0.4, 0.9, fresnel); 

    FragColor = vec4(finalColor + vec3(0.1, 0.2, 0.3), alpha);
}