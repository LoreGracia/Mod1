#version 430 core

in vec3 vNormal; // Lo calcularemos en el fragment, pero lo declaramos aquí por convención

uniform vec3 uLightDir; // Dirección de la luz
uniform vec4 uWaterColor;

out vec4 FragColor;

void main() {
    // Reconstruir la normal usando las derivadas de la posición en espacio de pantalla
    // (Nota: para esto necesitaríamos la posición en el vertex shader, 
    //  pero para un color plano con un poco de sombra, podemos usar una normal falsa
    //  o pasar la posición. Vamos a hacerlo simple por ahora).
    
    // Color base del agua
    vec3 baseColor = vec3(0.1, 0.4, 0.8);
    
    // Simulamos una iluminación simple basada en la posición Y (más claro arriba)
    // Esto es un placeholder hasta que añadamos normales reales.
    
    FragColor = vec4(baseColor, 1.0);
}