#version 430 core

// El VAO "vacío" asocia location 0 al vertex SSBO
layout(location = 0) in vec4 aPos;

uniform mat4 uViewProj;

void main() {
    gl_Position = uViewProj * aPos;
}
