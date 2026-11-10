#pragma once
#include <iostream>
#include <memory>

#include <sstream>
#include <string>

#include <glm.hpp>
#include <gtc/type_ptr.hpp>

#include <Camera.hpp>
#include <Environment.hpp>
#include <Input.hpp>
#include <Renderer.hpp>
#include <Mesh.hpp>
#include <Shader.hpp>
#include <WWindow.hpp>


class Terrain
{
    private:
            Environment env;
            WWindow::Config cfg;
            Camera   cam;
            Renderer renderer;
            std::shared_ptr<Shader> shader;
            std::shared_ptr<Mesh>   plane;
            float rotY = 0.f;
            float rotX = 0.f;
            float size = 0.5;
            bool parse(const std::string& mapPath);

            const std::vector<float> extractNumbers(const std::string& input);
            void initHooks();


    public:
        Terrain(const std::string mapPath);
        Terrain(const char* arg);
        Terrain& operator=(Terrain&& t) noexcept;
        Terrain(const Terrain&) = delete;
        Terrain& operator=(const Terrain&) = delete;
        ~Terrain() = default;

        WWindow &addWindow(WWindow::Config &cfg);

        void render();
};
