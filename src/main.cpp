#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <stdio.h>
#ifndef DEBUG
	#define DEBUG 0
#endif

#include<Input.hpp>

typedef GLXContext (*PFNGLCREATECONTEXTATTRIBSARBPROC)
    (Display*, GLXFBConfig, GLXContext, Bool, const int*);

int main() {
	if (DEBUG)
		printf("Hola Lore\n");
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "no display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    int visual_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,  GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER, True,
        None
    };
    int fbcount; 
    GLXFBConfig* fbc = glXChooseFBConfig(dpy, screen, visual_attribs, &fbcount);
    if (!fbc || !fbcount) return 1;
    XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbc[0]);

    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

    Window win = XCreateWindow(dpy, root, 0, 0, 1000, 600, 0, vi->depth,
                               InputOutput, vi->visual,
                               CWColormap | CWEventMask, &swa);
    XStoreName(dpy, win, "OpenGL 4.6");
    XMapWindow(dpy, win);

    auto glXCreateContextAttribsARB = (PFNGLCREATECONTEXTATTRIBSARBPROC)
        glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    int ctx_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 6,
        GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        None
    };
    GLXContext ctx = glXCreateContextAttribsARB(dpy, fbc[0], NULL, True, ctx_attribs);
    if (!ctx) { fprintf(stderr, "no context\n"); return 1; }
    glXMakeCurrent(dpy, win, ctx);

    printf("GL_VERSION:  %s\n", glGetString(GL_VERSION));
    printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));

    Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wmDelete, 1);

    bool running = true;
    while (running) {
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            if (ev.type == DestroyNotify ||
               (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wmDelete) ||
               (ev.type == KeyPress && ev.xkey.keycode == XKeysymToKeycode(dpy, XK_Escape)))
                running = false;
        }
        glClearColor(0.15f, 0.20f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(dpy, win);
    }
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, ctx);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
