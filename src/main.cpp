#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif

#include <Terrain.hpp>
#include <cstdio>
#include <memory>

int main(int ac, char** av)
{
	try {
		(void)ac;
		Terrain te(av[1]);
		te.render();
	} catch (const std::exception& e) {
		std::fprintf(stderr, "exception: %s\n", e.what());
		return 1;
	}
	return 0;
}