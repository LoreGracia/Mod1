#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <Input.hpp>
#include <atomic>
#include <string>

/**
 * @file WWindow.hpp
 * @brief Defines the WWindow class, an object-oriented wrapper around a GLFW window.
 */


/**
 * @class WWindow
 * @brief Manages the lifecycle, OpenGL context, events, and input state of a single window.
 *
 * This class abstracts away the raw GLFW C API, providing a cleaner interface for
 * window creation, context switching, and event polling while automatically
 * routing input events to an internal Input instance.
 */
class WWindow
{
public:
	/**
	 * @struct Config
	 * @brief Configuration parameters used to create a WWindow instance.
	 */
	struct Config
	{
		std::string title = "Default Name";		///< The title displayed on the window title bar.
		int			width = 800, height = 600;	///< The width and height of the window client area.
		int			x = 0, y = 0;  ///< The initial X and Y position of the window on the screen.
		WWindow*	shareContext =
			nullptr;  ///< Pointer to another window to share the OpenGL context with.
		int	 glMajor = 3, glMinor = 3;	///< Requested major and minor OpenGL version.
		bool coreProfile =
			true;  ///< Whether to request a Core Profile (true) or Compatibility Profile (false).
		bool resizable = true;	///< Whether the window can be resized by the user.
		bool visible   = true;	///< Whether the window is visible immediately after creation.
		bool decorated = true;	///< Whether the window has borders and a title bar.
		bool vsync =
			true;  ///< Whether to enable vertical synchronization to prevent screen tearing.
		int samples =
			0;	///< Number of samples for MSAA (Multisample Anti-Aliasing). 0 disables MSAA.
	};

	/**
	 * @brief Constructs a new WWindow and initializes the underlying GLFW window.
	 * @param conf The configuration parameters.
	 */
	explicit WWindow(Config& conf);

	/** @brief Destroys the window and releases its GLFW resources. */
	~WWindow();

	/** @brief Copy constructor is deleted. */
	WWindow(const WWindow&) = delete;

	/** @brief Copy assignment operator is deleted. */
	WWindow& operator=(const WWindow&) = delete;

	/** @brief Move constructor is deleted. */
	WWindow(WWindow&&) = delete;

	/** @brief Move assignment operator is deleted. */
	WWindow& operator=(WWindow&&) = delete;

	/** @brief Makes this window's OpenGL context current on the calling thread. */
	void makeCurrent();

	/** @brief Releases the current OpenGL context without making another one current. */
	void releaseCurrent();

	/** @brief Swaps the front and back buffers, presenting the rendered frame to the screen. */
	void swapBuffers();

	/**
	 * @brief Processes all pending window events for this window.
	 * @return True if the window should remain open, false if it should close.
	 */
	bool pollEvents();

	/** @brief Static method to process pending events for all existing windows. */
	static void pollEventsAll();

	/** @brief Waits for events, blocking the thread until an event occurs or a timeout is reached.
	 */
	static void waitEvents(double timeout);

	/** @brief Waits indefinitely for events, blocking the thread. */
	static void waitEvents();

	/**
	 * @brief Sets the swap interval (VSync).
	 * @param interval 1 to enable VSync, 0 to disable it.
	 */
	void setSwapInterval(int interval);

	/** @brief Gets the underlying raw GLFW window pointer. */
	GLFWwindow* nativeWindow() const { return m_window; }

	/** @brief Gets the width of the window client area. */
	int width() const { return m_width; }

	/** @brief Gets the height of the window client area. */
	int height() const { return m_height; }

	/** @brief Gets the width of the framebuffer (may differ from width on High-DPI displays). */
	int framebufferWidth() const { return m_fbWidth; }

	/** @brief Gets the height of the framebuffer (may differ from height on High-DPI displays). */
	int framebufferHeight() const { return m_fbHeight; }

	/** @brief Calculates the aspect ratio (width / height) of the window. */
	float aspect() const { return m_height > 0 ? float(m_width) / float(m_height) : 1.f; }

	/** @brief Checks if the window has been requested to close. */
	bool shouldClose() const;

	/** @brief Sets the close flag, requesting the window to close. */
	void requestClose();

	/**
	 * @brief Checks and consumes a pending resize event.
	 * @param w Reference to an integer where the new width will be stored.
	 * @param h Reference to an integer where the new height will be stored.
	 * @return True if a resize occurred since the last call, false otherwise.
	 */
	bool consumeResize(int& w, int& h);

	/** @brief Gets the Input manager associated with this window. */
	Input& input() { return m_input; }

	/** @brief Gets the Input manager associated with this window (const). */
	const Input& input() const { return m_input; }

private:
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action,
							int mods);	///< Internal GLFW callback for keyboard events
	static void mouseButtonCallback(GLFWwindow* window, int button, int action,
									int mods);	///< Internal GLFW callback for mouse button events.
	static void cursorPosCallback(GLFWwindow* window, double x,
								  double y);  ///< Internal GLFW callback for mouse movement.
	static void scrollCallback(GLFWwindow* window, double xoffset,
							   double yoffset);	 ///< Internal GLFW callback for mouse scrolling.
	static void
	windowSizeCallback(GLFWwindow* window, int width,
					   int height);	 ///< Internal GLFW callback for window size changes.
	static void
	framebufferSizeCallback(GLFWwindow* window, int width,
							int height);  ///< Internal GLFW callback for framebuffer size changes.
	static void
	windowCloseCallback(GLFWwindow* window);  ///< Internal GLFW callback for window close requests.
	static void
	windowFocusCallback(GLFWwindow* window,
						int			focused);  ///< Internal GLFW callback for window focus changes.

	GLFWwindow* m_window   = nullptr;  ///< Pointer to the raw GLFW window.
	int			m_width	   = 0;		   ///< Current window width.
	int			m_height   = 0;		   ///< Current window height.
	int			m_fbWidth  = 0;		   ///< Current framebuffer width.
	int			m_fbHeight = 0;		   ///< Current framebuffer height.
	bool  m_needsResize	   = false;	 ///< Flag indicating if a resize event is pending consumption.
	Input m_input;					 ///< The input manager for this window.
	std::atomic<bool> m_shouldClose{false};	 ///< Flag indicating if the window should close.
};
