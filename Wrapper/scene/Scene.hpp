#pragma once
#include "scene/Camera.hpp"
#include "scene/Entity.hpp"

#include <memory>
#include <vector>

/**
 * @file Scene.hpp
 * @brief Defines the Scene class, which acts as a container for entities and the active camera.
 */

/**
 * @class Scene
 * @brief Manages a collection of Entities and a single active Camera.
 *
 * The Scene is responsible for updating and rendering all its entities
 * and provides the necessary aspect ratio and view-projection matrices
 * to the rendering pipeline.
 */
class Scene
{
public:
	/**
	 * @brief Adds a new entity to the scene.
	 * @param e Shared pointer to the Entity to add.
	 */
	void addEntity(std::shared_ptr<Entity> e);

	/**
	 * @brief Removes a specific entity from the scene.
	 * @param e Raw pointer to the Entity to remove.
	 */
	void removeEntity(Entity* e);

	/**
	 * @brief Updates all entities in the scene.
	 * @param dt Delta time in seconds.
	 */
	void update(float dt);

	/**
	 * @brief Renders all entities in the scene using the internal camera.
	 * @param aspect The aspect ratio of the viewport (width / height).
	 */
	void render(float aspect);

	/** @brief Gets a reference to the active camera. */
	Camera& camera() { return m_camera; }

	/** @brief Gets a const reference to the active camera. */
	const Camera& camera() const { return m_camera; }

	/** @brief Gets a const reference to the list of entities. */
	const std::vector<std::shared_ptr<Entity>>& entities() const { return m_entities; }

	/** @brief Gets a mutable reference to the list of entities. */
	std::vector<std::shared_ptr<Entity>>& entities() { return m_entities; }

private:
	std::vector<std::shared_ptr<Entity>> m_entities;  ///< List of active entities in the scene.
	Camera								 m_camera;	  ///< The active camera used for rendering.
};
