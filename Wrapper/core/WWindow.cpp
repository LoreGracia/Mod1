/**
 * @file WWindow.cpp
 * @brief Implementation of the WWindow class, managing GLFW window lifecycles,
 * context switching, and event routing.
 */

#include <WWindow.hpp>
#include <cstdio>
#include <mutex>
#include <stdexcept>

namespace
{
std::mutex g_glfwMutex;			   ///< Mutex to protect GLFW initialization state across threads.
int		   g_glfwRefCount	 = 0;  ///< Reference counter for GLFW instances.
bool	   g_glfwInitialized = false;  ///< Flag indicating if GLFW is globally initialized.

/**
 * @brief Internal GLFW error callback. Prints errors to stderr.
 * @param code The GLFW error code.
 * @param description A human-readable description of the error.
 */
void errorCallback(int code, const char* description)
{
	std::fprintf(stderr, "GLFW error %d: %s\n", code, description ? description : "(null)");
}

/**
 * @brief Ensures GLFW is initialized. Thread-safe.
 * Increments the reference counter if already initialized.
 * @throw std::runtime_error If glfwInit fails.
 */
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

/**
 * @brief Decrements the GLFW reference counter. Thread-safe.
 * Calls glfwTerminate() when the counter reaches zero.
 */
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

/**
 * @brief Retrieves the WWindow instance associated with a GLFW window.
 * @param window The raw GLFW window pointer.
 * @return Pointer to the corresponding WWindow instance.
 */
WWindow* windowFrom(GLFWwindow* window)
{
	return static_cast<WWindow*>(glfwGetWindowUserPointer(window));
}
}  // namespace

/**
 * @brief Constructs the WWindow by creating the underlying GLFW window and setting up callbacks.
 * @param conf The configuration parameters.
 * @throw std::runtime_error If GLFW initialization or window creation fails.
 */
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
		// Clean up on construction failure to prevent leaks.
		if (m_window) {
			glfwDestroyWindow(m_window);
			m_window = nullptr;
		}
		releaseGlfw();
		throw;
	}
}

/**
 * @brief Destroys the WWindow. Releases the GLFW context if it was current and destroys the window.
 */
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

/** @brief Makes this window's OpenGL context current on the calling thread. */
void WWindow::makeCurrent()
{
	glfwMakeContextCurrent(m_window);
}

/** @brief Releases the current OpenGL context. */
void WWindow::releaseCurrent()
{
	glfwMakeContextCurrent(nullptr);
}

/** @brief Swaps the front and back buffers. */
void WWindow::swapBuffers()
{
	glfwSwapBuffers(m_window);
}

/**
 * @brief Polls for events and updates the input state.
 * @return True if the window should remain open.
 */
bool WWindow::pollEvents()
{
	m_input.beginFrame();
	glfwPollEvents();
	return !shouldClose();
}

/** @brief Polls for events across all windows. */
void WWindow::pollEventsAll()
{
	glfwPollEvents();
}

/** @brief Waits for events with a timeout. */
void WWindow::waitEvents(double timeout)
{
	glfwWaitEventsTimeout(timeout);
}

/** @brief Waits indefinitely for events. */
void WWindow::waitEvents()
{
	glfwWaitEvents();
}

/** @brief Sets the swap interval (VSync). */
void WWindow::setSwapInterval(int interval)
{
	glfwSwapInterval(interval);
}

/**
 * @brief Checks if the window should close.
 * @return True if a close was requested or the native window is closing.
 */
bool WWindow::shouldClose() const
{
	if (!m_window)
		return true;
	return m_shouldClose.load() || glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

/** @brief Requests the window to close. */
void WWindow::requestClose()
{
	m_shouldClose.store(true);
	if (m_window)
		glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

/**
 * @brief Checks for a pending resize event and consumes it.
 * @param w Output parameter for the new framebuffer width.
 * @param h Output parameter for the new framebuffer height.
 * @return True if a resize occurred.
 */
bool WWindow::consumeResize(int& w, int& h)
{
	if (!m_needsResize)
		return false;
	w			  = m_fbWidth;
	h			  = m_fbHeight;
	m_needsResize = false;
	return true;
}

/**
 * @brief Internal callback for keyboard events.
 * Routes the event to the internal Input manager.
 */
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

/**
 * @brief Internal callback for mouse button events.
 * Routes the event to the internal Input manager.
 */
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

/**
 * @brief Internal callback for mouse cursor position changes.
 * Routes the event to the internal Input manager.
 */
void WWindow::cursorPosCallback(GLFWwindow* window, double x, double y)
{
	WWindow* self = windowFrom(window);

	if (self)
		self->m_input.onMouseMove(x, y);
}

/**
 * @brief Internal callback for mouse scroll events.
 * Routes the event to the internal Input manager.
 */
void WWindow::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	WWindow* self = windowFrom(window);

	if (self)
		self->m_input.onScroll(xoffset, yoffset);
}

/**
 * @brief Internal callback for window size changes.
 * Updates the window width and height.
 */
void WWindow::windowSizeCallback(GLFWwindow* window, int width, int height)
{
	WWindow* self = windowFrom(window);

	if (!self)
		return;
	self->m_width  = width;
	self->m_height = height;
}

/**
 * @brief Internal callback for framebuffer size changes.
 * Updates the framebuffer dimensions and flags a pending resize for the renderer.
 */
void WWindow::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	WWindow* self = windowFrom(window);

	if (!self)
		return;
	self->m_fbWidth		= width;
	self->m_fbHeight	= height;
	self->m_needsResize = true;
}

/**
 * @brief Internal callback for window close requests.
 * Sets the internal close flag.
 */
void WWindow::windowCloseCallback(GLFWwindow* window)
{
	WWindow* self = windowFrom(window);

	if (self)
		self->m_shouldClose.store(true);
}

/**
 * @brief Internal callback for window focus changes.
 * Resets input states on focus loss to prevent stuck keys.
 */
void WWindow::windowFocusCallback(GLFWwindow* window, int focused)
{
	WWindow* self = windowFrom(window);

	if (self)
		self->m_input.onFocus(focused == GLFW_TRUE);
}
