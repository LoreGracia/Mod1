#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <glad/gl.h>

#include <Camera.hpp>
#include <Entity.hpp>
#include <Environment.hpp>
#include <Input.hpp>
#include <Mesh.hpp>
#include <Renderer.hpp>
#include <Shader.hpp>
#include <WWindow.hpp>
#include <cstdint>
#include <cstdio>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <memory>
#include <scene/Transform.hpp>
#include <vector>

// int main()
// {
// 	try {
// 		Environment env;

// 		WWindow::Config cfg;
// 		cfg.title		= "Input + Renderer — WASD / mouse / scroll";
// 		cfg.width		= 900;
// 		cfg.height		= 600;
// 		cfg.glMajor		= 3;
// 		cfg.glMinor		= 3;
// 		cfg.coreProfile = true;
// 		cfg.vsync		= true;
// 		env.addWindow(cfg);	 // solo una ventana; usamos env por comodidad

// 		struct State
// 		{
// 			Camera					cam;
// 			Renderer				renderer;
// 			std::shared_ptr<Entity> cube;
// 		} st;

// 		std::shared_ptr<Shader> shader;
// 		std::shared_ptr<Mesh>	cubeMesh;

// 		env.setInit([&](WWindow& w) {
// 			// ── Recursos (una vez) ─────────────────────────────────
// 			shader = std::make_shared<Shader>();
// 			shader->loadFromSource(
// 				"#version 330 core\n"
// 				"layout(location = 0) in vec3 aPos;\n"
// 				"layout(location = 1) in vec3 aColor;\n"
// 				"uniform mat4 uModel;\n"
// 				"uniform mat4 uViewProj;\n"
// 				"out vec3 vColor;\n"
// 				"void main(){ vColor=aColor; gl_Position=uViewProj*uModel*vec4(aPos,1); }\n",
// 				"#version 330 core\n"
// 				"in vec3 vColor;\n"
// 				"out vec4 FragColor;\n"
// 				"void main(){ FragColor = vec4(vColor,1); }\n");

// 			cubeMesh			   = std::make_shared<Mesh>();
// 			std::vector<float> pos = {
// 				-0.5f, -0.5f, 0.5f,	 0.5f,	-0.5f, 0.5f,  0.5f,	 0.5f,	0.5f,  -0.5f, 0.5f,	 0.5f,
// 				0.5f,  -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,	-0.5f, 0.5f,  0.5f,	 -0.5f,
// 				0.5f,  -0.5f, 0.5f,	 0.5f,	-0.5f, -0.5f, 0.5f,	 0.5f,	-0.5f, 0.5f,  0.5f,	 0.5f,
// 				-0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,	0.5f,  -0.5f, 0.5f,	 -0.5f,
// 				-0.5f, 0.5f,  0.5f,	 0.5f,	0.5f,  0.5f,  0.5f,	 0.5f,	-0.5f, -0.5f, 0.5f,	 -0.5f,
// 				-0.5f, -0.5f, -0.5f, 0.5f,	-0.5f, -0.5f, 0.5f,	 -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,
// 			};
// 			std::vector<float> col;
// 			auto			   pushColor = [&](float r, float g, float b) {
// 				  for (int i = 0; i < 4; ++i) {
// 					  col.push_back(r);
// 					  col.push_back(g);
// 					  col.push_back(b);
// 				  }
// 			};
// 			pushColor(1, 0.2f, 0.2f);
// 			pushColor(0.2f, 1, 0.2f);
// 			pushColor(0.2f, 0.4f, 1);
// 			pushColor(1, 0.9f, 0.2f);
// 			pushColor(1, 0.2f, 1);
// 			pushColor(0.2f, 1, 1);
// 			std::vector<uint32_t> idx;
// 			for (uint32_t f = 0; f < 6; ++f) {
// 				uint32_t b = f * 4;
// 				idx.insert(idx.end(), {b + 0, b + 1, b + 2, b + 2, b + 3, b + 0});
// 			}
// 			cubeMesh->upload(pos, col, idx);

// 			// ── Estado por ventana ──────────────────────────────────
// 			st.cube = std::make_shared<Entity>(cubeMesh, shader);
// 			st.cam	= Camera({4.f, 2.f, 4.f}, {0.f, 0.f, 0.f});

// 			// Renderer: config inicial (aquí es donde cachea y aplica)
// 			st.renderer.depthTest(true);
// 			st.renderer.depthFunc(GL_LESS);
// 			st.renderer.depthWrite(true);
// 			st.renderer.cullFace(true);
// 			st.renderer.cullMode(GL_BACK);
// 			st.renderer.frontFace(GL_CCW);
// 			st.renderer.blend(false);

// 			(void)w;
// 		});

// 		env.setUpdate([&](WWindow& w, float dt) {
// 			Input& in = w.input();

// 			if (in.isKeyPressed(Key::Code::Escape))
// 				w.requestClose();

// 			// ── Cámara: WASD mueve, rueda hace zoom ────────────────
// 			float	  speed = 3.f * dt;
// 			glm::vec3 fwd	= st.cam.forward();
// 			glm::vec3 right = st.cam.right();
// 			glm::vec3 up	= st.cam.up();

// 			glm::vec3 move{0.f};

// 			if (in.isKeyDown(Key::Code::LeftShift) && in.isKeyDown(Key::Code::W))
// 				move += fwd;
// 			if (in.isKeyDown(Key::Code::LeftShift) && in.isKeyDown(Key::Code::S))
// 				move -= fwd;
// 			if (in.isKeyDown(Key::Code::W))
// 				move += up;
// 			if (in.isKeyDown(Key::Code::S))
// 				move -= up;
// 			if (in.isKeyDown(Key::Code::D))
// 				move += right;
// 			if (in.isKeyDown(Key::Code::A))
// 				move -= right;

// 			if (in.isMouseButtonDown(Mouse::Button::Right)) {
// 				const float panSens = 0.02f;  // píxeles → unidades de mundo
// 				glm::vec3	pan		= right * (-static_cast<float>(in.mouseDeltaX()) * panSens) +
// 								up * (static_cast<float>(in.mouseDeltaY()) * panSens);
// 				st.cam.move(pan);
// 			}

// 			if (glm::length(move) > 1e-6f)
// 				st.cam.move(glm::normalize(move) * speed);

// 			if (in.scrollDelta() != 0.0)
// 				st.cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);
// 			if (in.isMouseButtonDown(Mouse::Button::Left))
// 				st.cam.orbit(-static_cast<float>(in.mouseDeltaX()) * 0.5f, 0.f, 0.f);
// 			if (in.isMouseButtonDown(Mouse::Button::Left))
// 				st.cam.orbit(0.f, static_cast<float>(in.mouseDeltaY()) * 0.5f, 0.f);

// 			// ── Rotación del cubo con flechas ──────────────────────
// 			float rotSpeed = 60.f * dt;
// 			if (in.isKeyDown(Key::Code::Left))
// 				st.cube->transform().rotate(rotSpeed, {0, 1, 0});
// 			if (in.isKeyDown(Key::Code::Right))
// 				st.cube->transform().rotate(-rotSpeed, {0, 1, 0});
// 			if (in.isKeyDown(Key::Code::Up))
// 				st.cube->transform().rotate(rotSpeed, {1, 0, 0});
// 			if (in.isKeyDown(Key::Code::Down))
// 				st.cube->transform().rotate(-rotSpeed, {1, 0, 0});

// 			// ── Saltar con Space (flanco) ──────────────────────────
// 			if (in.isKeyPressed(Key::Code::Space))
// 				std::printf("Jump! (isKeyPressed Space)\n");
// 		});

// 		env.setRender([&](WWindow& w) {
// 			int rw, rh;
// 			if (w.consumeResize(rw, rh))
// 				st.renderer.setViewport(rw, rh);
// 			else
// 				st.renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

// 			st.renderer.setClearColor(0.10f, 0.15f, 0.20f, 1.f);
// 			st.renderer.clear(true, true, false);

// 			glm::mat4 vp = st.cam.projection(w.aspect()) * st.cam.view();
// 			st.cube->render(vp);
// 		});

// 		env.run();
// 	} catch (const std::exception& e) {
// 		std::fprintf(stderr, "exception: %s\n", e.what());
// 		return 1;
// 	}
// 	return 0;
// }

// int main()
// {
// 	try {
// 		Environment env;

// 		WWindow::Config cfg;
// 		cfg.title       = "One particle — Space to reset, Esc to close";
// 		cfg.width       = 900;
// 		cfg.height      = 600;
// 		cfg.glMajor     = 3;
// 		cfg.glMinor     = 3;
// 		cfg.coreProfile = true;
// 		cfg.vsync       = true;
// 		env.addWindow(cfg);

// 		struct State
// 		{
// 			Camera   cam;
// 			Renderer renderer;

// 			std::shared_ptr<Shader> particleShader;
// 			std::shared_ptr<Mesh>   particleMesh;

// 			// Estado de la partícula (una sola)
// 			glm::vec3 position{0.f, 2.f, 0.f};
// 			glm::vec3 velocity{0.f};
// 			std::vector<float> positions{0.f, 2.f, 0.f};   // flat vec3 (1 punto)
// 		} st;

// 		env.setInit([&](WWindow& w) {
// 			// ── Shader de partículas ──────────────────────────────
// 			st.particleShader = std::make_shared<Shader>();
// 			if (!st.particleShader->loadFromSource(
// 				// vertex
// 				"#version 330 core\n"
// 				"layout(location = 0) in vec3 aPos;\n"
// 				"uniform mat4  uViewProj;\n"
// 				"uniform float uPointSize;\n"
// 				"void main() {\n"
// 				"    gl_Position  = uViewProj * vec4(aPos, 1.0);\n"
// 				"    gl_PointSize = uPointSize;\n"
// 				"}\n",
// 				// fragment
// 				"#version 330 core\n"
// 				"uniform vec3  uColor;\n"
// 				"uniform float uAlpha;\n"
// 				"out vec4 FragColor;\n"
// 				"void main() {\n"
// 				"    vec2 uv = gl_PointCoord * 2.0 - 1.0;\n"
// 				"    float r2 = dot(uv, uv);\n"
// 				"    if (r2 > 1.0) discard;\n"
// 				"    float edge = smoothstep(1.0, 0.85, r2);\n"
// 				"    FragColor = vec4(uColor, uAlpha * edge);\n"
// 				"}\n"))
// 			{
// 				std::fprintf(stderr, "particle shader failed\n");
// 				return;
// 			}

// 			// ── Mesh con UN solo punto ────────────────────────────
// 			st.particleMesh = std::make_shared<Mesh>();
// 			st.particleMesh->uploadPoints(st.positions);   // 1 punto en (0, 2, 0)

// 			// ── Cámara ────────────────────────────────────────────
// 			st.cam = Camera({3.f, 2.f, 4.f}, {0.f, 1.f, 0.f});

// 			// ── Estado GL ─────────────────────────────────────────
// 			st.renderer.depthTest(true);
// 			st.renderer.depthFunc(GL_LESS);
// 			st.renderer.depthWrite(true);
// 			st.renderer.cullFace(false);       // no culling: queremos ver el punto siempre
// 			st.renderer.blend(false);

// 			// OBLIGATORIO: sin esto, gl_PointSize se ignora y el punto
// 			// sale de 1 píxel (prácticamente invisible).
// 			glEnable(GL_PROGRAM_POINT_SIZE);

// 			(void)w;
// 		});

// 		env.setUpdate([&](WWindow& w, float dt) {
// 			Input& in = w.input();

// 			if (in.isKeyPressed(Key::Code::Escape))
// 				w.requestClose();

// 			// Reset de la partícula al estado inicial
// 			if (in.isKeyPressed(Key::Code::Space)) {
// 				st.position = {0.f, 2.f, 0.f};
// 				st.velocity.y = 5.f;
// 				std::printf("[reset] particle at (%.1f, %.1f, %.1f)\n",
// 							st.position.x, st.position.y, st.position.z);
// 			}

// 			// Lanzar hacia arriba (para ver el rebote)
// 			if (in.isKeyPressed(Key::Code::Up)) {
// 				st.velocity.y = 5.f;
// 				std::printf("[jump] vy = 5\n");
// 			}

// 			// ── Física de una partícula ───────────────────────────
// 			constexpr float GRAVITY    = -9.8f;
// 			constexpr float BOUNCE     = 0.5f;
// 			constexpr float FLOOR_Y    = 0.f;
// 			constexpr float REST_SPEED = 0.1f;   // si rebota muy poco, se para

// 			st.velocity.y += GRAVITY * dt;
// 			st.position   += st.velocity * dt;

// 			// Colisión con el suelo
// 			if (st.position.y < FLOOR_Y) {
// 				st.position.y = FLOOR_Y;
// 				if (st.velocity.y < 0.f)
// 					st.velocity.y = -st.velocity.y * BOUNCE;
// 				// Si la velocidad es muy pequeña, se queda quieta (evita jitter)
// 				if (std::abs(st.velocity.y) < REST_SPEED)
// 					st.velocity.y = 0.f;
// 			}

// 			// Actualiza el buffer flat para subirlo a GPU
// 			st.positions[0] = st.position.x;
// 			st.positions[1] = st.position.y;
// 			st.positions[2] = st.position.z;
// 			st.particleMesh->updatePoints(st.positions);

// 			// ── Cámara: WASD + botón derecho pan + rueda zoom ────
// 			float     speed = 3.f * dt;
// 			glm::vec3 fwd   = st.cam.forward();
// 			glm::vec3 right = st.cam.right();
// 			glm::vec3 up    = st.cam.up();

// 			glm::vec3 move{0.f};
// 			if (in.isKeyDown(Key::Code::W)) move += fwd;
// 			if (in.isKeyDown(Key::Code::S)) move -= fwd;
// 			if (in.isKeyDown(Key::Code::D)) move += right;
// 			if (in.isKeyDown(Key::Code::A)) move -= right;
// 			if (glm::length(move) > 1e-6f)
// 				st.cam.move(glm::normalize(move) * speed);

// 			if (in.isMouseButtonDown(Mouse::Button::Right)) {
// 				const float panSens = 0.02f;
// 				glm::vec3   pan     = right * (-static_cast<float>(in.mouseDeltaX()) * panSens) +
// 								up * (static_cast<float>(in.mouseDeltaY()) * panSens);
// 				st.cam.move(pan);
// 			}

// 			if (in.scrollDelta() != 0.0)
// 				st.cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);
// 		});

// 		env.setRender([&](WWindow& w) {
// 			int rw, rh;
// 			if (w.consumeResize(rw, rh))
// 				st.renderer.setViewport(rw, rh);
// 			else
// 				st.renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

// 			st.renderer.setClearColor(0.05f, 0.05f, 0.10f, 1.f);
// 			st.renderer.clear(true, true, false);

// 			glm::mat4 vp = st.cam.projection(w.aspect()) * st.cam.view();

// 			// ── Dibujar la partícula ──────────────────────────────
// 			glEnable(GL_BLEND);
// 			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
// 			glDepthMask(GL_FALSE);

// 			st.particleShader->use();
// 			st.particleShader->setMat4("uViewProj",   vp);
// 			st.particleShader->setFloat("uPointSize", 24.0f);
// 			st.particleShader->setVec3 ("uColor",     glm::vec3(0.2f, 0.6f, 1.0f));
// 			st.particleShader->setFloat("uAlpha",     1.0f);
// 			st.particleMesh->drawPoints();

// 			glDepthMask(GL_TRUE);
// 			glDisable(GL_BLEND);
// 		});

// 		env.run();
// 	} catch (const std::exception& e) {
// 		std::fprintf(stderr, "exception: %s\n", e.what());
// 		return 1;
// 	}
// 	return 0;
// }
#include <Particle.hpp>
#include <Particles.hpp>

int main() {
    try {
        Environment env;

        WWindow::Config cfg;
        cfg.title       = "SPH Water — 1=rise 2=wave 3=rain  R=reset  Esc=close";
        cfg.width       = 1000;
        cfg.height      = 700;
        cfg.glMajor     = 3;
        cfg.glMinor     = 3;
        cfg.coreProfile = true;
        cfg.vsync       = true;
        env.addWindow(cfg);

        struct State {
            Camera   cam;
            Renderer renderer;
            std::shared_ptr<Shader>   shader;
            std::shared_ptr<Particles> water;
            Particles::Mode mode = Particles::Mode::UniformRise;
        } st;

        env.setInit([&](WWindow& w) {
            st.shader = std::make_shared<Shader>();
            if (!st.shader->loadFromSource(
                "#version 330 core\n"
                "layout(location=0) in vec3 aPos;\n"
                "layout(location=1) in float aDensity;\n"
                "uniform mat4 uViewProj;\n"
                "uniform float uPointSize, uDensityMin, uDensityMax;\n"
                "out float vDensity;\n"
                "void main(){\n"
                "  vDensity = aDensity;\n"
                "  gl_Position = uViewProj * vec4(aPos, 1.0);\n"
                "  float t = clamp((aDensity - uDensityMin) /\n"
                "                  max(uDensityMax - uDensityMin, 0.001), 0.0, 1.0);\n"
                "  gl_PointSize = uPointSize * (0.7 + 0.6 * t);\n"
                "}\n",
                "#version 330 core\n"
                "in float vDensity;\n"
                "uniform vec3 uColorLow, uColorHigh;\n"
                "uniform float uDensityMin, uDensityMax, uAlpha;\n"
                "out vec4 FragColor;\n"
                "void main(){\n"
                "  vec2 uv = gl_PointCoord * 2.0 - 1.0;\n"
                "  float r2 = dot(uv, uv);\n"
                "  if (r2 > 1.0) discard;\n"
                "  float edge = smoothstep(1.0, 0.85, r2);\n"
                "  float t = clamp((vDensity - uDensityMin) /\n"
                "                  max(uDensityMax - uDensityMin, 0.001), 0.0, 1.0);\n"
                "  FragColor = vec4(mix(uColorLow, uColorHigh, t), uAlpha * edge);\n"
                "}\n"))
            {
                std::fprintf(stderr, "shader failed\n");
                return;
            }

            // Sistema de agua
            st.water = std::make_shared<Particles>();
            Particles::Config pcfg;
            pcfg.boundsMin = {-1.f, 0.f, -1.f};
            pcfg.boundsMax = { 1.f, 3.f,  1.f};
            pcfg.pointSize = 6.f;
            pcfg.alpha     = 0.6f;
            st.water->setConfig(pcfg);

            // (Opcional) colisión con terreno:
            // st.water->setTerrainHeight([](float x, float z) {
            //     return 0.2f * std::sin(x * 3.f) * std::cos(z * 3.f);
            // });

            st.water->init(1500, st.mode);

            st.cam = Camera({3.f, 3.f, 4.f}, {0.f, 1.f, 0.f});

            st.renderer.depthTest(true);
            st.renderer.cullFace(false);
            st.renderer.blend(false);   // el blending de partículas se activa en setRender

            glEnable(GL_PROGRAM_POINT_SIZE);

            (void)w;
        });

        env.setUpdate([&](WWindow& w, float dt) {
            Input& in = w.input();
            if (in.isKeyPressed(Key::Code::Escape)) w.requestClose();

            // Cambiar modo
            if (in.isKeyPressed(Key::Code::Num1)) { st.mode = Particles::Mode::UniformRise; st.water->reset(st.mode); }
            if (in.isKeyPressed(Key::Code::Num2)) { st.mode = Particles::Mode::Wave;        st.water->reset(st.mode); }
            if (in.isKeyPressed(Key::Code::Num3)) { st.mode = Particles::Mode::Rain;        st.water->reset(st.mode); }
            if (in.isKeyPressed(Key::Code::R))    { st.water->reset(st.mode); }

            // Física
            st.water->update(dt);
            st.water->uploadToGPU();

            // Cámara
            float speed = 3.f * dt;
            glm::vec3 fwd   = st.cam.forward();
            glm::vec3 right = st.cam.right();
            glm::vec3 up    = st.cam.up();
            glm::vec3 move{0.f};
            if (in.isKeyDown(Key::Code::W)) move += fwd;
            if (in.isKeyDown(Key::Code::S)) move -= fwd;
            if (in.isKeyDown(Key::Code::D)) move += right;
            if (in.isKeyDown(Key::Code::A)) move -= right;
            if (glm::length(move) > 1e-6f) st.cam.move(glm::normalize(move) * speed);

            if (in.isMouseButtonDown(Mouse::Button::Right)) {
                const float s = 0.02f;
                st.cam.move(right * (-static_cast<float>(in.mouseDeltaX()) * s) +
                            up    * ( static_cast<float>(in.mouseDeltaY()) * s));
            }
            if (in.scrollDelta() != 0.0)
                st.cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);
        });

        env.setRender([&](WWindow& w) {
            int rw, rh;
            if (w.consumeResize(rw, rh)) st.renderer.setViewport(rw, rh);
            else                          st.renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

            st.renderer.setClearColor(0.05f, 0.05f, 0.10f, 1.f);
            st.renderer.clear(true, true, false);

            glm::mat4 vp = st.cam.projection(w.aspect()) * st.cam.view();

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            st.water->render(*st.shader, vp);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        });

        env.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "exception: %s\n", e.what());
        return 1;
    }
    return 0;
}