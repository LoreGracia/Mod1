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

// #include <glad/gl.h>
// #include <glad/glad_glx.h>
// #include <X11/Xlib.h>
// #include <X11/keysym.h>

// #include <WWindow.hpp>
// #include <Shader.hpp>
// #include <Mesh.hpp>

// #include <cstdio>
// #include <cmath>
// #include <vector>
// #include <stdexcept>

// void rotationX(float deg, float out[16]) {
//     float r = deg * 3.14159265f / 180.f;
//     float c = std::cos(r), s = std::sin(r);
//     out[0]=1; out[1]=0; out[2]=0;  out[3]=0;
//     out[4]=0; out[5]=c; out[6]=s;  out[7]=0;
//     out[8]=0; out[9]=-s;out[10]=c; out[11]=-1;
//     out[12]=0;out[13]=0;out[14]=0; out[15]=1;
// }
// void rotationY(float deg, float out[16]) {
//     float r = deg * 3.14159265f / 180.f;
//     float c = std::cos(r), s = std::sin(r);
//     out[0]=c; out[1]=0; out[2]=-s; out[3]=0;
//     out[4]=0; out[5]=1; out[6]=0;  out[7]=0;
//     out[8]=s; out[9]=0; out[10]=c; out[11]=0;
//     out[12]=0;out[13]=0;out[14]=0; out[15]=1;
// }
// void multiply4(const float a[16], const float b[16], float out[16]) {
//     for (int col = 0; col < 4; ++col)
//         for (int row = 0; row < 4; ++row) {
//             float sum = 0.f;
//             for (int k = 0; k < 4; ++k)
//                 sum += a[k*4 + row] * b[col*4 + k];
//             out[col*4 + row] = sum;
//         }
// }

// int main() {
//     try {
//         WWindow::Config cfg;
//         cfg.title  = "Colored cube — Esc to close";
//         cfg.width  = 640;
//         cfg.height = 480;

//         WWindow win(cfg);
//         win.makeCurrent();
//         if (!gladLoadGL((GLADloadfunc)glXGetProcAddressARB)) return 1;
//         std::printf("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));

//         Shader shader;
//         if (!shader.loadFromSource(
//             "#version 330 core\n"
//             "layout(location = 0) in vec3 aPos;\n"
//             "layout(location = 1) in vec3 aColor;\n"
//             "uniform mat4 uMVP;\n"
//             "out vec3 vColor;\n"
//             "void main() {\n"
//             "    vColor = aColor;\n"
//             "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
//             "}\n",
//             "#version 330 core\n"
//             "in vec3 vColor;\n"
//             "out vec4 FragColor;\n"
//             "void main() { FragColor = vec4(vColor, 1.0); }\n"))
//         { std::fprintf(stderr, "shader failed\n"); return 1; }

//         // --- Cubo coloreado: 24 vértices, 6 caras ---
//         std::vector<float> pos = {
//             -0.5f,-0.5f, 0.5f,   0.5f,-0.5f, 0.5f,   0.5f, 0.5f, 0.5f,  -0.5f, 0.5f, 0.5f,
//              0.5f,-0.5f,-0.5f,  -0.5f,-0.5f,-0.5f,  -0.5f, 0.5f,-0.5f,   0.5f, 0.5f,-0.5f,
//              0.5f,-0.5f, 0.5f,   0.5f,-0.5f,-0.5f,   0.5f, 0.5f,-0.5f,   0.5f, 0.5f, 0.5f,
//             -0.5f,-0.5f,-0.5f,  -0.5f,-0.5f, 0.5f,  -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f,-0.5f,
//             -0.5f, 0.5f, 0.5f,   0.5f, 0.5f, 0.5f,   0.5f, 0.5f,-0.5f,  -0.5f, 0.5f,-0.5f,
//             -0.5f,-0.5f,-0.5f,   0.5f,-0.5f,-0.5f,   0.5f,-0.5f, 0.5f,  -0.5f,-0.5f, 0.5f,
//         };
//         std::vector<float> col;
//         auto pushColor = [&](float r, float g, float b) {
//             for (int i = 0; i < 4; ++i) { col.push_back(r); col.push_back(g); col.push_back(b); }
//         };
//         pushColor(1.0f, 0.2f, 0.2f);   // Front  rojo
//         pushColor(0.2f, 1.0f, 0.2f);   // Back   verde
//         pushColor(0.2f, 0.4f, 1.0f);   // Right  azul
//         pushColor(1.0f, 0.9f, 0.2f);   // Left   amarillo
//         pushColor(1.0f, 0.2f, 1.0f);   // Top    magenta
//         pushColor(0.2f, 1.0f, 1.0f);   // Bottom cian

//         std::vector<uint32_t> idx;
//         for (uint32_t f = 0; f < 6; ++f) {
//             uint32_t b = f * 4;
//             idx.insert(idx.end(), { b+0, b+1, b+2,  b+2, b+3, b+0 });
//         }

//         Mesh cub;
//         cub.upload(pos, col, idx);

//         glEnable(GL_DEPTH_TEST);
//         glDepthFunc(GL_LESS);
//         glEnable(GL_CULL_FACE);          // opcional: no dibuja caras traseras
//         glCullFace(GL_BACK);
//         glFrontFace(GL_CCW);             // los vértices están en CCW visto desde fuera

//         glViewport(0, 0, win.width(), win.height());

//         float aX = 0.f, aY = 0.f;

//         while (!win.shouldClose()) {
//             if (!win.pollEvents()) break;

//             int rw, rh;
//             if (win.consumeResize(rw, rh)) glViewport(0, 0, rw, rh);

//             glClearColor(0.10f, 0.15f, 0.20f, 1.0f);
//             glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//             aX += 0.7f; aY += 1.5f;
//             if (aX > 360.f) aX -= 360.f;
//             if (aY > 360.f) aY -= 360.f;

//             float rx[16];
// 			// float rx[16], ry[16], mvp[16];
//             rotationX(aX, rx);
//             // rotationY(aY, ry);
//             // multiply4(ry, rx, mvp);       // primero X, luego Y

//             shader.use();
//             shader.setMat4("uMVP", rx);
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

// #include <glad/gl.h>
// #include <glad/glad_glx.h>
// #include <X11/Xlib.h>
// #include <X11/keysym.h>

// #include <WWindow.hpp>
// #include <Shader.hpp>
// #include <Mesh.hpp>
// #include <Camera.hpp>
// #include <Transform.hpp>
// #include <Entity.hpp>
// #include <Scene.hpp>

// #include <glm.hpp>
// #include <type_ptr.hpp>

// #include <cstdio>
// #include <cmath>
// #include <memory>
// #include <vector>
// #include <stdexcept>

// // ── Datos del cubo coloreado (24 vértices, 6 caras) ──────────────────
// static void uploadColoredCube(Mesh& m) {
//     std::vector<float> pos = {
//         -0.5f,-0.5f, 0.5f,   0.5f,-0.5f, 0.5f,   0.5f, 0.5f, 0.5f,  -0.5f, 0.5f, 0.5f,
//          0.5f,-0.5f,-0.5f,  -0.5f,-0.5f,-0.5f,  -0.5f, 0.5f,-0.5f,   0.5f, 0.5f,-0.5f,
//          0.5f,-0.5f, 0.5f,   0.5f,-0.5f,-0.5f,   0.5f, 0.5f,-0.5f,   0.5f, 0.5f, 0.5f,
//         -0.5f,-0.5f,-0.5f,  -0.5f,-0.5f, 0.5f,  -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f,-0.5f,
//         -0.5f, 0.5f, 0.5f,   0.5f, 0.5f, 0.5f,   0.5f, 0.5f,-0.5f,  -0.5f, 0.5f,-0.5f,
//         -0.5f,-0.5f,-0.5f,   0.5f,-0.5f,-0.5f,   0.5f,-0.5f, 0.5f,  -0.5f,-0.5f, 0.5f,
//     };
//     std::vector<float> col;
//     auto pushColor = [&](float r, float g, float b) {
//         for (int i = 0; i < 4; ++i) { col.push_back(r); col.push_back(g); col.push_back(b); }
//     };
//     pushColor(1.0f, 0.2f, 0.2f);   // Front  rojo
//     pushColor(0.2f, 1.0f, 0.2f);   // Back   verde
//     pushColor(0.2f, 0.4f, 1.0f);   // Right  azul
//     pushColor(1.0f, 0.9f, 0.2f);   // Left   amarillo
//     pushColor(1.0f, 0.2f, 1.0f);   // Top    magenta
//     pushColor(0.2f, 1.0f, 1.0f);   // Bottom cian

//     std::vector<uint32_t> idx;
//     for (uint32_t f = 0; f < 6; ++f) {
//         uint32_t b = f * 4;
//         idx.insert(idx.end(), { b+0, b+1, b+2,  b+2, b+3, b+0 });
//     }
//     m.upload(pos, col, idx);
// }

// int main() {
//     try {
//         // ─── Ventana ──────────────────────────────────────────────────
//         WWindow::Config cfg;
//         cfg.title  = "Entity test — Esc to close";
//         cfg.width  = 900;
//         cfg.height = 600;

//         WWindow win(cfg);
//         win.makeCurrent();
//         if (!gladLoadGL((GLADloadfunc)glXGetProcAddressARB)) {
//             std::fprintf(stderr, "gladLoadGL failed\n");
//             return 1;
//         }
//         std::printf("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));

//         // ─── Shader (uno solo, compartido por las 3 entidades) ────────
//         auto shader = std::make_shared<Shader>();
// 		if (!shader->loadFromSource(
// 			"#version 330 core\n"
// 			"layout(location = 0) in vec3 aPos;\n"
// 			"layout(location = 1) in vec3 aColor;\n"
// 			"uniform mat4 uModel;\n"
// 			"uniform mat4 uViewProj;\n"
// 			"out vec3 vColor;\n"
// 			"void main() {\n"
// 			"    vColor = aColor;\n"
// 			"    gl_Position = uViewProj * uModel * vec4(aPos, 1.0);\n"
// 			"}\n",
// 			"#version 330 core\n"
// 			"in vec3 vColor;\n"
// 			"out vec4 FragColor;\n"
// 			"void main() { FragColor = vec4(vColor, 1.0); }\n"))
// 		{
// 			std::fprintf(stderr, "shader failed\n");
// 			return 1;
// 		}

//         // ─── Mesh (uno solo, compartido por las 3 entidades) ─────────
//         auto cubeMesh = std::make_shared<Mesh>();
//         uploadColoredCube(*cubeMesh);

//         // ─── Tres entidades con transformaciones distintas ───────────
//         auto e1 = std::make_shared<Entity>(cubeMesh, shader);
//         e1->transform().position = {-2.f, 0.f, 0.f};   // izquierda
//         e1->transform().scale    = {0.8f, 0.8f, 0.8f};

//         auto e2 = std::make_shared<Entity>(cubeMesh, shader);
//         e2->transform().position = { 0.f, 0.f, 0.f};   // centro
//         // sin escala → 1,1,1

//         auto e3 = std::make_shared<Entity>(cubeMesh, shader);
//         e3->transform().position = { 2.f, 0.f, 0.f};   // derecha
//         e3->transform().scale    = {1.2f, 1.2f, 1.2f};

//         // ─── Estado GL ───────────────────────────────────────────────
//         glEnable(GL_DEPTH_TEST);
//         glEnable(GL_CULL_FACE);
//         glCullFace(GL_BACK);
//         glFrontFace(GL_CCW);

//         // ─── Cámara ──────────────────────────────────────────────────
//         Camera cam({0.f, 2.f, 8.f}, {0.f, 0.f, 0.f});

//         glViewport(0, 0, win.width(), win.height());

//         float t = 0.f;

//         // ─── Bucle ───────────────────────────────────────────────────
//         while (!win.shouldClose()) {
//             if (!win.pollEvents()) break;

//             int rw, rh;
//             if (win.consumeResize(rw, rh))
//                 glViewport(0, 0, rw, rh);

//             glClearColor(0.10f, 0.15f, 0.20f, 1.0f);
//             glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//             t += 0.016f;

//             // Cada entidad se transforma de forma independiente
//             e1->transform().rotate(45.f * 0.016f, {1.f, 0.f, 0.f});
//             e2->transform().rotate(60.f * 0.016f, {0.f, 1.f, 0.f});
//             e3->transform().rotate(45.f * 0.016f, {0.f, 0.f, 1.f});
//             // e3 además late en Y (escala sinusoidal)
//             e3->transform().scale = {1.2f, 1.2f + 0.4f * std::sin(t * 3.f), 1.2f};

//             // ViewProjection común a todas las entidades
//             float aspect = (float)win.width() / (float)win.height();
//             glm::mat4 viewProj = cam.projection(aspect) * cam.view();

//             // Un solo draw por entidad
//             e1->render(viewProj);
//             e2->render(viewProj);
//             e3->render(viewProj);

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
#include <X11/keysym.h>

#include <Environment.hpp>
#include <WWindow.hpp>
#include <Shader.hpp>
#include <Mesh.hpp>
#include <Renderer.hpp>
#include <Camera.hpp>
#include <Entity.hpp>
#include <Transform.hpp>

#include <glm.hpp>
#include <gtc/type_ptr.hpp>

#include <cstdio>
#include <memory>
#include <vector>

int main() {
    try {
        Environment env;

        WWindow::Config cfg;
        cfg.title  = "Input + Renderer — WASD / mouse / scroll";
        cfg.width  = 900;
        cfg.height = 600;
        env.addWindow(cfg);   // solo una ventana; usamos env por comodidad

        struct State {
            Camera   cam;
            Renderer renderer;
            std::shared_ptr<Entity> cube;
        } st;

        std::shared_ptr<Shader> shader;
        std::shared_ptr<Mesh>   cubeMesh;

        env.setInit([&](WWindow& w) {
            // ── Recursos (una vez) ─────────────────────────────────
            shader = std::make_shared<Shader>();
            shader->loadFromSource(
                "#version 330 core\n"
                "layout(location = 0) in vec3 aPos;\n"
                "layout(location = 1) in vec3 aColor;\n"
                "uniform mat4 uModel;\n"
                "uniform mat4 uViewProj;\n"
                "out vec3 vColor;\n"
                "void main(){ vColor=aColor; gl_Position=uViewProj*uModel*vec4(aPos,1); }\n",
                "#version 330 core\n"
                "in vec3 vColor;\n"
                "out vec4 FragColor;\n"
                "void main(){ FragColor = vec4(vColor,1); }\n");

            cubeMesh = std::make_shared<Mesh>();
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
            pushColor(1,0.2f,0.2f); pushColor(0.2f,1,0.2f);
            pushColor(0.2f,0.4f,1); pushColor(1,0.9f,0.2f);
            pushColor(1,0.2f,1);    pushColor(0.2f,1,1);
            std::vector<uint32_t> idx;
            for (uint32_t f = 0; f < 6; ++f) {
                uint32_t b = f * 4;
                idx.insert(idx.end(), { b+0, b+1, b+2,  b+2, b+3, b+0 });
            }
            cubeMesh->upload(pos, col, idx);

            // ── Estado por ventana ──────────────────────────────────
            st.cube = std::make_shared<Entity>(cubeMesh, shader);
            st.cam  = Camera({4.f, 2.f, 4.f}, {0.f, 0.f, 0.f});

            // Renderer: config inicial (aquí es donde cachea y aplica)
            st.renderer.depthTest(true);
            st.renderer.depthFunc(GL_LESS);
            st.renderer.depthWrite(true);
            st.renderer.cullFace(true);
            st.renderer.cullMode(GL_BACK);
            st.renderer.frontFace(GL_CCW);
            st.renderer.blend(false);

            (void)w;
        });

        env.setUpdate([&](WWindow& w, float dt) {
            Input& in = w.input();

            // ── Cámara: WASD mueve, rueda hace zoom ────────────────
            float speed = 3.f * dt;
            glm::vec3 fwd  = st.cam.forward();
            glm::vec3 right = st.cam.right();

            glm::vec3 move{0.f};
            if (in.isKeyDown(XK_w)) move += fwd;
            if (in.isKeyDown(XK_s)) move -= fwd;
            if (in.isKeyDown(XK_d)) move += right;
            if (in.isKeyDown(XK_a)) move -= right;
            if (glm::length(move) > 1e-6f)
                st.cam.move(glm::normalize(move) * speed);

            if (in.scrollDelta() != 0.f)
                st.cam.orbit(0.f, 0.f, -in.scrollDelta() * 0.5f);

            // ── Rotación del cubo con flechas ──────────────────────
            float rotSpeed = 60.f * dt;
            if (in.isKeyDown(XK_Left))  st.cube->transform().rotate( rotSpeed, {0,1,0});
            if (in.isKeyDown(XK_Right)) st.cube->transform().rotate(-rotSpeed, {0,1,0});
            if (in.isKeyDown(XK_Up))    st.cube->transform().rotate( rotSpeed, {1,0,0});
            if (in.isKeyDown(XK_Down))  st.cube->transform().rotate(-rotSpeed, {1,0,0});

            // ── Saltar con Space (flanco) ──────────────────────────
            if (in.isKeyPressed(XK_space)) {
                std::printf("Jump! (isKeyPressed XK_space)\n");
            }
        });

        env.setRender([&](WWindow& w) {
            int rw, rh;
            if (w.consumeResize(rw, rh))
                st.renderer.setViewport(rw, rh);
            else
                st.renderer.setViewport(w.width(), w.height());

            st.renderer.setClearColor(0.10f, 0.15f, 0.20f, 1.f);
            st.renderer.clear(true, true, false);

            glm::mat4 vp = st.cam.projection(w.aspect()) * st.cam.view();
            st.cube->render(vp);
        });

        env.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "exception: %s\n", e.what());
        return 1;
    }
    return 0;
}