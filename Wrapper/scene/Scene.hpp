// inc/scene/Scene.hpp
#pragma once
#include "scene/Camera.hpp"
#include "scene/Entity.hpp"

#include <memory>
#include <vector>

class Scene
{
public:
	void addEntity(std::shared_ptr<Entity> e);
	void removeEntity(Entity* e);

	void update(float dt);
	void render(float aspect);

	Camera&		  camera() { return m_camera; }
	const Camera& camera() const { return m_camera; }

	const std::vector<std::shared_ptr<Entity>>& entities() const { return m_entities; }
	std::vector<std::shared_ptr<Entity>>&		entities() { return m_entities; }

private:
	std::vector<std::shared_ptr<Entity>> m_entities;
	Camera								 m_camera;
};