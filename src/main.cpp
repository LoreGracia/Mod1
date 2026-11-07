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
#include <cstdlib>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <memory>
#include <scene/Transform.hpp>
#include <vector>
#include <Particle.hpp>
#include <Particles.hpp>
#include <GPUMarchingCubes.hpp>

int main() {
    try {
        Environment env;

        WWindow::Config cfg;
        cfg.title       = "SPH Water — 1=rise 2=wave 3=rain  R=reset  Esc=close";
        cfg.width       = 1000;
        cfg.height      = 700;
        cfg.glMajor     = 4;
        cfg.glMinor     = 3;
        cfg.coreProfile = true;
        cfg.vsync       = true;
        env.addWindow(cfg);

        struct State {
            Camera   cam;
            Renderer renderer;
            std::shared_ptr<Shader>   shader;
            std::shared_ptr<Particles> water;
			std::shared_ptr<GPUMarchingCubes> marchingCubes;
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
                "  gl_PointSize = uPointSize * (0.9 + 2.0 * t);\n"
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
                "  float edge = smoothstep(1.0, 0.8, r2);\n"
                "  float t = clamp((vDensity - uDensityMin) /\n"
                "                  max(uDensityMax - uDensityMin, 0.001), 0.0, 1.0);\n"
				"  float finalAlpha = mix(0.4, 1.0, t);\n"
                "  FragColor = vec4(mix(uColorLow, uColorHigh, t), uAlpha * edge);\n"
                "}\n"))
            {
                std::fprintf(stderr, "shader failed\n");
                return;
            }

            // Sistema de agua
            st.water = std::make_shared<Particles>();
			Particles::Config pcfg;
			// Caja un poco más pequeña (2x1.5x2 = 6 unidades cúbicas)
			pcfg.boundsMin = {-1.0f, -1.f, -1.0f};
			pcfg.boundsMax = { 1.0f, 1.0f,  1.0f}; 

			pcfg.h = 0.25f;             // Radio más pequeño porque la caja es más pequeña
			pcfg.restDensity  = 150.f;  
			pcfg.particleMass = 0.5f;  // Masa más pequeña para que no haya picos de presión
			pcfg.stiffness    = 80.f;   // Fuerza de empuje moderada
			pcfg.viscosity    = 0.3f;   // ¡CLAVE! Subimos mucho la viscosidad. Esto frena el efecto "gas" y hace que fluyan como un bloque de agua.
			pcfg.gravity      = -9.8f;
			pcfg.damping      = 0.95f;
			pcfg.substeps     = 8;      // Subimos a 8 para que la simulación sea más estable y suave

			pcfg.wallRestitution = 0.0f; 

			pcfg.pointSize = 15.0f;     // Puntos MUY grandes para que se solapen entre sí
			pcfg.alpha     = 0.9f;

			st.marchingCubes = std::make_shared<GPUMarchingCubes>();
			bool ok = st.marchingCubes->init(
				pcfg.boundsMin, 
				pcfg.boundsMax, 
				32, // Resolución baja para probar
				"src/water/shader/density_splat.comp",
				"src/water/shader/marching_cubes.comp",
				"src/water/shader/meshVertex.glsl",
				"src/water/shader/meshFrag.frag"
			);
			if (!ok) {
				std::fprintf(stderr, "Failed to init Marching Cubes\n");
			}
			st.water->init(1000, st.mode, pcfg);

            // (Opcional) colisión con terreno:
            // st.water->setTerrainHeight([](float x, float z) {
            //     return 0.2f * std::sin(x * 3.f) * std::cos(z * 3.f);
            // });
            st.cam = Camera({3.f, 3.f, 4.f}, {0.f, 1.f, 0.f});

            st.renderer.depthTest(true);
            st.renderer.cullFace(false);
            st.renderer.blend(false);   // el blending de partículas se activa en setRender

            glEnable(GL_PROGRAM_POINT_SIZE);

            (void)w;
        });

        env.setUpdate([&](WWindow& w, float dt) {
			st.marchingCubes->uploadParticles(st.water->getParticles());
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
			if (in.isKeyPressed(Key::Code::Space)) {
				st.water->reset(st.mode);
				std::printf("[reset] Simulación reiniciada con la tecla Espacio\n");
			}
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

            // st.water->render(*st.shader, vp);

			st.marchingCubes->generateMesh();
			static bool dbgCount = std::getenv("MC_DEBUG_COUNT") != nullptr;
			if (dbgCount) {
				dbgCount = false;
				std::printf("[MC] debug vertexCount = %u\n",
				            st.marchingCubes->debugReadDrawCount());
			}
			st.marchingCubes->render(vp);
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
