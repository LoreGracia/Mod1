#include "core/Environment.hpp"

#include <glad/gl.h>
#include <glad/glad_glx.h>
#include <X11/Xlib.h>

#include <cstdio>
#include <stdexcept>

Environment::~Environment() {
	stop();
	for (auto& t : m_threads)
		if (t.joinable()) t.join();
}

WWindow& Environment::addWindow(WWindow::Config& cfg) {
	if (m_running.load())
		throw std::runtime_error("addWindow() after run()");
	m_windows.emplace_back(new WWindow(cfg));
	return *m_windows.back();
}

void Environment::run() {

	if (m_windows.empty())
		return;

	WWindow &first = *m_windows.front();

	first.makeCurrent();
	if (!gladLoadGL((GLADloadfunc)glXGetProcAddressARB)) {
		first.releaseCurrent();
		throw std::runtime_error("gladLoadGL failed");
	}

	if (m_init)
		m_init(first);
	first.releaseCurrent();

	m_running.store(true);
	m_threads.reserve(m_windows.size());
	for (auto& w : m_windows)
		m_threads.emplace_back(&Environment::windowThread, this, w.get());
	for (auto& t : m_threads)
		t.join();
	m_threads.clear();

	cleanupGpuResources();
}

void Environment::windowThread(WWindow* win) {
	win->makeCurrent();

	if (m_init && win != m_windows.front().get())
		m_init(*win);

	auto last = std::chrono::steady_clock::now();

	while (m_running.load() && !win->shouldClose()) {
		if (!win->pollEvents()) break;

		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - last).count();
		last = now;

		if (m_update) m_update(*win, dt);
		if (m_render) m_render(*win);

		win->swapBuffers();
	}

	win->releaseCurrent();
}

void Environment::cleanupGpuResources() {
	if (m_gpuResources.empty() || m_windows.empty())
		return;
	m_windows.front()->makeCurrent();
	m_gpuResources.clear();
	m_windows.front()->releaseCurrent();
}
