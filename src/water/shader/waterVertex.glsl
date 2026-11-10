#version 330 core

/**
 * @file waterVertex.glsl
 * @brief Vertex shader for rendering SPH particles as points.
 * 
 * Handles position transformation, hides settled particles below the water 
 * surface (unless they are splashing), and scales the point size based 
 * on the particle's density.
 */

layout(location = 0) in vec3  aPos;     ///< Particle position.
layout(location = 1) in float aDensity; ///< Particle density.
layout(location = 2) in vec3  aVel;     ///< Particle velocity (used for splash detection).

uniform mat4  uViewProj;    ///< Combined View-Projection matrix.
uniform float uPointSize;   ///< Base size of the points in pixels.
uniform float uDensityMin;  ///< Minimum density for color/size mapping.
uniform float uDensityMax;  ///< Maximum density for color/size mapping.

// Phase 3.7/4: Drops below this height are considered part of the puddle and 
// should not be drawn as points... unless they are splashing.
uniform float uHideBelowY;  ///< Height threshold to hide settled particles.
uniform float uSplashSpeed; ///< If |velocity| >= this, the particle is splashing and remains visible.

out float vDensity; ///< Passes density to the fragment shader.

/**
 * @brief Main vertex shader execution.
 */
void main() {
    vDensity = aDensity;

    // Hide only if inside the puddle AND almost at rest. If the velocity 
    // is high (bounce/splash), the particle remains visible.
    bool insidePool = aPos.y < uHideBelowY;
    bool atRest     = length(aVel) < uSplashSpeed;
    if (insidePool && atRest) {
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
