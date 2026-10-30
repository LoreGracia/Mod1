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

int main()
{
	try {
		Environment env;

		WWindow::Config cfg;
		cfg.title		= "Input + Renderer — WASD / mouse / scroll";
		cfg.width		= 900;
		cfg.height		= 600;
		cfg.glMajor		= 3;
		cfg.glMinor		= 3;
		cfg.coreProfile = true;
		cfg.vsync		= true;
		env.addWindow(cfg);	 // solo una ventana; usamos env por comodidad

		struct State
		{
			Camera					cam;
			Renderer				renderer;
			std::shared_ptr<Entity> cube;
		} st;

		std::shared_ptr<Shader> shader;
		std::shared_ptr<Mesh>	cubeMesh;

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

			cubeMesh			   = std::make_shared<Mesh>();
			std::vector<float> pos = {
				-0.5f, -0.5f, 0.5f,	 0.5f,	-0.5f, 0.5f,  0.5f,	 0.5f,	0.5f,  -0.5f, 0.5f,	 0.5f,
				0.5f,  -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,	-0.5f, 0.5f,  0.5f,	 -0.5f,
				0.5f,  -0.5f, 0.5f,	 0.5f,	-0.5f, -0.5f, 0.5f,	 0.5f,	-0.5f, 0.5f,  0.5f,	 0.5f,
				-0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,	0.5f,  -0.5f, 0.5f,	 -0.5f,
				-0.5f, 0.5f,  0.5f,	 0.5f,	0.5f,  0.5f,  0.5f,	 0.5f,	-0.5f, -0.5f, 0.5f,	 -0.5f,
				-0.5f, -0.5f, -0.5f, 0.5f,	-0.5f, -0.5f, 0.5f,	 -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,
			};
			std::vector<float> col;
			auto			   pushColor = [&](float r, float g, float b) {
				  for (int i = 0; i < 4; ++i) {
					  col.push_back(r);
					  col.push_back(g);
					  col.push_back(b);
				  }
			};
			pushColor(1, 0.2f, 0.2f);
			pushColor(0.2f, 1, 0.2f);
			pushColor(0.2f, 0.4f, 1);
			pushColor(1, 0.9f, 0.2f);
			pushColor(1, 0.2f, 1);
			pushColor(0.2f, 1, 1);
			std::vector<uint32_t> idx;
			for (uint32_t f = 0; f < 6; ++f) {
				uint32_t b = f * 4;
				idx.insert(idx.end(), {b + 0, b + 1, b + 2, b + 2, b + 3, b + 0});
			}
			cubeMesh->upload(pos, col, idx);

			// ── Estado por ventana ──────────────────────────────────
			st.cube = std::make_shared<Entity>(cubeMesh, shader);
			st.cam	= Camera({4.f, 2.f, 4.f}, {0.f, 0.f, 0.f});

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

			if (in.isKeyPressed(Key::Code::Escape))
				w.requestClose();

			// ── Cámara: WASD mueve, rueda hace zoom ────────────────
			float	  speed = 3.f * dt;
			glm::vec3 fwd	= st.cam.forward();
			glm::vec3 right = st.cam.right();
			glm::vec3 up	= st.cam.up();

			glm::vec3 move{0.f};

			if (in.isKeyDown(Key::Code::LeftShift) && in.isKeyDown(Key::Code::W))
				move += fwd;
			if (in.isKeyDown(Key::Code::LeftShift) && in.isKeyDown(Key::Code::S))
				move -= fwd;
			if (in.isKeyDown(Key::Code::W))
				move += up;
			if (in.isKeyDown(Key::Code::S))
				move -= up;
			if (in.isKeyDown(Key::Code::D))
				move += right;
			if (in.isKeyDown(Key::Code::A))
				move -= right;

			if (in.isMouseButtonDown(Mouse::Button::Right)) {
				const float panSens = 0.02f;  // píxeles → unidades de mundo
				glm::vec3	pan		= right * (-static_cast<float>(in.mouseDeltaX()) * panSens) +
								up * (static_cast<float>(in.mouseDeltaY()) * panSens);
				st.cam.move(pan);
			}

			if (glm::length(move) > 1e-6f)
				st.cam.move(glm::normalize(move) * speed);

			if (in.scrollDelta() != 0.0)
				st.cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);
			if (in.isMouseButtonDown(Mouse::Button::Left))
				st.cam.orbit(-static_cast<float>(in.mouseDeltaX()) * 0.5f, 0.f, 0.f);
			if (in.isMouseButtonDown(Mouse::Button::Left))
				st.cam.orbit(0.f, static_cast<float>(in.mouseDeltaY()) * 0.5f, 0.f);

			// ── Rotación del cubo con flechas ──────────────────────
			float rotSpeed = 60.f * dt;
			if (in.isKeyDown(Key::Code::Left))
				st.cube->transform().rotate(rotSpeed, {0, 1, 0});
			if (in.isKeyDown(Key::Code::Right))
				st.cube->transform().rotate(-rotSpeed, {0, 1, 0});
			if (in.isKeyDown(Key::Code::Up))
				st.cube->transform().rotate(rotSpeed, {1, 0, 0});
			if (in.isKeyDown(Key::Code::Down))
				st.cube->transform().rotate(-rotSpeed, {1, 0, 0});

			// ── Saltar con Space (flanco) ──────────────────────────
			if (in.isKeyPressed(Key::Code::Space))
				std::printf("Jump! (isKeyPressed Space)\n");
		});

		env.setRender([&](WWindow& w) {
			int rw, rh;
			if (w.consumeResize(rw, rh))
				st.renderer.setViewport(rw, rh);
			else
				st.renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

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
