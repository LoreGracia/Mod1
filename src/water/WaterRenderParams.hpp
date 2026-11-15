#pragma once

#include <Shader.hpp>
#include <glm.hpp>

/**
 * @file WaterRenderParams.hpp
 * @brief Material and lighting parameters shared by every water renderer.
 *
 * A single instance of WaterRenderParams feeds both the Marching Cubes mesh
 * and the fake water volume, so both representations use identical colors,
 * Fresnel terms, procedural-sky colors and foam settings.
 */

/**
 * @struct WaterRenderParams
 * @brief Shared water material, lighting and volume parameters.
 */
struct WaterRenderParams
{
	glm::vec3 deep{0.015f, 0.10f, 0.28f};		///< Color of deep water.
	glm::vec3 shallow{0.10f, 0.48f, 0.78f};		///< Color of shallow water.
	glm::vec3 sunDir{0.55f, 1.00f, 0.35f};		///< Direction towards the sun.
	glm::vec3 sunColor{1.00f, 0.95f, 0.82f};	///< Color of the sunlight.
	glm::vec3 skyHorizon{0.82f, 0.66f, 0.50f};	///< Warm color of the procedural sky horizon.
	glm::vec3 skyZenith{0.06f, 0.18f, 0.42f};	///< Deep color of the procedural sky zenith.
	glm::vec3 sunGlow{1.00f, 0.83f, 0.58f};		///< Color of the sun halo in the reflected sky.
	glm::vec3 foamColor{0.92f, 0.96f, 1.00f};	///< Base color of the procedural foam.
	float	  foamIntensity = 1.00f;			///< Global strength of the foam layer.
	float	  foamThreshold = 0.45f;			///< Foam cutoff: lower values expand the patches.
	float	  foamScale		= 4.50f;			///< Spatial frequency of the foam noise.
	float	  foamSpeed		= 2.50f;			///< Weight of the derived flow/velocity term.
	float	  waterEnergy	= 0.00f;			///< Mean squared particle speed.
	glm::vec3 boundsMin{-1.0f, -1.0f, -1.0f};	///< Minimum corner of the water volume.
	glm::vec3 boundsMax{1.0f, 3.0f, 1.0f};		///< Maximum corner of the water volume.
	float	  surfaceY = -1.0f;					///< World-space height of the current water surface.
};

/**
 * @brief Uploads the shared water material uniforms to a shader program.
 *
 * Missing uniforms are silently ignored by Shader::set*, so the same call can
 * be used with the mesh shader and the volume shader even though each declares
 * a different subset of the shared parameters.
 *
 * @param shader Target shader program.
 * @param params Material, lighting and bounds parameters.
 */
inline void applyWaterUniforms(Shader& shader, const WaterRenderParams& params)
{
	shader.setVec3("uWaterDeep", params.deep);
	shader.setVec3("uWaterShallow", params.shallow);
	shader.setVec3("uSunDir", glm::normalize(params.sunDir));
	shader.setVec3("uSunColor", params.sunColor);
	shader.setVec3("uSkyHorizon", params.skyHorizon);
	shader.setVec3("uSkyZenith", params.skyZenith);
	shader.setVec3("uSunGlowColor", params.sunGlow);
	shader.setVec3("uFoamColor", params.foamColor);
	shader.setFloat("uFoamIntensity", params.foamIntensity);
	shader.setFloat("uFoamThreshold", params.foamThreshold);
	shader.setFloat("uFoamScale", params.foamScale);
	shader.setFloat("uFoamSpeed", params.foamSpeed);
	shader.setFloat("uWaterEnergy", params.waterEnergy);
	shader.setVec3("uBoundsMin", params.boundsMin);
	shader.setVec3("uBoundsMax", params.boundsMax);
	shader.setFloat("uSurfaceY", params.surfaceY);
}
