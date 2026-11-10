/**
 * @file Scene.cpp
 * @brief Implementation of the Scene class, managing a collection of entities
 * and a camera for rendering.
 */

#include "scene/Scene.hpp"

#include <algorithm>

/**
 * @brief Adds a new entity to the scene.
 * @param e Shared pointer to the Entity to add.
 * @note Ignores null pointers to prevent invalid entities in the scene graph.
 */
void Scene::addEntity(std::shared_ptr<Entity> e)
{
	if (e)
		m_entities.push_back(std::move(e));
}

/**
 * @brief Removes a specific entity from the scene by its raw pointer.
 *
 * Uses the erase-remove idiom to efficiently remove the entity from the vector
 * without invalidating other iterators.
 *
 * @param e Raw pointer to the Entity to remove.
 */
void Scene::removeEntity(Entity* e)
{
	m_entities.erase(std::remove_if(m_entities.begin(), m_entities.end(),
									[e](const std::shared_ptr<Entity>& p) { return p.get() == e; }),
					 m_entities.end());
}

/**
 * @brief Updates all entities in the scene.
 * @param dt Delta time in seconds since the last frame.
 */
void Scene::update(float dt)
{
	for (auto& e : m_entities)
		e->update(dt);
}

/**
 * @brief Renders all entities in the scene.
 *
 * Computes the combined View-Projection matrix once using the scene's camera
 * and the provided aspect ratio, then passes it to each entity's render method.
 *
 * @param aspect The aspect ratio of the current viewport (width / height).
 */
void Scene::render(float aspect)
{
	glm::mat4 vp = m_camera.projection(aspect) * m_camera.view();

	for (auto& e : m_entities)
		e->render(vp);
}
