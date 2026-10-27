// src/scene/Entity.cpp
#include <Entity.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <iostream>
#include <quaternion.hpp>

Entity::Entity(std::shared_ptr<Mesh> m, std::shared_ptr<Shader> s)
	: m_mesh(std::move(m)), m_shader(std::move(s))
{
}

void Entity::update(float dt)
{
	m_transform.position += velocity * dt;
	float len = glm::length(angularVel);

	if (len > 1e-6f)
		m_transform.rotate(glm::degrees(len) * dt, angularVel / len);
}

void Entity::render(const glm::mat4& viewProj)
{
	if (!m_shader || !m_mesh)
		return;
	m_shader->use();

	m_shader->setMat4("uModel", m_transform.matrix());
	m_shader->setMat4("uViewProj", viewProj);
	m_mesh->draw();
}
