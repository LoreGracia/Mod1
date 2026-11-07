#pragma once
#include <glm.hpp>

struct Particle
{
	glm::vec3	position{0.f};
	glm::vec3	velocity{0.f};
	glm::vec3	force   {0.f};
	float	density		= 0.f;
	float	pressure	= 0.f;
	float	age			= 0.f;
	// 1 = aporta densidad al Marching Cubes; 0 = gota en el aire (solo puntos).
	float	splatWeight	= 1.f;
	// Subconjunto que se recicla continuamente para mantener la lluvia.
	bool	rainDrop	= false;
};
