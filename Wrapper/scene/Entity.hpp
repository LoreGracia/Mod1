#pragma once
#include "Transform.hpp"

#include <glm.hpp>
#include <memory>
#include <string>

class Shader;
class Mesh;

/**
 * @file Entity.hpp
 * @brief Defines the base Entity class, representing a renderable object in the scene.
 */

/**
 * @class Entity
 * @brief Represents a basic object in the scene with geometry, material, and spatial properties.
 *
 * The Entity class acts as a base for any renderable object. It combines a Mesh (geometry),
 * a Shader (material/rendering logic), and a Transform (position, rotation, scale).
 * It also includes basic physics integration for linear and angular velocity.
 */
class Entity
{
public:
	/** @brief Default constructor. */
	Entity() = default;

	/**
	 * @brief Constructs an Entity with a specific mesh and shader.
	 * @param mesh Shared pointer to the geometry.
	 * @param shader Shared pointer to the shader program used for rendering.
	 */
	Entity(std::shared_ptr<Mesh> mesh, std::shared_ptr<Shader> shader);

	/** @brief Virtual destructor to ensure proper cleanup of derived classes. */
	virtual ~Entity() = default;

	/**
	 * @brief Updates the entity's state.
	 *
	 * Integrates linear and angular velocity over time to update the Transform.
	 * This method is virtual so derived classes can implement custom update logic.
	 *
	 * @param dt Delta time in seconds.
	 */
	virtual void update(float dt);

	/**
	 * @brief Renders the entity.
	 *
	 * Binds the shader, passes the required uniforms, and draws the mesh.
	 * This method is virtual to allow custom rendering behaviors.
	 *
	 * @param viewProj The combined view-projection matrix from the active camera.
	 */
	virtual void render(const glm::mat4& viewProj);

	/** @brief Gets a reference to the entity's Transform component. */
	Transform& transform() { return m_transform; }

	/** @brief Gets a const reference to the entity's Transform component. */
	const Transform& transform() const { return m_transform; }

	/** @brief Assigns a new mesh to the entity. */
	void setMesh(std::shared_ptr<Mesh> m) { m_mesh = std::move(m); }

	/** @brief Assigns a new shader to the entity. */
	void setShader(std::shared_ptr<Shader> s) { m_shader = std::move(s); }

	glm::vec3 velocity{0.f};	///< Linear velocity in world space.
	glm::vec3 angularVel{0.f};	///< Angular velocity vector (axis of rotation * speed in rad/s).

protected:
	Transform				m_transform;  ///< Spatial properties (position, rotation, scale).
	std::shared_ptr<Mesh>	m_mesh;		  ///< Shared pointer to the entity's geometry.
	std::shared_ptr<Shader> m_shader;	  ///< Shared pointer to the entity's shader program.
};
