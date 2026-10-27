// inc/scene/Camera.hpp
#pragma once
#include <glm.hpp>

class Camera
{
public:
	Camera() = default;
	Camera(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up = {0, 1, 0});

	glm::mat4 view() const;
	glm::mat4 projection(float aspect) const;

	void move(const glm::vec3& delta);
	void lookAt(const glm::vec3& target);
	void setPosition(const glm::vec3& p) { m_position = p; }
	void setFov(float deg) { m_fov = deg; }
	void setClip(float n, float f)
	{
		m_near = n;
		m_far  = f;
	}

	void setOrbitTarget(const glm::vec3& t) { m_target = t; }
	void orbit(float dYaw, float dPitch, float dDist);

	const glm::vec3& position() const { return m_position; }

	glm::vec3 forward() const;
	glm::vec3 right() const;
	glm::vec3 up() const;

private:
	glm::vec3 m_position{0.f, 0.f, 5.f};
	glm::vec3 m_target{0.f};
	glm::vec3 m_worldUp{0.f, 1.f, 0.f};
	float	  m_fov	 = 60.f;
	float	  m_near = 0.1f;
	float	  m_far	 = 1000.f;

	float m_yaw		 = 0.f;
	float m_pitch	 = 0.f;
	float m_distance = 5.f;
};
