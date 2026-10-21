#ifndef DEBUG
#define DEBUG 0
#endif
// int main()
// {
    // Display* display = XOpenDisplay(NULL);

    // if (!display)
    // {
    //     printf("Error: no se pudo abrir X11\n");
    //     return 1;
    // }

    // if (DEBUG)
    //     printf("Hola Lore\n");
    // int screen = DefaultScreen(display);

    // int visual_attribs[] =
    // {
    //     GLX_RGBA,
    //     GLX_DOUBLEBUFFER,
    //     GLX_DEPTH_SIZE, 24,
    //     None
    // };

    // XVisualInfo* visual =
    //     glXChooseVisual(display, screen, visual_attribs);

    // if (!visual)
    // {
    //     printf("Error: no se pudo obtener un visual GLX\n");
    //     XCloseDisplay(display);
    //     return 1;
    // }

    // Colormap colormap =
    //     XCreateColormap(
    //         display,
    //         RootWindow(display, visual->screen),
    //         visual->visual,
    //         AllocNone
    //     );

    // XSetWindowAttributes attributes{};
    // attributes.colormap = colormap;
    // attributes.event_mask = ExposureMask | KeyPressMask;

    // Window window =
    //     XCreateWindow(
    //         display,
    //         RootWindow(display, visual->screen),
    //         100,
    //         100,
    //         800,
    //         600,
    //         0,
    //         visual->depth,
    //         InputOutput,
    //         visual->visual,
    //         CWColormap | CWEventMask,
    //         &attributes
    //     );

    // XStoreName(display, window, "OpenGL + X11 + GLAD");

    // XMapWindow(display, window);

    // GLXContext context =
    //     glXCreateContext(
    //         display,
    //         visual,
    //         NULL,
    //         True
    //     );

    // if (!context)
    // {
    //     printf("Error: no se pudo crear el contexto OpenGL\n");

    //     XDestroyWindow(display, window);
    //     XFreeColormap(display, colormap);
    //     XFree(visual);
    //     XCloseDisplay(display);

    //     return 1;
    // }

    // glXMakeCurrent(display, window, context);

    // // Cargar OpenGL mediante GLAD
    // if (!gladLoadGL((GLADloadfunc)glXGetProcAddressARB))
    // {
    //     printf("Error: GLAD no pudo cargar OpenGL\n");

    //     glXMakeCurrent(display, None, NULL);
    //     glXDestroyContext(display, context);
    //     XDestroyWindow(display, window);
    //     XFreeColormap(display, colormap);
    //     XFree(visual);
    //     XCloseDisplay(display);

    //     return 1;
    // }

    // printf("OpenGL: %s\n", glGetString(GL_VERSION));
    // printf("Vendor: %s\n", glGetString(GL_VENDOR));
    // printf("Renderer: %s\n", glGetString(GL_RENDERER));

    // // Limpiar
    // glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT);

    // glXSwapBuffers(display, window);

    // getchar();

    // glXMakeCurrent(display, None, NULL);
    // glXDestroyContext(display, context);

    // XDestroyWindow(display, window);
    // XFreeColormap(display, colormap);
    // XFree(visual);

    // XCloseDisplay(display);

//     return 0;
// }
#include <WWindow.hpp>
int main(void)
{
	WWindow win;

	(void)win;

	return  0;
}