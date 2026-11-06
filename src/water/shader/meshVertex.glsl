#version 430 core

// El atributo location 0 coincide con el glVertexAttribPointer de C++
layout(location = 0) in vec4 aPos; 

uniform mat4 uViewProj;

out vec3 vWorldPos;

void main() {
    vWorldPos = aPos.xyz;
    gl_Position = uViewProj * aPos;
}