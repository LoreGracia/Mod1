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


// #include <glad/gl.h>
// #include <glad/glad_glx.h>
// #include <X11/Xlib.h>
// #include <X11/keysym.h>

// #include <WWindow.hpp>
// #include <Shader.hpp>
// #include <Mesh.hpp>

// #include <cstdio>
// #include <cmath>
// #include <stdexcept>

// // Rotación alrededor de Y, en grados. Salida: 16 floats column-major.
// void rotationY(float deg, float out[16]) {
//     float r = deg * 3.14159265f / 180.f;
//     float c = std::cos(r), s = std::sin(r);
//     out[0]= c; out[1]= 0; out[2]=-s; out[3]= 0;
//     out[4]= 0; out[5]= 1; out[6]= 0; out[7]= 0;
//     out[8]= s; out[9]= 0; out[10]= c; out[11]= 0;
//     out[12]=0; out[13]= 0; out[14]= 0; out[15]= 1;
// }

// // Rotación alrededor de X, en grados. Salida: 16 floats column-major.
// void rotationX(float deg, float out[16]) {
//     float r = deg * 3.14159265f / 180.f;
//     float c = std::cos(r), s = std::sin(r);
//     out[0] = 1; out[1] = 0; out[2] = 0;  out[3]  = 0;
//     out[4] = 0; out[5] = c; out[6] = s;  out[7]  = 0;
//     out[8] = 0; out[9] =-s; out[10]= c;  out[11] = 0;
//     out[12]= 0; out[13]= 0; out[14]= 0;  out[15] = 1;
// }

// int main() {
//     try {
//         WWindow::Config cfg;
//         cfg.title  = "Cube test — Esc to close";
//         cfg.width  = 640;
//         cfg.height = 640;

//         WWindow win(cfg);
//         win.makeCurrent();

//         if (!gladLoadGL((GLADloadfunc)glXGetProcAddressARB)) {
//             std::fprintf(stderr, "gladLoadGL failed\n");
//             return 1;
//         }
//         std::printf("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));

//         // ── Shader con uniform MVP ──────────────────────────────────────────
//         Shader shader;
//         if (!shader.loadFromSource(
//             "#version 330 core\n"
//             "layout(location = 0) in vec3 aPos;\n"
//             "uniform mat4 uMVP;\n"
//             "void main() {\n"
//             "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
//             "}\n",
//             "#version 330 core\n"
//             "out vec4 FragColor;\n"
//             "void main() {\n"
//             "    FragColor = vec4(1.0, 0.55, 0.2, 1.0);\n"
//             "}\n"))
//         {
//             std::fprintf(stderr, "shader failed\n");
//             return 1;
//         }

//         // ── Mesh: ASIGNAR el retorno, no descartarlo ────────────────────────
//         Mesh cub = Mesh::cube();          // ✅ en vez de `cub.cube();`

//         // ── Estado GL que faltaba ───────────────────────────────────────────
//         glEnable(GL_DEPTH_TEST);          // ✅ imprescindible para un cubo
//         glDepthFunc(GL_LESS);

//         glViewport(0, 0, win.width(), win.height());

//         float angle = 0.f;

//         while (!win.shouldClose()) {
//             if (!win.pollEvents()) break;

//             int rw, rh;
//             if (win.consumeResize(rw, rh))
//                 glViewport(0, 0, rw, rh);

//             // Limpiar color Y profundidad
//             glClearColor(0.10f, 0.15f, 0.20f, 1.0f);
//             glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);   // ✅

//             angle += 1.0f;
//             if (angle > 360.f) angle -= 360.f;

//             float mvp[16];
// 			rotationY(angle, mvp);

//             shader.use();
//             shader.setMat4("uMVP", mvp);
//             cub.draw();

//             win.swapBuffers();
//         }

//         win.releaseCurrent();
//     } catch (const std::exception& e) {
//         std::fprintf(stderr, "exception: %s\n", e.what());
//         return 1;
//     }
//     return 0;
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
#include <vector>
#include <stdexcept>

void rotationX(float deg, float out[16]) {
    float r = deg * 3.14159265f / 180.f;
    float c = std::cos(r), s = std::sin(r);
    out[0]=1; out[1]=0; out[2]=0;  out[3]=0;
    out[4]=0; out[5]=c; out[6]=s;  out[7]=0;
    out[8]=0; out[9]=-s;out[10]=c; out[11]=0;
    out[12]=0;out[13]=0;out[14]=0; out[15]=1;
}
void rotationY(float deg, float out[16]) {
    float r = deg * 3.14159265f / 180.f;
    float c = std::cos(r), s = std::sin(r);
    out[0]=c; out[1]=0; out[2]=-s; out[3]=0;
    out[4]=0; out[5]=1; out[6]=0;  out[7]=0;
    out[8]=s; out[9]=0; out[10]=c; out[11]=0;
    out[12]=0;out[13]=0;out[14]=0; out[15]=1;
}
void multiply4(const float a[16], const float b[16], float out[16]) {
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) {
            float sum = 0.f;
            for (int k = 0; k < 4; ++k)
                sum += a[k*4 + row] * b[col*4 + k];
            out[col*4 + row] = sum;
        }
}

int main() {
    try {
        WWindow::Config cfg;
        cfg.title  = "Colored cube — Esc to close";
        cfg.width  = 640;
        cfg.height = 480;

        WWindow win(cfg);
        win.makeCurrent();
        if (!gladLoadGL((GLADloadfunc)glXGetProcAddressARB)) return 1;
        std::printf("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));

        Shader shader;
        if (!shader.loadFromSource(
            "#version 330 core\n"
            "layout(location = 0) in vec3 aPos;\n"
            "layout(location = 1) in vec3 aColor;\n"
            "uniform mat4 uMVP;\n"
            "out vec3 vColor;\n"
            "void main() {\n"
            "    vColor = aColor;\n"
            "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
            "}\n",
            "#version 330 core\n"
            "in vec3 vColor;\n"
            "out vec4 FragColor;\n"
            "void main() { FragColor = vec4(vColor, 1.0); }\n"))
        { std::fprintf(stderr, "shader failed\n"); return 1; }

        // --- Cubo coloreado: 24 vértices, 6 caras ---
        std::vector<float> pos = {
            -0.5f,-0.5f, 0.5f,   0.5f,-0.5f, 0.5f,   0.5f, 0.5f, 0.5f,  -0.5f, 0.5f, 0.5f,
             0.5f,-0.5f,-0.5f,  -0.5f,-0.5f,-0.5f,  -0.5f, 0.5f,-0.5f,   0.5f, 0.5f,-0.5f,
             0.5f,-0.5f, 0.5f,   0.5f,-0.5f,-0.5f,   0.5f, 0.5f,-0.5f,   0.5f, 0.5f, 0.5f,
            -0.5f,-0.5f,-0.5f,  -0.5f,-0.5f, 0.5f,  -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f,-0.5f,
            -0.5f, 0.5f, 0.5f,   0.5f, 0.5f, 0.5f,   0.5f, 0.5f,-0.5f,  -0.5f, 0.5f,-0.5f,
            -0.5f,-0.5f,-0.5f,   0.5f,-0.5f,-0.5f,   0.5f,-0.5f, 0.5f,  -0.5f,-0.5f, 0.5f,
        };
        std::vector<float> col;
        auto pushColor = [&](float r, float g, float b) {
            for (int i = 0; i < 4; ++i) { col.push_back(r); col.push_back(g); col.push_back(b); }
        };
        pushColor(1.0f, 0.2f, 0.2f);   // Front  rojo
        pushColor(0.2f, 1.0f, 0.2f);   // Back   verde
        pushColor(0.2f, 0.4f, 1.0f);   // Right  azul
        pushColor(1.0f, 0.9f, 0.2f);   // Left   amarillo
        pushColor(1.0f, 0.2f, 1.0f);   // Top    magenta
        pushColor(0.2f, 1.0f, 1.0f);   // Bottom cian

        std::vector<uint32_t> idx;
        for (uint32_t f = 0; f < 6; ++f) {
            uint32_t b = f * 4;
            idx.insert(idx.end(), { b+0, b+1, b+2,  b+2, b+3, b+0 });
        }

        Mesh cub;
        cub.upload(pos, col, idx);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);          // opcional: no dibuja caras traseras
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);             // los vértices están en CCW visto desde fuera

        glViewport(0, 0, win.width(), win.height());

        float aX = 0.f, aY = 0.f;

        while (!win.shouldClose()) {
            if (!win.pollEvents()) break;

            int rw, rh;
            if (win.consumeResize(rw, rh)) glViewport(0, 0, rw, rh);

            glClearColor(0.10f, 0.15f, 0.20f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            aX += 0.7f; aY += 1.5f;
            if (aX > 360.f) aX -= 360.f;
            if (aY > 360.f) aY -= 360.f;

            float rx[16];
			// float rx[16], ry[16], mvp[16];
            rotationX(aX, rx);
            // rotationY(aY, ry);
            // multiply4(ry, rx, mvp);       // primero X, luego Y

            shader.use();
            shader.setMat4("uMVP", rx);
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
