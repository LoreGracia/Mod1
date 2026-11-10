#version 430 core

/**
 * @file meshFrag.frag
 * @brief Fragment shader for rendering the water surface with realistic lighting.
 * 
 * Implements procedural micro-waves (sum of directional sines), detailed normal 
 * perturbation, Schlick's Fresnel approximation, Blinn-Phong specular highlights, 
 * and depth-based color blending to simulate water transparency and sky reflection.
 */

in vec3 vWorldPos; ///< Interpolated world position from the vertex shader.
in vec3 vNormal;   ///< Interpolated smooth normal from the vertex shader.

uniform vec3  uCameraPos;     ///< Position of the camera in world space.
uniform vec3  uSunDir;        ///< Direction towards the sun (normalized).
uniform vec3  uSunColor;      ///< Color of the sunlight.
uniform vec3  uWaterDeep;     ///< Color of deep water.
uniform vec3  uWaterShallow;  ///< Color of shallow water.
uniform vec3  uBoundsMin;     ///< Minimum bounds of the water volume.
uniform vec3  uBoundsMax;     ///< Maximum bounds of the water volume.
uniform float uTime;          ///< Current time for wave animation.

out vec4 FragColor;

// ─────────────────────────────────────────────────────────────────────
// Phase 5: micro-waves. Sum of directional sines; returns height.
// ─────────────────────────────────────────────────────────────────────
/**
 * @brief Calculates the height of the procedural micro-waves at a given 2D position.
 * @param p The 2D position (usually world x and z coordinates).
 * @param t The current time.
 * @return The combined wave height.
 */
float waveH(vec2 p, float t) {
    float h = 0.0;
    h += 0.0040 * sin(dot(p, vec2( 18.0,  7.0)) + t * 2.1);
    h += 0.0025 * sin(dot(p, vec2( -9.0, 27.0)) + t * 2.9);
    h += 0.0015 * sin(dot(p, vec2( 41.0,-16.0)) + t * 3.7);
    return h;
}

/**
 * @brief Computes the gradient of the wave height using finite differences.
 * 
 * Used to perturb the surface normal and create the illusion of ripples.
 * 
 * @param p The 2D position.
 * @param t The current time.
 * @return A 2D gradient vector (dx, dz).
 */
vec2 waveGrad(vec2 p, float t) {
    const float e = 0.02;
    float hx = waveH(p + vec2(e, 0.0), t) - waveH(p - vec2(e, 0.0), t);
    float hz = waveH(p + vec2(0.0, e), t) - waveH(p - vec2(0.0, e), t);
    return vec2(hx, hz) / (2.0 * e);
}

/**
 * @brief Main fragment shading logic for the water surface.
 */
void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(uSunDir);

    // Distance-based detail attenuation to reduce aliasing in the distance.
    float dist   = length(uCameraPos - vWorldPos);
    float detail = 1.0 / (1.0 + 0.15 * dist);

    // Normal perturbation by micro-waves.
    vec2 g = waveGrad(vWorldPos.xz, uTime);
    N = normalize(N + vec3(-g.x, 0.0, -g.y) * 1.5 * detail);

    // Extra fine noise to break the periodicity of the sine waves.
    float micro = sin(vWorldPos.x * 41.0 + uTime * 4.1)
                * sin(vWorldPos.z * 37.0 - uTime * 3.3);
    N = normalize(N + vec3(micro, 0.0, micro) * 0.025 * detail);

    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    // Aggressive Fresnel (Schlick): transparent center, reflective edges.
    const float F0 = 0.015;
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    fresnel = clamp(fresnel, 0.0, 1.0);

    // Approximate depth-based color within the water AABB.
    float heightT = clamp((vWorldPos.y - uBoundsMin.y) /
                          max(uBoundsMax.y - uBoundsMin.y, 0.001), 0.0, 1.0);
    vec3 base = mix(uWaterDeep, uWaterShallow, heightT);

    // Ambient + Diffuse.
    vec3 ambient = base * 0.28;
    vec3 diffuse = base * uSunColor * NdotL * 0.50;

    // Blinn-Phong specular: sun glint sharper at grazing angles.
    float shininess = mix(120.0, 600.0, fresnel);
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = uSunColor * spec * (0.50 + 2.0 * fresnel);

    // Approximate sky reflection, much stronger at grazing angles.
    vec3 skyColor = mix(vec3(0.30, 0.50, 0.82),
                        vec3(0.80, 0.93, 1.00),
                        pow(NdotV, 0.4));
    vec3 color = ambient + diffuse + specular;
    color = mix(color, skyColor, fresnel * 0.85);

        // Subtle highlight on the wave crests.
    color += uSunColor * max(g.x + g.y, 0.0) * 0.15 * fresnel;

    // Transparency: the center lets the background through, edges are opaque.
    float alpha = mix(0.30, 0.97, fresnel);
    alpha = mix(alpha, 0.85, heightT * 0.25);

    FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
