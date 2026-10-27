#pragma once
#include <glad/glad_glx.h>

#include <Input.hpp>
#include <X11/Xlib.h>
#include <atomic>
#include <string>

class WWindow
{
private:
	WWindow() {};
	Display*		  m_display	 = nullptr;
	Window			  m_xwindow	 = 0;
	GLXContext		  m_context	 = nullptr;
	Colormap		  m_colormap = 0;
	GLXFBConfig		  m_fbconfig = nullptr;
	Atom			  m_wmDelete = 0;
	int				  m_screen	 = 0;
	int				  m_width = 0, m_height = 0;
	bool			  m_needsResize = false;
	Input			  m_input;
	std::atomic<bool> m_shouldClose{false};

public:
	struct Config
	{
		std::string title = "Default Name";
		int			width = 800, height = 600;
		int			x = 0, y = 0;
		GLXContext	shareContext = nullptr;
		int			glMajor = 4, glMinor = 6;
		bool		coreProfile = false;
	};
	explicit WWindow(Config& conf);
	~WWindow();
	WWindow(const WWindow&)			   = delete;
	WWindow& operator=(const WWindow&) = delete;

	void makeCurrent();
	void releaseCurrent();
	void swapBuffers();
	bool pollEvents();

	Window	   xwindow() const { return m_xwindow; }
	GLXContext context() const { return m_context; }
	int		   width() const { return m_width; }
	int		   height() const { return m_height; }
	float	   aspect() const { return float(m_width) / float(m_height); }
	bool	   shouldClose() const { return m_shouldClose.load(); }
	void	   requestClose() { m_shouldClose.store(true); }
	bool	   consumeResize(int& w, int& h);

	Display* display() const { return m_display; }
	int		 screen() const { return m_screen; }

	Input&		 input() { return m_input; }
	const Input& input() const { return m_input; }
};