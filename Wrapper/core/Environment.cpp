#include "core/Environment.hpp"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <glad/gl.h>

#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdio>
#include <stdexcept>

Environment::~Environment()
{
	stop();
	if (m_glLoaded && !m_windows.empty()) {
		m_windows.front()->makeCurrent();
		cleanupGpuResources();
	}
}

WWindow& Environment::addWindow(WWindow::Config& cfg)
{
	if (m_running.load())
		throw std::runtime_error("addWindow() after run()");

	if (!m_windows.empty() && cfg.shareContext == nullptr)
		cfg.shareContext = m_windows.front().get();

	m_windows.emplace_back(new WWindow(cfg));
	return *m_windows.back();
}

void Environment::run()
{
	if (m_windows.empty())
		return;

	WWindow& first = *m_windows.front();

	first.makeCurrent();
	if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
		first.releaseCurrent();
		throw std::runtime_error("gladLoadGL failed");
	}
	m_glLoaded = true;

	const char* version	 = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	const char* vendor	 = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
	const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

	std::printf("OpenGL %s | %s | %s\n", version ? version : "?", vendor ? vendor : "?",
				renderer ? renderer : "?");

	if (m_init)
		m_init(first);

	for (std::size_t i = 1; i < m_windows.size(); ++i) {
		m_windows[i]->makeCurrent();
		if (m_init)
			m_init(*m_windows[i]);
	}

	m_running.store(true);

	auto last = std::chrono::steady_clock::now();

	while (m_running.load()) {
		bool anyOpen = false;

		for (auto& window : m_windows) {
			if (!window->shouldClose()) {
				anyOpen = true;
				break;
			}
		}
		if (!anyOpen)
			break;

		for (auto& window : m_windows)
			window->input().beginFrame();

		WWindow::pollEventsAll();

		auto  now = std::chrono::steady_clock::now();
		float dt  = std::chrono::duration<float>(now - last).count();
		last	  = now;

		for (auto& window : m_windows) {
			if (window->shouldClose())
				continue;

			window->makeCurrent();

			if (m_update)
				m_update(*window, dt);
			if (m_render)
				m_render(*window);

			window->swapBuffers();
			window->input().endFrame();
		}
	}

	first.makeCurrent();
	cleanupGpuResources();
}

void Environment::cleanupGpuResources()
{
	m_gpuResources.clear();
}
