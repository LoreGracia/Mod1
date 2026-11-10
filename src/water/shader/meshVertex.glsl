#version 430 core

/**
 * @file meshVertex.glsl
 * @brief Vertex shader for rendering the Marching Cubes water mesh.
 * 
 * Transforms the vertex position to clip space and passes the world position 
 * and normalized normal to the fragment shader for lighting calculations.
 */

// The VAO associates:
//   location 0 -> position (vec4)
//   location 1 -> normal   (vec4)
layout(location = 0) in vec4 aPos;    ///< Vertex position in world space (w is typically 1.0).
layout(location = 1) in vec4 aNormal; ///< Vertex normal (w is typically 0.0).

uniform mat4 uViewProj; ///< Combined View-Projection matrix.

out vec3 vWorldPos; ///< World position passed to the fragment shader.
out vec3 vNormal;   ///< Normalized normal passed to the fragment shader.

/**
 * @brief Main vertex shader execution.
 */
void main() {
    vWorldPos = aPos.xyz;

    vec3 n = aNormal.xyz;
    float len = length(n);
    // Safely normalize the normal, falling back to a default up vector if 
    // the input vector is zero-length.
    vNormal = (len > 1e-8) ? n / len : vec3(0.0, 1.0, 0.0);

    gl_Position = uViewProj * aPos;
}
