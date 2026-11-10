#version 330 core

/**
 * @file waterFrag.frag
 * @brief Fragment shader for rendering SPH particles as circular points.
 * 
 * Uses the point coordinate to discard pixels outside a circle, creating
 * a soft-edged particle. The color is interpolated based on the particle's
 * density, and the alpha is modulated by the edge softness.
 */

in float vDensity; ///< Interpolated density value from the vertex shader.

uniform vec3  uColorLow;    ///< Color for particles with low density.
uniform vec3  uColorHigh;   ///< Color for particles with high density.
uniform float uDensityMin;  ///< Minimum density threshold for color mapping.
uniform float uDensityMax;  ///< Maximum density threshold for color mapping.
uniform float uAlpha;       ///< Base alpha (opacity) of the particle.

out vec4 FragColor;

/**
 * @brief Main fragment shader execution.
 */
void main() {
    // Circular clipping: convert point coordinates to [-1, 1] range.
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(uv, uv);
    
    // Discard fragments outside the unit circle.
    if (r2 > 1.0) discard;
    
    // Soft edge transition for anti-aliasing.
    float edge = smoothstep(1.0, 0.85, r2);

    // Density-based coloring: normalize density between min and max.
    float t = clamp((vDensity - uDensityMin) /
                    max(uDensityMax - uDensityMin, 0.001), 0.0, 1.0);
    vec3 color = mix(uColorLow, uColorHigh, t);

    FragColor = vec4(color, uAlpha * edge);
}
