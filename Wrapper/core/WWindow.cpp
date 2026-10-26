#include <WWindow.hpp>
#include <stdexcept>
#include <X11/XKBlib.h>

namespace
{
	int fbAttribs[] = {
		GLX_X_RENDERABLE,  True,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE,   GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_RED_SIZE,      8,
		GLX_GREEN_SIZE,    8,
		GLX_BLUE_SIZE,     8,
		GLX_ALPHA_SIZE,    8,
		GLX_DEPTH_SIZE,    24,
		GLX_STENCIL_SIZE,  8,
		GLX_DOUBLEBUFFER,  True,
		None
	};
}

WWindow::WWindow(Config &conf)
{
	m_display = XOpenDisplay(nullptr);
	Bool	supported = False;

	XkbSetDetectableAutoRepeat(m_display, True, &supported);

	if (!m_display)
		throw std::runtime_error("XOpenDisplay faile");
	
	m_screen = DefaultScreen(m_display);

	if (!gladLoadGLX(m_display, m_screen))
		throw std::runtime_error("gladLoadGLX failed");

	int	fbCount = 0;

	GLXFBConfig	*configs =  glXChooseFBConfig(m_display, m_screen, fbAttribs, \
		&fbCount);
	if (!configs || !fbCount)
		throw std::runtime_error("no FBConfig");

	m_fbconfig = *configs;

	XVisualInfo *vi = glXGetVisualFromFBConfig(m_display, m_fbconfig);

	if (!vi) {
		XFree(configs);
		throw std::runtime_error("no visual");
	}

	m_colormap = XCreateColormap(m_display, RootWindow(m_display, \
		vi->screen), vi->visual, AllocNone);

	XSetWindowAttributes swa{};

	swa.colormap = m_colormap;
	swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
					StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
					PointerMotionMask;
	swa.background_pixmap = None;
	swa.border_pixel = 0;

	m_xwindow = XCreateWindow(
		m_display,
		RootWindow(m_display, vi->screen),
		conf.x, conf.y,
		conf.width, conf.height,
		0, vi->depth, InputOutput, vi->visual,
		CWColormap | CWEventMask | CWBackPixmap | CWBorderPixel,
		&swa);

	if (!m_xwindow) {
		XFreeColormap(m_display, m_colormap);
		XFree(vi);
		XFree(configs);
		XCloseDisplay(m_display);
		throw std::runtime_error("XCreateWindow failed");
	}

	m_width = conf.width;
	m_height = conf.height;

	XStoreName(m_display, m_xwindow, conf.title.c_str());
	m_wmDelete = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(m_display, m_xwindow, &m_wmDelete, 1);

	int ctxAttribs[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, conf.glMajor,
		GLX_CONTEXT_MINOR_VERSION_ARB, conf.glMinor,
		GLX_CONTEXT_PROFILE_MASK_ARB,
		conf.coreProfile
			? GLX_CONTEXT_CORE_PROFILE_BIT_ARB
			: GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
		None
	};

	m_context = glXCreateContextAttribsARB(m_display, m_fbconfig, \
		conf.shareContext, True, ctxAttribs);

	if (!m_context) {
		XDestroyWindow(m_display, m_xwindow);
		XFreeColormap(m_display, m_colormap);
		XFree(vi);
		XFree(configs);
		XCloseDisplay(m_display);
		throw std::runtime_error("glXCreateContextAttribsARB failed");
	}

	XSync(m_display, False);

	XFree(vi);
	XFree(configs);

	XMapWindow(m_display, m_xwindow);
}

WWindow::~WWindow()
{
	if (m_display && m_context) {
		glXMakeCurrent(m_display, None, nullptr);
		glXDestroyContext(m_display, m_context);
	}

	if (m_display && m_xwindow)
		XDestroyWindow(m_display, m_xwindow);
	if (m_display && m_colormap)
		XFreeColormap(m_display, m_colormap);
	if (m_display)
		XCloseDisplay(m_display);
}


void WWindow::makeCurrent() {
	glXMakeCurrent(m_display, m_xwindow, m_context);
}

void WWindow::releaseCurrent() {
	glXMakeCurrent(m_display, None, nullptr);
}

void WWindow::swapBuffers() {
	glXSwapBuffers(m_display, m_xwindow);
}

bool WWindow::pollEvents()
{
	m_input.beginFrame();

	while (XPending(m_display)) {
		XEvent ev;

		XNextEvent(m_display, &ev);
		m_input.handleEvent(ev);

		switch (ev.type) {
			case ClientMessage:
				if ((Atom)ev.xclient.data.l[0] == m_wmDelete) {
					m_shouldClose.store(true);
				}
				break;
			case KeyPress:
				if (XLookupKeysym(&ev.xkey, 0) == XK_Escape)
					m_shouldClose.store(true);
				break;
			case ConfigureNotify:
				if (ev.xconfigure.width != m_width || ev.xconfigure.height != m_height) {
					m_width  = ev.xconfigure.width;
					m_height = ev.xconfigure.height;
					m_needsResize = true;
				}
				break;
			case DestroyNotify:
				m_shouldClose.store(true);
				break;
		}
	}
	return !m_shouldClose.load();
}

bool WWindow::consumeResize(int &w, int &h)
{
	if (!m_needsResize)
		return false;
	w = m_width;
	h = m_height;
	m_needsResize = false;
	return true;
}