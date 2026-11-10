#pragma once
#include <glm.hpp>
#include <quaternion.hpp>

/**
 * @file Transform.hpp
 * @brief Defines the Transform class, representing the spatial state (position, rotation, scale)
 * of an object in the scene.
 */

/**
 * @class Transform
 * @brief Manages 3D spatial transformations using a TRS (Translation, Rotation, Scale) model.
 *
 * Rotation is handled via quaternions to avoid gimbal lock and ensure smooth
 * interpolation. The class provides helper methods to generate the final 4x4
 * model matrix or apply local transformations.
 */
class Transform
{
public:
	glm::vec3 position{0.f};				 ///< The translation of the object in world space.
	glm::quat rotation{1.f, 0.f, 0.f, 0.f};	 ///< The rotation of the object (w, x, y, z).
	glm::vec3 scale{1.f};					 ///< The scale of the object along each axis.

	/**
	 * @brief Computes the final 4x4 model matrix from the TRS components.
	 * @return The combined Translation * Rotation * Scale matrix.
	 */
	glm::mat4 matrix() const;

	/**
	 * @brief Translates the object by a given delta.
	 * @param d The translation vector to add to the current position.
	 */
	void translate(const glm::vec3& d) { position += d; }

	/**
	 * @brief Rotates the object around a specific axis by a given angle.
	 * @param angleDeg The angle of rotation in degrees.
	 * @param axis The axis of rotation (will be normalized internally).
	 */
	void rotate(float angleDeg, const glm::vec3& axis);

	/**
	 * @brief Overwrites the current rotation using Euler angles.
	 * @param pitchDeg Rotation around the X axis in degrees.
	 * @param yawDeg Rotation around the Y axis in degrees.
	 * @param rollDeg Rotation around the Z axis in degrees.
	 */
	void rotateEuler(float pitchDeg, float yawDeg, float rollDeg);
};
