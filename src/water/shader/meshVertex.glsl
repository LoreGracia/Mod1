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

uniform mat4 uViewProj;      ///< Combined View-Projection matrix.
uniform float uWaterEnergy;  ///< Legacy particle-energy uniform retained for source compatibility.
uniform float uWaterLevel;   ///< World-space height of the settled water surface.
uniform float uTime;         ///< Current time for the small surface waves.

out vec3 vWorldPos; ///< World position passed to the fragment shader.
out vec3 vNormal;   ///< Normalized normal passed to the fragment shader.

/**
* @brief Small procedural wave height for the forced water sheet.
* @param p Horizontal world position.
* @param t Current time in seconds.
* @return Wave height in approximately [-1, 1].
*/
float waveH(vec2 p, float t) {
	float h = 0.0;
	h += sin(dot(p, vec2(12.0,  7.0)) + t * 1.9);
	h += 0.5 * sin(dot(p, vec2(-8.0, 17.0)) + t * 2.7);
	return h * (2.0 / 3.0);
}

/**
* @brief Main vertex shader execution.
*/
void main() {
	vec3 outPos = aPos.xyz;
	vWorldPos = outPos;

	// Force every vertex onto the authoritative water surface. The surface
	// never depends on particle energy or particle height; only a tiny
	// micro-wave is added on top of uWaterLevel.
	float microWave = sin(vWorldPos.x * 30.0 + uTime * 2.0) * 0.005;
	outPos.y = uWaterLevel + microWave;

	vWorldPos = outPos;

	vec3 n = aNormal.xyz;
	float len = length(n);
	// Safely normalize the normal, falling back to a default up vector if 
	// the input vector is zero-length.
	vNormal = (len > 1e-8) ? n / len : vec3(0.0, 1.0, 0.0);

	gl_Position = uViewProj * vec4(outPos, 1.0);
}
