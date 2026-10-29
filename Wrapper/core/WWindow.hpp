#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <Input.hpp>
#include <atomic>
#include <string>

class WWindow
{
public:
	struct Config
	{
		std::string title = "Default Name";
		int			width = 800, height = 600;
		int			x = 0, y = 0;
		WWindow*	shareContext = nullptr;
		int			glMajor = 3, glMinor = 3;
		bool		coreProfile = true;
		bool		resizable = true;
		bool		visible = true;
		bool		decorated = true;
		bool		vsync = true;
		int			samples = 0;
	};

	explicit WWindow(Config& conf);
	~WWindow();
	WWindow(const WWindow&)			   = delete;
	WWindow& operator=(const WWindow&) = delete;
	WWindow(WWindow&&)				   = delete;
	WWindow& operator=(WWindow&&)	   = delete;

	void makeCurrent();
	void releaseCurrent();
	void swapBuffers();
	bool pollEvents();

	static void pollEventsAll();
	static void waitEvents(double timeout);
	static void waitEvents();

	void setSwapInterval(int interval);

	GLFWwindow* nativeWindow() const { return m_window; }

	int	  width() const { return m_width; }
	int	  height() const { return m_height; }
	int	  framebufferWidth() const { return m_fbWidth; }
	int	  framebufferHeight() const { return m_fbHeight; }
	float aspect() const
	{
		return m_height > 0 ? float(m_width) / float(m_height) : 1.f;
	}

	bool shouldClose() const;
	void requestClose();
	bool consumeResize(int& w, int& h);

	Input&		 input() { return m_input; }
	const Input& input() const { return m_input; }

private:
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void cursorPosCallback(GLFWwindow* window, double x, double y);
	static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
	static void windowSizeCallback(GLFWwindow* window, int width, int height);
	static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
	static void windowCloseCallback(GLFWwindow* window);
	static void windowFocusCallback(GLFWwindow* window, int focused);

	GLFWwindow*		  m_window	 = nullptr;
	int				  m_width	 = 0;
	int				  m_height	 = 0;
	int				  m_fbWidth	 = 0;
	int				  m_fbHeight = 0;
	bool			  m_needsResize = false;
	Input			  m_input;
	std::atomic<bool> m_shouldClose{false};
};
