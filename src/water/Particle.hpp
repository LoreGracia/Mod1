#pragma once
#include <glm.hpp>

/**
 * @file Particle.hpp
 * @brief Defines the Particle struct, representing a single SPH fluid particle.
 */

/**
 * @struct Particle
 * @brief Holds the physical state and rendering metadata for a single SPH particle.
 *
 * This struct contains both the core SPH simulation variables (position, velocity,
 * density, pressure, force) and auxiliary variables used by the rendering pipeline
 * (splatWeight, rainDrop) to control how the particle affects the Marching Cubes mesh.
 */
struct Particle
{
	glm::vec3 position{0.f};   ///< World-space position of the particle.
	glm::vec3 velocity{0.f};   ///< Current velocity vector.
	glm::vec3 force{0.f};	   ///< Accumulated force for the current simulation step.
	float	  density  = 0.f;  ///< Current SPH density at the particle's position.
	float	  pressure = 0.f;  ///< Current SPH pressure at the particle's position.
	float	  age	   = 0.f;  ///< Time in seconds since the particle was spawned/reset.

	/**
	 * @brief Weight indicating how much this particle contributes to the density field.
	 * 1.0 = full contribution to the Marching Cubes mesh.
	 * 0.0 = ignored (e.g., a falling rain drop that should only be drawn as a point).
	 */
	float splatWeight = 1.f;

	/**
	 * @brief Flag indicating if this particle is part of the continuous rain recycling system.
	 * True for rain drops that respawn at the top when hitting the ground.
	 */
	bool rainDrop = false;
};
