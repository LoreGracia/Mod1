/**
 * @file Transform.cpp
 * @brief Implementation of the Transform class, handling 3D spatial transformations
 * (translation, rotation, and scale) using quaternions to avoid gimbal lock.
 */
#include <Transform.hpp>
#include <matrix_transform.hpp>
#include <quaternion.hpp>

/**
 * @brief Computes the 4x4 transformation matrix representing this Transform.
 *
 * The matrix is constructed by multiplying the Translation, Rotation, and Scale
 * matrices in that exact order (T * R * S).
 *
 * @return A 4x4 model matrix.
 */
glm::mat4 Transform::matrix() const
{
	glm::mat4 T = glm::translate(glm::mat4(1.f), position);
	glm::mat4 R = glm::mat4_cast(rotation);
	glm::mat4 S = glm::scale(glm::mat4(1.f), scale);
	return T * R * S;
}

/**
 * @brief Rotates the transform by a given angle around a specific axis.
 *
 * @param deg The angle of rotation in degrees.
 * @param axis The axis of rotation (it will be normalized internally).
 * @note The resulting quaternion is normalized to prevent numerical drift
 * over time due to floating-point accumulation errors.
 */
void Transform::rotate(float deg, const glm::vec3& axis)
{
	rotation = glm::normalize(glm::rotate(rotation, glm::radians(deg), glm::normalize(axis)));
}

/**
 * @brief Sets the rotation of the transform using Euler angles (Pitch, Yaw, Roll).
 *
 * Creates individual quaternions for each axis of rotation and combines them.
 *
 * @param p Pitch (rotation around the X axis) in degrees.
 * @param y Yaw (rotation around the Y axis) in degrees.
 * @param r Roll (rotation around the Z axis) in degrees.
 * @note The resulting quaternion is normalized to ensure it represents a valid
 * rotation without scaling.
 */
void Transform::rotateEuler(float p, float y, float r)
{
	glm::quat qx = glm::angleAxis(glm::radians(p), glm::vec3(1, 0, 0));
	glm::quat qy = glm::angleAxis(glm::radians(y), glm::vec3(0, 1, 0));
	glm::quat qz = glm::angleAxis(glm::radians(r), glm::vec3(0, 0, 1));
	rotation	 = glm::normalize(qy * qx * qz);
}
