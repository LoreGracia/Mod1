#include <WWindow.hpp>
#include <cstdio>
#include <mutex>
#include <stdexcept>

namespace
{
std::mutex g_glfwMutex;
int		   g_glfwRefCount	 = 0;
bool	   g_glfwInitialized = false;

void errorCallback(int code, const char* description)
{
	std::fprintf(stderr, "GLFW error %d: %s\n", code, description ? description : "(null)");
}

void ensureGlfw()
{
	std::lock_guard<std::mutex> lock(g_glfwMutex);

	if (!g_glfwInitialized) {
		glfwSetErrorCallback(errorCallback);
		if (!glfwInit())
			throw std::runtime_error("glfwInit failed");
		g_glfwInitialized = true;
	}
	++g_glfwRefCount;
}

void releaseGlfw()
{
	std::lock_guard<std::mutex> lock(g_glfwMutex);

	if (g_glfwRefCount > 0)
		--g_glfwRefCount;
	if (g_glfwRefCount == 0 && g_glfwInitialized) {
		glfwTerminate();
		g_glfwInitialized = false;
	}
}

WWindow* windowFrom(GLFWwindow* window)
{
	return static_cast<WWindow*>(glfwGetWindowUserPointer(window));
}
}  // namespace

WWindow::WWindow(Config& conf)
{
	ensureGlfw();

	try {
		glfwDefaultWindowHints();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, conf.glMajor);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, conf.glMinor);

		if (conf.glMajor > 3 || (conf.glMajor == 3 && conf.glMinor >= 2)) {
			glfwWindowHint(GLFW_OPENGL_PROFILE, conf.coreProfile ? GLFW_OPENGL_CORE_PROFILE
																 : GLFW_OPENGL_COMPAT_PROFILE);
		}

		glfwWindowHint(GLFW_RESIZABLE, conf.resizable ? GLFW_TRUE : GLFW_FALSE);
		glfwWindowHint(GLFW_VISIBLE, conf.visible ? GLFW_TRUE : GLFW_FALSE);
		glfwWindowHint(GLFW_DECORATED, conf.decorated ? GLFW_TRUE : GLFW_FALSE);
		glfwWindowHint(GLFW_SAMPLES, conf.samples);

		GLFWwindow* share = conf.shareContext ? conf.shareContext->m_window : nullptr;

		m_window = glfwCreateWindow(conf.width, conf.height, conf.title.c_str(), nullptr, share);
		if (!m_window)
			throw std::runtime_error("glfwCreateWindow failed");

		if (conf.x != 0 || conf.y != 0)
			glfwSetWindowPos(m_window, conf.x, conf.y);

		glfwSetWindowUserPointer(m_window, this);

		glfwSetKeyCallback(m_window, keyCallback);
		glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
		glfwSetCursorPosCallback(m_window, cursorPosCallback);
		glfwSetScrollCallback(m_window, scrollCallback);
		glfwSetWindowSizeCallback(m_window, windowSizeCallback);
		glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
		glfwSetWindowCloseCallback(m_window, windowCloseCallback);
		glfwSetWindowFocusCallback(m_window, windowFocusCallback);

		glfwGetWindowSize(m_window, &m_width, &m_height);
		glfwGetFramebufferSize(m_window, &m_fbWidth, &m_fbHeight);

		makeCurrent();
		setSwapInterval(conf.vsync ? 1 : 0);
	} catch (...) {
		if (m_window) {
			glfwDestroyWindow(m_window);
			m_window = nullptr;
		}
		releaseGlfw();
		throw;
	}
}

WWindow::~WWindow()
{
	if (m_window) {
		if (glfwGetCurrentContext() == m_window)
			glfwMakeContextCurrent(nullptr);
		glfwDestroyWindow(m_window);
		m_window = nullptr;
	}
	releaseGlfw();
}

void WWindow::makeCurrent()
{
	glfwMakeContextCurrent(m_window);
}

void WWindow::releaseCurrent()
{
	glfwMakeContextCurrent(nullptr);
}

void WWindow::swapBuffers()
{
	glfwSwapBuffers(m_window);
}

bool WWindow::pollEvents()
{
	m_input.beginFrame();
	glfwPollEvents();
	return !shouldClose();
}

void WWindow::pollEventsAll()
{
	glfwPollEvents();
}

void WWindow::waitEvents(double timeout)
{
	glfwWaitEventsTimeout(timeout);
}

void WWindow::waitEvents()
{
	glfwWaitEvents();
}

void WWindow::setSwapInterval(int interval)
{
	glfwSwapInterval(interval);
}

bool WWindow::shouldClose() const
{
	if (!m_window)
		return true;
	return m_shouldClose.load() || glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

void WWindow::requestClose()
{
	m_shouldClose.store(true);
	if (m_window)
		glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

bool WWindow::consumeResize(int& w, int& h)
{
	if (!m_needsResize)
		return false;
	w			  = m_fbWidth;
	h			  = m_fbHeight;
	m_needsResize = false;
	return true;
}

void WWindow::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void)scancode;
	(void)mods;

	WWindow* self = windowFrom(window);

	if (!self)
		return;
	if (action == GLFW_PRESS)
		self->m_input.onKeyDown(Key::fromGlfw(key));
	else if (action == GLFW_RELEASE)
		self->m_input.onKeyUp(Key::fromGlfw(key));
}

void WWindow::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	(void)mods;

	WWindow* self = windowFrom(window);

	if (!self)
		return;
	if (action == GLFW_PRESS)
		self->m_input.onMouseButtonDown(Mouse::fromGlfw(button));
	else if (action == GLFW_RELEASE)
		self->m_input.onMouseButtonUp(Mouse::fromGlfw(button));
}

void WWindow::cursorPosCallback(GLFWwindow* window, double x, double y)
{
	WWindow* self = windowFrom(window);

	if (self)
		self->m_input.onMouseMove(x, y);
}

void WWindow::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	WWindow* self = windowFrom(window);

	if (self)
		self->m_input.onScroll(xoffset, yoffset);
}

void WWindow::windowSizeCallback(GLFWwindow* window, int width, int height)
{
	WWindow* self = windowFrom(window);

	if (!self)
		return;
	self->m_width  = width;
	self->m_height = height;
}

void WWindow::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	WWindow* self = windowFrom(window);

	if (!self)
		return;
	self->m_fbWidth		= width;
	self->m_fbHeight	= height;
	self->m_needsResize = true;
}

void WWindow::windowCloseCallback(GLFWwindow* window)
{
	WWindow* self = windowFrom(window);

	if (self)
		self->m_shouldClose.store(true);
}

void WWindow::windowFocusCallback(GLFWwindow* window, int focused)
{
	WWindow* self = windowFrom(window);

	if (self)
		self->m_input.onFocus(focused == GLFW_TRUE);
}
