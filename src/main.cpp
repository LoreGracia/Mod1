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
#include <Particle.hpp>
#include <Particles.hpp>
#include <GPUMarchingCubes.hpp>

// Debug flag: set to true to render SPH particles as points on top of the mesh.
// Useful for diagnosing whether Marching Cubes is correctly wrapping the fluid.
static constexpr bool RENDER_POINTS = false;

int main() {
    try {
        Environment env;

        WWindow::Config cfg;
        cfg.width       = 800;
        cfg.height      = 800;
        cfg.glMajor     = 4;
        cfg.glMinor     = 3;          // H-01: 4.3 is the minimum for compute shaders
        cfg.coreProfile = true;
        cfg.vsync       = true;
        env.addWindow(cfg);

        struct State {
            Camera   cam;
            Renderer renderer;
            std::shared_ptr<Shader>           pointShader;   // optional, for RENDER_POINTS
            std::shared_ptr<Particles>        water;
            std::shared_ptr<GPUMarchingCubes> marchingCubes;
            Particles::Mode                   mode = Particles::Mode::UniformRise;
        } st;

        env.setInit([&](WWindow& w) {
            // ── 1. Optional point shader (debug only) ───────────────────
            if (RENDER_POINTS) {
                st.pointShader = std::make_shared<Shader>();
                if (!st.pointShader->loadFromFile(
                        "src/water/shader/waterVertex.glsl",
                        "src/water/shader/waterFrag.frag")) {
                    std::fprintf(stderr, "Point shader failed to load\n");
                    return;
                }
            }

            // ── 2. SPH configuration ────────────────────────────────────
            Particles::Config pcfg;
            pcfg.boundsMin       = {-0.95f, -0.95f, -0.95f};
            pcfg.boundsMax       = { 0.95f,  0.95f,  0.95f};
            pcfg.h               = 0.25f;
            pcfg.restDensity     = 150.f;
            pcfg.particleMass    = 0.5f;
            pcfg.stiffness       = 80.f;
            pcfg.viscosity       = 0.3f;
            pcfg.gravity         = -9.8f;
            pcfg.damping         = 0.92f;
            pcfg.substeps        = 8;
            pcfg.wallRestitution = 0.0f;
            pcfg.pointSize       = 15.0f;
            pcfg.alpha           = 0.9f;

            // ── 3. Marching Cubes (loads its own shaders from disk) ─────
            st.marchingCubes = std::make_shared<GPUMarchingCubes>();
            bool mcOk = st.marchingCubes->init(
                pcfg.boundsMin,
                pcfg.boundsMax,
            	32,                                          // grid resolution
                "src/water/shader/marching_cubes.comp",
                "src/water/shader/meshVertex.glsl",
                "src/water/shader/meshFrag.frag");
            if (!mcOk) {
                std::fprintf(stderr, "Failed to init Marching Cubes\n");
                return;
            }
			st.marchingCubes->setIsoLevel(0.002f);
            // ── 4. Water simulation ─────────────────────────────────────
            st.water = std::make_shared<Particles>();
            st.water->init(1000, st.mode, pcfg);

            // ── 5. Camera ───────────────────────────────────────────────
            st.cam = Camera({3.f, 3.f, 4.f}, {0.f, 1.f, 0.f});

            // ── 6. Initial GL state ─────────────────────────────────────
            st.renderer.depthTest(true);
            st.renderer.cullFace(false);
            st.renderer.blend(false);   // opaque mesh by default

            if (RENDER_POINTS) {
                glEnable(GL_PROGRAM_POINT_SIZE);
            }

            (void)w;
        });

        env.setUpdate([&](WWindow& w, float dt) {
            Input& in = w.input();

            if (in.isKeyPressed(Key::Code::Escape))
                w.requestClose();

            // ── Mode switching ─────────────────────────────────────────
            if (in.isKeyPressed(Key::Code::Num1)) { st.mode = Particles::Mode::UniformRise; st.water->reset(st.mode); }
            if (in.isKeyPressed(Key::Code::Num2)) { st.mode = Particles::Mode::Wave;        st.water->reset(st.mode); }
            if (in.isKeyPressed(Key::Code::Num3)) { st.mode = Particles::Mode::Rain;        st.water->reset(st.mode); }
            if (in.isKeyPressed(Key::Code::R))    { st.water->reset(st.mode); }
            if (in.isKeyPressed(Key::Code::Space)) {
                st.water->reset(st.mode);
                std::printf("[reset] Simulation reset\n");
            }

            // ── Physics first ──────────────────────────────────────────
            st.water->update(dt);

            // ── M-05: upload particles AFTER the physics update ────────
            st.marchingCubes->uploadParticles(st.water->getParticles());

            // Only upload point VBOs if we're actually going to draw points.
            if (RENDER_POINTS)
                st.water->uploadToGPU();

            // ── Camera ─────────────────────────────────────────────────
            float     speed = 3.f * dt;
            glm::vec3 fwd   = st.cam.forward();
            glm::vec3 right = st.cam.right();
            glm::vec3 up    = st.cam.up();

            glm::vec3 move{0.f};
            if (in.isKeyDown(Key::Code::W)) move += fwd;
            if (in.isKeyDown(Key::Code::S)) move -= fwd;
            if (in.isKeyDown(Key::Code::D)) move += right;
            if (in.isKeyDown(Key::Code::A)) move -= right;
            if (glm::length(move) > 1e-6f)
                st.cam.move(glm::normalize(move) * speed);

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
            else                          st.renderer.setViewport(w.framebufferWidth(),
                                                                  w.framebufferHeight());

            st.renderer.setClearColor(0.05f, 0.05f, 0.10f, 1.f);
            st.renderer.clear(true, true, false);

            glm::mat4 vp = st.cam.projection(w.aspect()) * st.cam.view();

            // ── H-03: opaque mesh pass ─────────────────────────────────
            // No blending, depth test on, depth write on. The mesh is
            // opaque; writing to the depth buffer lets the GPU discard
            // fragments behind the front surface deterministically.
            st.renderer.blend(true);
            st.renderer.depthTest(true);
            glDepthMask(GL_TRUE);

            st.marchingCubes->generateMesh();

            // static int dbg = 0;
            // if (++dbg % 60 == 0)
            //     std::printf("[MC] vertexCount = %u\n", st.marchingCubes->vertexCount());

            st.marchingCubes->render(vp);

            // ── Optional: points on top (debug only) ───────────────────
            if (RENDER_POINTS) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                st.water->render(*st.pointShader, vp);
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
            }
        });

        env.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "exception: %s\n", e.what());
        return 1;
    }
    return 0;
}