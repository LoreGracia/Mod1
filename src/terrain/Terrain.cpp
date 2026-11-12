#include <Terrain.hpp>
#include <utils.h>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <string>
#include <iostream>
#include <delaunator.hpp>

const std::vector<float> Terrain::extractNumbers(const std::string& input)
{
    std::vector<float> numbers;
    std::regex pattern(R"(\d+(?:\.\d+)?)");

    float n;
    int i = 1;
    for (auto it = std::sregex_iterator(input.begin(), input.end(), pattern);
         it != std::sregex_iterator(); ++it)
    {
        n = size / (20000 / std::stof(it->str()));
        n = (i % 3 == 0? n : n  - (size/2));
        numbers.push_back(n);
        i++;
    }
    return numbers;
}


void Terrain::parse(const std::string& mapath) {
    const std::string m = readFile(mapath);

    if (std::any_of(m.begin(), m.end(), [](char c) {
        return !std::isdigit(static_cast<unsigned char>(c))
            && c != ',' && c != '(' && c != ')'
            && c != '\n';
    })) {
        throw std::runtime_error("unexpected token in " + mapath);
    }
    const auto numbers = extractNumbers(m);
    if (numbers.size() % 3 != 0)
        throw std::runtime_error("invalid map coordinates");
    points.clear();
    for (std::size_t i = 0; i < numbers.size(); i += 3) {
        points.emplace_back(
            numbers[i],
            numbers[i + 2],
            numbers[i + 1]
        );
    }
}

Terrain& Terrain::operator=(Terrain&& t) noexcept
{
    if (this == &t)
        return *this;

    cam      = std::move(t.cam);
    renderer = std::move(t.renderer);

    shader = std::move(t.shader);
    plane  = std::move(t.plane);

    rotY = t.rotY;
    rotX = t.rotX;

    cfg = std::move(t.cfg);

    size = t.size;
    initHooks();
    return *this;
}

#include <delaunator.hpp>

void Terrain::generateTerrain()
{
    std::vector<glm::vec3> vertices = points;

    const float h = size * 0.5f;

    vertices.emplace_back(-h, 0.f, -h);
    vertices.emplace_back( h, 0.f, -h);
    vertices.emplace_back( h, 0.f,  h);
    vertices.emplace_back(-h, 0.f,  h);

    // Delaunay trabaja en 2D: utilizamos X y Z.
    std::vector<double> coords;
    coords.reserve(vertices.size() * 2);

    for (const auto& p : vertices) {
        coords.push_back(p.x);
        coords.push_back(p.z);
    }

    delaunator::Delaunator triangulation(coords);

    std::vector<float> positions;
    positions.reserve(vertices.size() * 3);

    for (const auto& p : vertices) {
        positions.push_back(p.x);
        positions.push_back(p.y);
        positions.push_back(p.z);
    }

    std::vector<uint32_t> indices;
    indices.reserve(triangulation.triangles.size());

    for (const auto index : triangulation.triangles)
        indices.push_back(static_cast<uint32_t>(index));

    plane->upload(positions, indices, GL_TRIANGLES);
}

Terrain::Terrain(const char* arg)
    : Terrain(arg ? std::string(arg) : std::string())
{
}

Terrain::Terrain(const std::string mapath) {
    if (mapath.empty())
		throw std::runtime_error("missing path");
    parse(mapath);
    cfg.title       = std::filesystem::path(mapath).filename().string();;
    cfg.width       = 1000;
    cfg.height      = 700;
    initEnv();

    initHooks();

    return ;
}

void Terrain::addWindow(WWindow::Config &newcfg)
{
    cfg.title         = newcfg.title;
    cfg.width         = newcfg.width;
    cfg.height        = newcfg.height;
	cfg.x             = newcfg.x;
    cfg.y             = newcfg.y;
    cfg.shareContext  = newcfg.shareContext;
    cfg.glMajor       = newcfg.glMajor;
    cfg.glMinor       = newcfg.glMinor;
    cfg.coreProfile   = newcfg.coreProfile;
    cfg.resizable     = newcfg.resizable;
    cfg.visible       = newcfg.visible;
    cfg.decorated     = newcfg.decorated;
    cfg.vsync         = newcfg.vsync;
    cfg.samples       = newcfg.samples;
}

void Terrain::initHooks()
{
        env.setUpdate([&](WWindow& w, float dt) {
        Input& in = w.input();
        if (in.isKeyPressed(Key::Code::Escape)) w.requestClose();

        // Cámara
        float speed = 3.f * dt;
        glm::vec3 fwd   = cam.forward();
        glm::vec3 right = cam.right();
        glm::vec3 up    = cam.up();
        glm::vec3 move{0.f};
        if (in.isKeyDown(Key::Code::W)) move += fwd;
        if (in.isKeyDown(Key::Code::S)) move -= fwd;
        if (in.isKeyDown(Key::Code::D)) move += right;
        if (in.isKeyDown(Key::Code::A)) move -= right;
        if (glm::length(move) > 1e-6f) cam.move(glm::normalize(move) * speed);

        if (in.isMouseButtonDown(Mouse::Button::Right)) {
            const float s = 0.02f;
            cam.move(right * (-static_cast<float>(in.mouseDeltaX()) * s) +
                        up    * ( static_cast<float>(in.mouseDeltaY()) * s));
        }

        if (in.isMouseButtonDown(Mouse::Button::Left)) {
            const float sens = 0.01f;
            rotY += static_cast<float>(in.mouseDeltaX()) * sens;
            rotX += static_cast<float>(in.mouseDeltaY()) * sens;

            const float lim = glm::radians(89.f);
            if (rotX >  lim) rotX =  lim;
            if (rotX < -lim) rotX = -lim;
        }

        if (in.scrollDelta() != 0.0)
            cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);

        // Reset de rotación
        if (in.isKeyPressed(Key::Code::R)) {
            rotX = 0.f;
            rotY = 0.f;
        }
    });
}

void Terrain::initEnv() {
    env.setInit([&](WWindow& w) {
        shader = std::make_shared<Shader>();
        if (!shader->loadFromFile(
                "./src/terrain/shader/meshVertex.glsl",
                "./src/terrain/shader/meshFrag.frag"))
        {
            std::fprintf(stderr, "shader failed\n");
            return;
        }

        // Plano de 2x2 (lado = 2.0) centrado en el origen.
        // plane = std::make_shared<Mesh>(Mesh::plane(size));
        plane = std::make_shared<Mesh>();
        generateTerrain();

        cam = Camera({3.f, 3.f, 4.f}, {0.f, 0.f, 0.f});

        renderer.depthTest(true);
        renderer.cullFace(false);
        renderer.blend(false);

        (void)w;
    });
}



void Terrain::render()
{
    env.addWindow(cfg);
    env.setRender([&](WWindow& w) {
        int rw, rh;
        if (w.consumeResize(rw, rh))
            renderer.setViewport(rw, rh);
        else
            renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

        renderer.setClearColor(0.05f, 0.05f, 0.10f, 1.f);
        renderer.clear(true, true, false);

        glm::mat4 model = glm::mat4(1.f);
        model = glm::rotate(model, rotY, glm::vec3(0.f, 1.f, 0.f));
        model = glm::rotate(model, rotX, glm::vec3(1.f, 0.f, 0.f));

        glm::mat4 vp  = cam.projection(w.aspect()) * cam.view();
        glm::mat4 mvp = vp * model;

        shader->use();
        shader->setMat4("uViewProj", mvp);

        plane->draw();
    });

    env.run();
}