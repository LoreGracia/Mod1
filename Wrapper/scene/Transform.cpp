#include <Transform.hpp>
#include <matrix_transform.hpp>
#include <quaternion.hpp>

glm::mat4 Transform::matrix() const
{
	glm::mat4 T = glm::translate(glm::mat4(1.f), position);
	glm::mat4 R = glm::mat4_cast(rotation);
	glm::mat4 S = glm::scale(glm::mat4(1.f), scale);
	return T * R * S;
}

void Transform::rotate(float deg, const glm::vec3& axis)
{
	rotation = glm::normalize(glm::rotate(rotation, glm::radians(deg), glm::normalize(axis)));
}

void Transform::rotateEuler(float p, float y, float r)
{
	glm::quat qx = glm::angleAxis(glm::radians(p), glm::vec3(1, 0, 0));
	glm::quat qy = glm::angleAxis(glm::radians(y), glm::vec3(0, 1, 0));
	glm::quat qz = glm::angleAxis(glm::radians(r), glm::vec3(0, 0, 1));
	rotation	 = glm::normalize(qy * qx * qz);
}
