#version 430 core

// El VAO asocia:
//   location 0 -> posición (vec4)
//   location 1 -> normal   (vec4)
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec4 aNormal;

uniform mat4 uViewProj;

out vec3 vWorldPos;
out vec3 vNormal;

void main() {
    vWorldPos = aPos.xyz;

    vec3 n = aNormal.xyz;
    float len = length(n);
    vNormal = (len > 1e-8) ? n / len : vec3(0.0, 1.0, 0.0);

    gl_Position = uViewProj * aPos;
}
