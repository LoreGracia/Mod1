#pragma once
#include <glm.hpp>
#include <quaternion.hpp>

class Transform
{
public:
	glm::vec3 position{0.f};
	glm::quat rotation{1.f, 0.f, 0.f, 0.f};
	glm::vec3 scale{1.f};

	glm::mat4 matrix() const;
	void	  translate(const glm::vec3& d) { position += d; }
	void	  rotate(float angleDeg, const glm::vec3& axis);
	void	  rotateEuler(float pitchDeg, float yawDeg, float rollDeg);
};
