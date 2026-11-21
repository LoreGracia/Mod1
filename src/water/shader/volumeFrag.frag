#version 430 core
#include "waterShading.glsl"

/**
 * @file volumeFrag.frag
 * @brief Fragment shader for the fake water volume.
 *
 * The volume uses the same shared shading functions as the Marching Cubes
 * surface so its color, Fresnel response and reflected sky match the real
 * water surface. The normal is reconstructed from screen-space derivatives
 * because the volume geometry only provides positions.
 */

in vec3 vWorldPos; ///< Interpolated world position from the vertex shader.

uniform vec3 uCameraPos;    ///< Position of the camera in world space.
uniform vec3 uWaterDeep;    ///< Color of deep water.
uniform vec3 uWaterShallow; ///< Color of shallow water.
uniform vec3 uSunDir;       ///< Direction towards the sun (normalized by the uploader).
uniform vec3 uSunColor;     ///< Color of the sunlight.
uniform vec3 uSkyHorizon;   ///< Warm color of the procedural sky at the horizon.
uniform vec3 uSkyZenith;    ///< Deep color of the procedural sky at the zenith.
uniform vec3 uSunGlowColor; ///< Color of the sun halo in the reflected sky.
uniform vec3 uFoamColor;    ///< Base color of the procedural foam.
uniform vec3 uBoundsMin;    ///< Minimum bounds of the water volume.
uniform vec3 uBoundsMax;    ///< Maximum bounds of the water volume.
uniform float uSurfaceY;    ///< World-space height of the authoritative water surface.
uniform sampler2D uTerrainHeight; ///< R32F heightfield sampled in world-space XZ.
uniform vec2 uTerrainMinXZ;      ///< Minimum corner of the terrain footprint in world XZ.
uniform vec2 uTerrainMaxXZ;      ///< Maximum corner of the terrain footprint in world XZ.
uniform int  uUseTerrainHeight;  ///< 1 when the terrain height texture is bound, 0 otherwise.

out vec4 FragColor;

/**
 * @brief Samples the terrain elevation at a world-space XZ position.
 *
 * Coordinates outside the terrain footprint are clamped to the edge texels.
 * When no height texture is bound, a very low sentinel value disables the
 * terrain clip so the volume renders unmodified.
 *
 * @param xz World-space XZ position of the fragment.
 * @return Terrain height in world units, or a low sentinel when unavailable.
 */
float terrainHeight(vec2 xz) {
	if (uUseTerrainHeight == 0)
		return -1000.0;

	vec2 extent = max(uTerrainMaxXZ - uTerrainMinXZ, vec2(1e-4));
	vec2 uv		= clamp((xz - uTerrainMinXZ) / extent, 0.0, 1.0);
	return texture(uTerrainHeight, uv).r;
}

/**
 * @brief Main fragment shader execution.
 */
void main() {
	/**
	 * @brief Discards fragments that lie inside the terrain solid.
	 */
	float ground = terrainHeight(vWorldPos.xz);
	if (vWorldPos.y < ground - 0.01)
		discard;

	vec3 V = normalize(uCameraPos - vWorldPos);

	// Screen-space derivatives provide a flat normal per cube face.
	vec3 Ng = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));

	// Discard only the top cap: the real top is supplied by Marching Cubes.
	// The bottom cap remains visible as a solid blue floor.
	if (Ng.y > 0.9)
		discard;

	vec3 N = dot(Ng, V) < 0.0 ? -Ng : Ng;

	vec3  L       = normalize(uSunDir);
	float NdotV   = max(dot(N, V), 0.0);
	float fresnel = waterFresnel(NdotV);

	// Use the same height-based palette as the Marching Cubes surface, but
	// map it across the current water column instead of the full container.
	vec3 waterMax = vec3(uBoundsMax.x, max(uSurfaceY, uBoundsMin.y + 0.001), uBoundsMax.z);
	vec3 base = waterBaseColor(vWorldPos.y, uBoundsMin, waterMax, uWaterDeep, uWaterShallow);

	// The volume has no foam and no wave gradient, so both terms are zero.
	vec3 R        = reflect(-V, N);
	vec3 skyColor = waterSky(R, L, uSkyHorizon, uSkyZenith, uSunGlowColor);
	vec3 color    = waterShade(N, V, base, L, uSunColor, fresnel, skyColor,
							   0.0, uFoamColor, vec2(0.0));

	// The body of the water is nearly opaque; only grazing views retain a
	// small amount of Fresnel transparency.
	float alpha = mix(0.85, 1.0, fresnel);
	FragColor   = vec4(color, alpha);
}
