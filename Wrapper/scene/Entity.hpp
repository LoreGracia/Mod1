#pragma once
#include <Transform.hpp>
#include <glm.hpp>
#include <memory>
#include <string>

class Shader;
class Mesh;

class Entity {
	public:
		Entity() = default;
		Entity(std::shared_ptr<Mesh> mesh, std::shared_ptr<Shader> shader);
		virtual ~Entity() = default;

		virtual void update(float dt);
		virtual void render(const glm::mat4 &viewProj);

		Transform &transform()       { return m_transform; }
		const Transform &transform() const { return m_transform; }

		void setMesh  (std::shared_ptr<Mesh>   m) { m_mesh = std::move(m); }
		void setShader(std::shared_ptr<Shader> s) { m_shader = std::move(s); }

		glm::vec3 velocity{0.f};
		glm::vec3 angularVel{0.f};

	protected:
		Transform                m_transform;
		std::shared_ptr<Mesh>    m_mesh;
		std::shared_ptr<Shader>  m_shader;
};
