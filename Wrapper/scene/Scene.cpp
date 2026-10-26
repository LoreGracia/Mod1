#include "scene/Scene.hpp"
#include <algorithm>

void Scene::addEntity(std::shared_ptr<Entity> e)
{
	if (e)
		m_entities.push_back(std::move(e));
}
void Scene::removeEntity(Entity *e)
{
	m_entities.erase(
	std::remove_if(m_entities.begin(), m_entities.end(),
		[e](const std::shared_ptr<Entity> &p) 
		{ return p.get() == e; }),
		m_entities.end());
}

void Scene::update(float dt)
{
	for (auto &e : m_entities)
		e->update(dt);
}
void Scene::render(float aspect)
{
	glm::mat4 vp = m_camera.projection(aspect) * m_camera.view();

	for (auto &e : m_entities)
		e->render(vp);
}