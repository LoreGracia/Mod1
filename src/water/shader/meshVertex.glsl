#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uViewProj;
out vec3 vColor;
void main(){
  vColor = aPos * 0.5 + 0.5;
  gl_Position = uViewProj * vec4(aPos, 1.0);
}