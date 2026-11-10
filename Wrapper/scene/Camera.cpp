/**
 * @file Camera.cpp
 * @brief Implementation of the Camera class, providing view and projection
 * matrix generation, as well as orbital and movement controls.
 */

#include <Camera.hpp>
#include <cmath>
#include <gtc/matrix_transform.hpp>

/**
 * @brief Constructs a new Camera.
 *
 * Initializes the position, target, and world up vectors. It also calculates
 * the initial spherical coordinates (distance, pitch, yaw) based on the
 * relative position of the camera to the target.
 *
 * @param pos The initial position of the camera in world space.
 * @param target The point the camera is looking at.
 * @param up The world's up vector (usually (0, 1, 0)).
 */
Camera::Camera(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up)
	: m_position(pos), m_target(target), m_worldUp(up)
{
	glm::vec3 d = pos - target;
	m_distance	= glm::length(d);
	if (m_distance > 1e-6f) {
		glm::vec3 n = d / m_distance;

		m_pitch = glm::degrees(std::asin(glm::clamp(n.y, -1.f, 1.f)));
		m_yaw	= glm::degrees(std::atan2(n.x, n.z));
	}
}

/**
 * @brief Computes the view matrix.
 * @return A 4x4 view matrix based on the camera's position, target, and up vector.
 */
glm::mat4 Camera::view() const
{
	return glm::lookAt(m_position, m_target, m_worldUp);
}

/**
 * @brief Computes the perspective projection matrix.
 * @param aspect The aspect ratio of the viewport (width / height).
 * @return A 4x4 perspective projection matrix.
 */
glm::mat4 Camera::projection(float aspect) const
{
	return glm::perspective(glm::radians(m_fov), aspect, m_near, m_far);
}

/**
 * @brief Translates the camera and its target by a given delta.
 * Useful for free-flight movement (panning).
 * @param delta The translation vector in world space.
 */
void Camera::move(const glm::vec3& delta)
{
	m_position += delta;
	m_target += delta;
}

/**
 * @brief Sets a new target point for the camera to look at.
 * @param target The new target position.
 */
void Camera::lookAt(const glm::vec3& target)
{
	m_target = target;
}

/**
 * @brief Orbits the camera around its target using spherical coordinates.
 *
 * Updates the yaw, pitch, and distance, then recalculates the camera's position
 * in space. The pitch is clamped to prevent flipping over the poles (gimbal lock).
 *
 * @param dYaw Change in yaw angle (horizontal rotation) in degrees.
 * @param dPitch Change in pitch angle (vertical rotation) in degrees.
 * @param dDist Change in distance from the target.
 */
void Camera::orbit(float dYaw, float dPitch, float dDist)
{
	m_yaw += dYaw;
	m_pitch += dPitch;
	m_pitch	   = glm::clamp(m_pitch, -89.f, 89.f);
	m_distance = glm::max(0.1f, m_distance + dDist);

	float	  ry = glm::radians(m_yaw);
	float	  rp = glm::radians(m_pitch);
	glm::vec3 offset{m_distance * std::cos(rp) * std::sin(ry), m_distance * std::sin(rp),
					 m_distance * std::cos(rp) * std::cos(ry)};
	m_position = m_target + offset;
}

/**
 * @brief Gets the forward direction vector of the camera.
 * @return A normalized vector pointing from the camera to the target.
 */
glm::vec3 Camera::forward() const
{
	return glm::normalize(m_target - m_position);
}

/**
 * @brief Gets the right direction vector of the camera.
 * @return A normalized vector pointing to the right relative to the camera's view.
 */
glm::vec3 Camera::right() const
{
	return glm::normalize(glm::cross(forward(), m_worldUp));
}

/**
 * @brief Gets the up direction vector of the camera.
 * @return A normalized vector pointing upwards relative to the camera's view.
 */
glm::vec3 Camera::up() const
{
	return glm::normalize(glm::cross(right(), forward()));
}
