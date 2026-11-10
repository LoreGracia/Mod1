/**
 * @file Entity.cpp
 * @brief Implementation of the Entity class, handling basic movement and rendering.
 */

#include <Entity.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <iostream>
#include <quaternion.hpp>

/**
 * @brief Constructs an Entity with a specific mesh and shader.
 * @param m Shared pointer to the mesh geometry.
 * @param s Shared pointer to the shader program used for rendering.
 */
Entity::Entity(std::shared_ptr<Mesh> m, std::shared_ptr<Shader> s)
	: m_mesh(std::move(m)), m_shader(std::move(s))
{
}

/**
 * @brief Updates the entity's position and rotation based on its velocity and angular velocity.
 *
 * Integrates the linear velocity to update the position. If the angular velocity is non-zero,
 * it normalizes the vector to get the rotation axis and applies the rotation using the
 * magnitude of the angular velocity as the angle (converted to degrees).
 *
 * @param dt Delta time in seconds (time elapsed since the last frame).
 */
void Entity::update(float dt)
{
	m_transform.position += velocity * dt;
	float len = glm::length(angularVel);

	if (len > 1e-6f)
		m_transform.rotate(glm::degrees(len) * dt, angularVel / len);
}

/**
 * @brief Renders the entity using its assigned mesh and shader.
 *
 * Binds the shader, updates the Model matrix uniform (from the Transform component),
 * sets the combined View-Projection matrix, and issues the draw call for the mesh.
 *
 * @param viewProj The combined view-projection matrix from the active camera.
 */
void Entity::render(const glm::mat4& viewProj)
{
	if (!m_shader || !m_mesh)
		return;
	m_shader->use();

	m_shader->setMat4("uModel", m_transform.matrix());
	m_shader->setMat4("uViewProj", viewProj);
	m_mesh->draw();
}
