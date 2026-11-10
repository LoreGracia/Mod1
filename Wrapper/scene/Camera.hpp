#pragma once
#include <glm.hpp>

/**
 * @file Camera.hpp
 * @brief Defines the Camera class, handling view and projection matrices,
 * as well as orbital and free-flight movement.
 */

/**
 * @class Camera
 * @brief Represents a 3D camera in the scene.
 *
 * Manages the camera's position, target, and up vector to generate view matrices.
 * It also handles perspective projection parameters (FOV, near/far planes) and
 * provides orbital controls around a target point using spherical coordinates.
 */
class Camera
{
public:
	/** @brief Default constructor. Initializes a camera at (0,0,5) looking at the origin. */
	Camera() = default;

	/**
	 * @brief Constructs a Camera with a specific position, target, and up vector.
	 * @param position The initial position of the camera in world space.
	 * @param target The point the camera is looking at.
	 * @param up The world's up vector. Defaults to (0, 1, 0).
	 */
	Camera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up = {0, 1, 0});

	/** @brief Computes and returns the view matrix based on the current camera state. */
	glm::mat4 view() const;

	/**
	 * @brief Computes and returns the perspective projection matrix.
	 * @param aspect The aspect ratio of the viewport (width / height).
	 */
	glm::mat4 projection(float aspect) const;

	/**
	 * @brief Translates the camera and its target in world space.
	 * @param delta The translation vector.
	 */
	void move(const glm::vec3& delta);

	/**
	 * @brief Sets a new target point for the camera to look at.
	 * @param target The new target position.
	 */
	void lookAt(const glm::vec3& target);

	/** @brief Sets the camera's position directly. */
	void setPosition(const glm::vec3& p) { m_position = p; }

	/** @brief Sets the field of view in degrees. */

	/** @brief Sets the near and far clipping planes. */
	void setFov(float deg) { m_fov = deg; }

	/** @brief Sets the near and far clipping planes. */
	void setClip(float n, float f)
	{
		m_near = n;
		m_far  = f;
	}

	/** @brief Sets the target point for orbital movement. */
	void setOrbitTarget(const glm::vec3& t) { m_target = t; }

	/**
	 * @brief Orbits the camera around its target using spherical coordinates.
	 * @param dYaw Change in yaw angle (horizontal rotation) in degrees.
	 * @param dPitch Change in pitch angle (vertical rotation) in degrees.
	 * @param dDist Change in distance from the target.
	 */
	void orbit(float dYaw, float dPitch, float dDist);

	/** @brief Gets the current position of the camera. */
	const glm::vec3& position() const { return m_position; }

	/** @brief Gets the forward direction vector of the camera. */
	glm::vec3 forward() const;

	/** @brief Gets the right direction vector of the camera. */
	glm::vec3 right() const;

	/** @brief Gets the up direction vector of the camera. */
	glm::vec3 up() const;

private:
	glm::vec3 m_position{0.f, 0.f, 5.f};  ///< The camera's position in world space.
	glm::vec3 m_target{0.f};			  ///< The point the camera is looking at.
	glm::vec3 m_worldUp{0.f, 1.f, 0.f};	  ///< The world's up vector.
	float	  m_fov	 = 60.f;			  ///< Field of view in degrees.
	float	  m_near = 0.1f;			  ///< Near clipping plane.
	float	  m_far	 = 1000.f;			  ///< Far clipping plane.

	float m_yaw		 = 0.f;	 ///< Horizontal rotation angle (for orbital control).
	float m_pitch	 = 0.f;	 ///< Vertical rotation angle (for orbital control).
	float m_distance = 5.f;	 ///< Distance from the camera to the target (for orbital control).
};
