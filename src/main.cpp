#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <glad/gl.h>

#include <Camera.hpp>
#include <Environment.hpp>
#include <Input.hpp>
#include <Renderer.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <WWindow.hpp>
#include <cstdint>
#include <cstdio>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <memory>

int main() {
    try {
        Environment env;

        WWindow::Config cfg;
        cfg.title       = "Plano cuadrado — W/A/S/D mover  Esc cerrar";
        cfg.width       = 1000;
        cfg.height      = 700;
        env.addWindow(cfg);

        struct State {
            Camera   cam;
            Renderer renderer;
            std::shared_ptr<Shader> shader;
            std::shared_ptr<Mesh>   plane;   // ahora usamos Mesh
            float rotY = 0.f;
            float rotX = 0.f;
        } st;

        env.setInit([&](WWindow& w) {
            st.shader = std::make_shared<Shader>();
            if (!st.shader->loadFromFile(
                    "./src/water/shader/meshVertex.glsl",
                    "./src/water/shader/meshFrag.frag"))
            {
                std::fprintf(stderr, "shader failed\n");
                return;
            }

            // Plano de 2x2 (lado = 2.0) centrado en el origen.
            st.plane = std::make_shared<Mesh>(Mesh::plane(2.0f));

            st.cam = Camera({3.f, 3.f, 4.f}, {0.f, 0.f, 0.f});

            st.renderer.depthTest(true);
            st.renderer.cullFace(false);
            st.renderer.blend(false);

            (void)w;
        });

        env.setUpdate([&](WWindow& w, float dt) {
            Input& in = w.input();
            if (in.isKeyPressed(Key::Code::Escape)) w.requestClose();

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

            if (in.isMouseButtonDown(Mouse::Button::Left)) {
                const float sens = 0.01f;
                st.rotY += static_cast<float>(in.mouseDeltaX()) * sens;
                st.rotX += static_cast<float>(in.mouseDeltaY()) * sens;

                const float lim = glm::radians(89.f);
                if (st.rotX >  lim) st.rotX =  lim;
                if (st.rotX < -lim) st.rotX = -lim;
            }

            if (in.scrollDelta() != 0.0)
                st.cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);

            // Reset de rotación
            if (in.isKeyPressed(Key::Code::R)) {
                st.rotX = 0.f;
                st.rotY = 0.f;
            }
        });

        env.setRender([&](WWindow& w) {
            int rw, rh;
            if (w.consumeResize(rw, rh)) st.renderer.setViewport(rw, rh);
            else                          st.renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

            st.renderer.setClearColor(0.05f, 0.05f, 0.10f, 1.f);
            st.renderer.clear(true, true, false);

            glm::mat4 model = glm::mat4(1.f);
            model = glm::rotate(model, st.rotY, glm::vec3(0.f, 1.f, 0.f));
            model = glm::rotate(model, st.rotX, glm::vec3(1.f, 0.f, 0.f));

            glm::mat4 vp  = st.cam.projection(w.aspect()) * st.cam.view();
            glm::mat4 mvp = vp * model;

            st.shader->use();
            st.shader->setMat4("uViewProj", mvp);

            st.plane->draw();
        });

        env.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "exception: %s\n", e.what());
        return 1;
    }
    return 0;
}