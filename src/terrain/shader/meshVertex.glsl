#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform float uTime;
uniform float uMapHeight;

out vec3 vNormal;
out vec3 vWorldPos;
out float vHeight;
out float vMapHeight;

void main(){
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos  = world.xyz;
    vHeight    = aPos.y;
    vMapHeight = uMapHeight;
    vNormal    = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    gl_Position = uViewProj * world;
}