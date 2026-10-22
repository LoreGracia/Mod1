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
// #include <glad/gl.h>
// #include <glad/glad_glx.h>
// #include <X11/Xlib.h>
// #include <X11/keysym.h>

// #include <WWindow.hpp>
// #include <Shader.hpp>
// #include <Mesh.hpp>

// #include <cstdio>
// #include <stdexcept>

// int main() {
// 	try {
// 		// ── 1. Ventana (ya tienes esto funcionando) ─────────────────────────
// 		WWindow::Config cfg;
// 		cfg.title  = "Triangle test — Esc to close";
// 		cfg.width  = 640;
// 		cfg.height = 480;

// 		WWindow win(cfg);
// 		win.makeCurrent();

// 		if (!gladLoadGL((GLADloadfunc)glXGetProcAddressARB)) {
// 			std::fprintf(stderr, "gladLoadGL failed\n");
// 			return 1;
// 		}
// 		std::printf("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));

// 		// ── 2. Shader ───────────────────────────────────────────────────────
// 		Shader shader;
// 		if (!shader.loadFromSource(
// 			// vertex
// 			"#version 330 core\n"
// 			"layout(location = 0) in vec3 aPos;\n"
// 			"void main() {\n"
// 			"    gl_Position = vec4(aPos, 1.0);\n"
// 			"}\n",
// 			// fragment
// 			"#version 330 core\n"
// 			"out vec4 FragColor;\n"
// 			"void main() {\n"
// 			"    FragColor = vec4(1.0, 0.55, 0.2, 1.0);\n"
// 			"}\n"))
// 		{
// 			std::fprintf(stderr, "shader failed\n");
// 			return 1;
// 		}

// 		// ── 3. Mesh ─────────────────────────────────────────────────────────
// 		// Mesh tri;
// 		// tri.upload(
// 		// 	{  //  x      y     z
// 		// 	-0.5f, -0.5f, 0.0f,
// 		// 		0.5f, -0.5f, 0.0f,
// 		// 		0.0f,  0.5f, 0.0f
// 		// 	},
// 		// 	{ 0, 1, 2 }
// 		// );

// 		Mesh cub = Mesh::cube();

// 		// ── 4. Primer frame ─────────────────────────────────────────────────
// 		glViewport(0, 0, win.width(), win.height());

// 		while (!win.shouldClose()) {
// 			if (!win.pollEvents()) break;

// 			int rw, rh;
// 			if (win.consumeResize(rw, rh))
// 				glViewport(0, 0, rw, rh);

// 			glClearColor(0.10f, 0.15f, 0.20f, 1.0f);
// 			glClear(GL_COLOR_BUFFER_BIT);

// 			shader.use();
// 			cub.draw();

// 			win.swapBuffers();
// 		}

// 		win.releaseCurrent();
// 	} catch (const std::exception& e) {
// 		std::fprintf(stderr, "exception: %s\n", e.what());
// 		return 1;
// 	}
// 	return 0;
// }


#include <glad/gl.h>
#include <glad/glad_glx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <WWindow.hpp>
#include <Shader.hpp>
#include <Mesh.hpp>

#include <cstdio>
#include <cmath>
#include <stdexcept>

// Rotación alrededor de Y, en grados. Salida: 16 floats column-major.
void rotationY(float deg, float out[16]) {
    float r = deg * 3.14159265f / 180.f;
    float c = std::cos(r), s = std::sin(r);
    out[0]= c; out[1]= 0; out[2]=-s; out[3]= 0;
    out[4]= 0; out[5]= 1; out[6]= 0; out[7]= 0;
    out[8]= s; out[9]= 0; out[10]= c; out[11]= 0;
    out[12]=0; out[13]= 0; out[14]= 0; out[15]= 1;
}

// Rotación alrededor de X, en grados. Salida: 16 floats column-major.
void rotationX(float deg, float out[16]) {
    float r = deg * 3.14159265f / 180.f;
    float c = std::cos(r), s = std::sin(r);
    out[0] = 1; out[1] = 0; out[2] = 0;  out[3]  = 0;
    out[4] = 0; out[5] = c; out[6] = s;  out[7]  = 0;
    out[8] = 0; out[9] =-s; out[10]= c;  out[11] = 0;
    out[12]= 0; out[13]= 0; out[14]= 0;  out[15] = 1;
}

int main() {
    try {
        WWindow::Config cfg;
        cfg.title  = "Cube test — Esc to close";
        cfg.width  = 640;
        cfg.height = 640;

        WWindow win(cfg);
        win.makeCurrent();

        if (!gladLoadGL((GLADloadfunc)glXGetProcAddressARB)) {
            std::fprintf(stderr, "gladLoadGL failed\n");
            return 1;
        }
        std::printf("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));

        // ── Shader con uniform MVP ──────────────────────────────────────────
        Shader shader;
        if (!shader.loadFromSource(
            "#version 330 core\n"
            "layout(location = 0) in vec3 aPos;\n"
            "uniform mat4 uMVP;\n"
            "void main() {\n"
            "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
            "}\n",
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "void main() {\n"
            "    FragColor = vec4(1.0, 0.55, 0.2, 1.0);\n"
            "}\n"))
        {
            std::fprintf(stderr, "shader failed\n");
            return 1;
        }

        // ── Mesh: ASIGNAR el retorno, no descartarlo ────────────────────────
        Mesh cub = Mesh::cube();          // ✅ en vez de `cub.cube();`

        // ── Estado GL que faltaba ───────────────────────────────────────────
        glEnable(GL_DEPTH_TEST);          // ✅ imprescindible para un cubo
        glDepthFunc(GL_LESS);

        glViewport(0, 0, win.width(), win.height());

        float angle = 0.f;

        while (!win.shouldClose()) {
            if (!win.pollEvents()) break;

            int rw, rh;
            if (win.consumeResize(rw, rh))
                glViewport(0, 0, rw, rh);

            // Limpiar color Y profundidad
            glClearColor(0.10f, 0.15f, 0.20f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);   // ✅

            angle += 1.0f;
            if (angle > 360.f) angle -= 360.f;

            float mvp[16];
			rotationY(angle, mvp);

            shader.use();
            shader.setMat4("uMVP", mvp);
            cub.draw();

            win.swapBuffers();
        }

        win.releaseCurrent();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "exception: %s\n", e.what());
        return 1;
    }
    return 0;
}