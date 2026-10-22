// src/scene/Camera.cpp
#include <Camera.hpp>
#include <gtc/matrix_transform.hpp>
#include <cmath>

Camera::Camera(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up)
	: m_position(pos), m_target(target), m_worldUp(up)
{
	glm::vec3 d = pos - target;
	m_distance = glm::length(d);
	if (m_distance > 1e-6f) {
		glm::vec3 n = d / m_distance;
		m_pitch = glm::degrees(std::asin(glm::clamp(n.y, -1.f, 1.f)));
		m_yaw   = glm::degrees(std::atan2(n.x, n.z));
	}
}

glm::mat4 Camera::view() const {
	return glm::lookAt(m_position, m_target, m_worldUp);
}

glm::mat4 Camera::projection(float aspect) const {
	return glm::perspective(glm::radians(m_fov), aspect, m_near, m_far);
}

void Camera::move(const glm::vec3& delta) {
	m_position += delta;
	m_target += delta;
}

void Camera::lookAt(const glm::vec3& target) {
	m_target = target;
}

void Camera::orbit(float dYaw, float dPitch, float dDist) {
	m_yaw   += dYaw;
	m_pitch += dPitch;
	m_pitch = glm::clamp(m_pitch, -89.f, 89.f);
	m_distance = glm::max(0.1f, m_distance + dDist);

	float ry = glm::radians(m_yaw);
	float rp = glm::radians(m_pitch);
	glm::vec3 offset{
		m_distance * std::cos(rp) * std::sin(ry),
		m_distance * std::sin(rp),
		m_distance * std::cos(rp) * std::cos(ry)
	};
	m_position = m_target + offset;
}

glm::vec3 Camera::forward() const {
	return glm::normalize(m_target - m_position);
}

glm::vec3 Camera::right() const {
	return glm::normalize(glm::cross(forward(), m_worldUp));
}

glm::vec3 Camera::up() const {
	return glm::normalize(glm::cross(right(), forward()));
}