#version 330 core

/**
* @file waterVertex.glsl
* @brief Vertex shader for rendering SPH particles as points.
* 
* Handles position transformation, hides settled particles below the water 
* surface (unless they are splashing), and scales the point size based 
* on the particle's density. Particles are filtered by the rain-drop flag:
* only rain drops are drawn, everything else belongs to the Marching Cubes
* water mesh. Drops that have already merged into the puddle are detected
* through their splat weight (computed by the GPU density pass) and hidden
* as mesh water.
*/

layout(location = 0) in vec3  aPos;     ///< Particle position.
layout(location = 1) in float aDensity; ///< Particle density.
layout(location = 2) in vec3  aVel;     ///< Particle velocity (used for splash detection).
layout(location = 3) in float aRainDrop; ///< Rain-drop flag (1 = drop, 0 = mesh water).
layout(location = 4) in float aSplatWeight; ///< Splat weight from the density pass (1 = settled puddle water).

uniform mat4  uViewProj;    ///< Combined View-Projection matrix.
uniform float uPointSize;   ///< Base size of the points in pixels.
uniform float uDensityMin;  ///< Minimum density for color/size mapping.
uniform float uDensityMax;  ///< Maximum density for color/size mapping.

// Legacy uniform retained for source compatibility: no longer read here.
// The puddle classification comes from aSplatWeight, computed per particle
// by the GPU density pass.
uniform float uHideBelowY;  ///< Height threshold to hide settled particles.
uniform float uSplashSpeed; ///< If |velocity| >= this, the particle is splashing and remains visible.

out float vDensity; ///< Passes density to the fragment shader.

/**
* @brief Main vertex shader execution.
*/
void main() {
	vDensity = aDensity;

	// Non rain-drop particles are never drawn as points: they belong to the
	// water surface mesh. A rain drop is hidden once it has merged into the
	// puddle (splatWeight > 0.5) and is almost at rest (fast drops stay
	// visible as splashes).
	bool isDrop = aRainDrop > 0.5;
	// Hide only if the drop is puddle water AND almost at rest. If the
	// velocity is high (bounce/splash), the particle remains visible.
	bool inPuddle = aSplatWeight > 0.5;
	bool atRest   = length(aVel) < uSplashSpeed;
	if (!isDrop || (inPuddle && atRest)) {
		gl_Position  = vec4(2.0, 2.0, 2.0, 1.0); // Move outside NDC (clip space)
		gl_PointSize = 0.0;
		return;
	}

	gl_Position = uViewProj * vec4(aPos, 1.0);

	// Size modulated by density (denser particles are drawn larger)
	float t = clamp((aDensity - uDensityMin) /
					max(uDensityMax - uDensityMin, 0.001), 0.0, 1.0);
	gl_PointSize = uPointSize * (0.7 + 0.6 * t);
}
