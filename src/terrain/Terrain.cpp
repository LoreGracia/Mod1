#include <Terrain.hpp>
#include <utils.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <regex>
#include <algorithm>
#include <cmath>

namespace {

float hash2(glm::vec2 p) {
    return glm::fract(std::sin(glm::dot(p, glm::vec2(127.1f, 311.7f))) * 43758.5453f);
}

float noise2(glm::vec2 p) {
    glm::vec2 i = glm::floor(p);
    glm::vec2 f = glm::fract(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = hash2(i);
    float b = hash2(i + glm::vec2(1, 0));
    float c = hash2(i + glm::vec2(0, 1));
    float d = hash2(i + glm::vec2(1, 1));
    return glm::mix(glm::mix(a, b, f.x), glm::mix(c, d, f.x), f.y);
}

float fbm(glm::vec2 p) {
    float v = 0.0f, a = 0.5f;
    for (int i = 0; i < 5; ++i) {
        v += a * noise2(p);
        p *= 2.0f;
        a *= 0.5f;
    }
    return v;
}

} // namespace

const std::vector<float> Terrain::extractNumbers(const std::string& input)
{
	std::vector<float> numbers;
	std::regex		   pattern(R"(\d+(?:\.\d+)?)");

	float n;
	int	  i = 1;
	for (auto it = std::sregex_iterator(input.begin(), input.end(), pattern);
		 it != std::sregex_iterator(); ++it) {
		n = size / (20000 / std::stof(it->str()));
		n = (i % 3 == 0 ? n : n - (size / 2));
		numbers.push_back(n);
		i++;
	}
	return numbers;
}


void Terrain::parse(const std::string& mapath)
{
	const std::string m = readFile(mapath);

	if (std::any_of(m.begin(), m.end(), [](char c) {
			return !std::isdigit(static_cast<unsigned char>(c)) && c != ',' && c != '(' &&
				   c != ')' && c != '\n' && c != ' ';
		})) {
		throw std::runtime_error("unexpected token in " + mapath);
	}
	const auto numbers = extractNumbers(m);
	if (numbers.size() % 3 != 0)
		throw std::runtime_error("invalid map coordinates");
	points.clear();
	for (std::size_t i = 0; i < numbers.size(); i += 3) {
        if (numbers[i] >= size || numbers[i + 1] >= size)
		    throw std::runtime_error("coordinatesa are out of map size 20000");
		points.emplace_back(numbers[i], numbers[i + 2], numbers[i + 1]);
	}
	originalPeak = 0.f;
	for (const auto& p : points)
		originalPeak = std::max(originalPeak, p.y);
}

Terrain& Terrain::operator=(Terrain&& t) noexcept
{
	if (this == &t)
		return *this;

	cam		 = std::move(t.cam);
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

void Terrain::getNormals()
{
	normals.clear();
	std::vector<glm::vec3> vertexNormals(vertices.size(), glm::vec3(0.f));

	for (std::size_t i = 0; i < indices.size(); i += 3) {
		const uint32_t ia = indices[i];
		const uint32_t ib = indices[i + 1];
		const uint32_t ic = indices[i + 2];

		const glm::vec3& a = vertices[ia];
		const glm::vec3& b = vertices[ib];
		const glm::vec3& c = vertices[ic];

		glm::vec3 normal = glm::cross(b - a, c - a);

		// Queremos que las normales apunten hacia arriba.
		if (normal.y < 0.f)
			normal = -normal;

		vertexNormals[ia] += normal;
		vertexNormals[ib] += normal;
		vertexNormals[ic] += normal;
	}

	// Normalizar y convertir a formato float.
	normals.reserve(vertices.size() * 3);

	for (const auto& normal : vertexNormals) {
		glm::vec3 n = normal;

		if (glm::length(n) > 1e-6f)
			n = glm::normalize(n);
		else
			n = glm::vec3(0.f, 1.f, 0.f);

		normals.push_back(n.x);
		normals.push_back(n.y);
		normals.push_back(n.z);
	}
}

void Terrain::generateTerrain()
{
    constexpr int resolution = 128;
    const float h = size * 0.5f;

    const float gridSpacing = size / (resolution - 1);
    const float influenceScale = 0.6f;
    const float minInfluenceRadius = gridSpacing * 0.5f;
    const float maxInfluenceRadius = size * 0.3f;

    // 1. Precalcular jitter/tilt UNA SOLA VEZ por punto
    std::vector<float> influenceRadii(points.size(), 0.f);
    std::vector<float> pointTilt(points.size(), 0.f);

    for (std::size_t i = 0; i < points.size(); ++i)
    {
        float nearestDistance = size;
        float nearestNeighborHeight = points[i].y;

        for (std::size_t j = 0; j < points.size(); ++j)
        {
            if (i == j) continue;
            const float dx = points[i].x - points[j].x;
            const float dz = points[i].z - points[j].z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestNeighborHeight = points[j].y;
            }
        }

        const float baseRadius = nearestDistance * influenceScale;
        const float heightDifference = std::max(0.f, points[i].y - nearestNeighborHeight);
        const float heightExpansion = heightDifference * 0.35f;

        // Jitter
        const float jitter = (fbm(glm::vec2(points[i].x, points[i].z) * 5.0f) - 0.5f);

        influenceRadii[i] = std::clamp(
            baseRadius * (1.0f + 0.4f * jitter) + heightExpansion,
            minInfluenceRadius,
            maxInfluenceRadius
        );

        // Tilt
        pointTilt[i] = (fbm(glm::vec2(points[i].x, points[i].z) * 4.0f) - 0.5f) * 0.3f;
    }

    // 2. Factores de montaña
    float minH = points.front().y;
    float maxH = points.front().y;
    for (const auto& point : points) {
        minH = std::min(minH, point.y);
        maxH = std::max(maxH, point.y);
    }

    std::vector<float> mountainFactors(points.size(), 0.f);
    for (std::size_t i = 0; i < points.size(); ++i) {
        float t = 0.f;
        if (maxH - minH > 1e-6f)
            t = (points[i].y - minH) / (maxH - minH);
        t = std::clamp((t - 0.45f) / (0.80f - 0.45f), 0.f, 1.f);
        mountainFactors[i] = t * t * (3.f - 2.f * t);
    }

    const float blendFactor = 0.2f;

    auto smoothMax = [](float a, float b, float factor) -> float {
        const float maximum = std::max(a, b);
        const float minimum = std::min(a, b);
        const float k = factor * minimum;
        if (k <= 1e-6f) return maximum;
        const float difference = maximum - minimum;
        const float blend = 0.5f * (std::sqrt(difference * difference + k * k) - difference);
        return maximum + blend;
    };

    // 3. sampleHeight
    auto sampleHeight = [&](float x, float z) -> float
    {
        const float warp = fbm(glm::vec2(x, z) * 0.7f) - 0.5f;
        const float wx = x + warp * 0.15f;
        const float wz = z + warp * 0.15f;

        float height = 0.f;

        const float left  = x + h;
        const float right = h - x;
        const float near  = z + h;
        const float far   = h - z;

        auto smoothStep = [](float edge0, float edge1, float value) {
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.f, 1.f);
            return t * t * (3.f - 2.f * t);
        };

        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const glm::vec3& point = points[i];

            const float dx = wx - point.x;
            const float dz = wz - point.z;

            const float distanceSquared = dx * dx + dz * dz;
            const float radius = influenceRadii[i];
            const float exponent = 2.0f - 0.5f * mountainFactors[i];
            const float normalizedDistanceSquared = distanceSquared / (radius * radius);
            const float influence = std::exp(
                -0.5f * std::pow(normalizedDistanceSquared, exponent * 0.5f)
            );

            const float pointLeft  = point.x + h;
            const float pointRight = h - point.x;
            const float pointNear  = point.z + h;
            const float pointFar   = h - point.z;

            const float fadeLeft  = smoothStep(0.f, std::max(pointLeft,  gridSpacing), left);
            const float fadeRight = smoothStep(0.f, std::max(pointRight, gridSpacing), right);
            const float fadeNear  = smoothStep(0.f, std::max(pointNear,  gridSpacing), near);
            const float fadeFar   = smoothStep(0.f, std::max(pointFar,   gridSpacing), far);

            const float borderFade = fadeLeft * fadeRight * fadeNear * fadeFar;

            const float tiltedY =
                point.y * (1.0f + pointTilt[i] * (1.0f - influence));
            const float contribution =
                tiltedY * influence * borderFade;
            // const float contribution =
            //     point.y * (1.0f + pointTilt[i]) * influence * borderFade;

            height = smoothMax(height, contribution, blendFactor);
        }

        // Micro-detalle
        const float micro = (fbm(glm::vec2(x, z) * 8.0f) - 0.5f);
        height += micro * 0.02f;

        return height;
    };

    // 4. Cuadrícula + normales analíticas
    vertices.clear();
    vertices.reserve(resolution * resolution);
    normals.clear();
    normals.reserve(resolution * resolution * 3);

    for (int z = 0; z < resolution; ++z) {
        for (int x = 0; x < resolution; ++x) {
            const float worldX = -h + size * static_cast<float>(x) / (resolution - 1);
            const float worldZ = -h + size * static_cast<float>(z) / (resolution - 1);
            const float height = sampleHeight(worldX, worldZ);
            vertices.emplace_back(worldX, height, worldZ);
        }
    }

    const float eps = gridSpacing * 0.5f;
    for (int z = 0; z < resolution; ++z) {
        for (int x = 0; x < resolution; ++x) {
            const float worldX = -h + size * static_cast<float>(x) / (resolution - 1);
            const float worldZ = -h + size * static_cast<float>(z) / (resolution - 1);

            const float hL = sampleHeight(worldX - eps, worldZ);
            const float hR = sampleHeight(worldX + eps, worldZ);
            const float hD = sampleHeight(worldX, worldZ - eps);
            const float hU = sampleHeight(worldX, worldZ + eps);

            glm::vec3 n = glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));

            normals.push_back(n.x);
            normals.push_back(n.y);
            normals.push_back(n.z);
        }
    }

    // 5. Índices
    indices.clear();
    indices.reserve((resolution - 1) * (resolution - 1) * 6);

    for (int z = 0; z < resolution - 1; ++z) {
        for (int x = 0; x < resolution - 1; ++x) {
            const uint32_t a = z * resolution + x;
            const uint32_t b = a + 1;
            const uint32_t c = a + resolution;
            const uint32_t d = c + 1;

            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);

            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    // 6. min/max para el shader
    minHeight = vertices.front().y;
    maxHeight = vertices.front().y;
    for (const auto& v : vertices) {
        minHeight = std::min(minHeight, v.y);
        maxHeight = std::max(maxHeight, v.y);
    }

    // 7. Subir a GPU
    std::vector<float> positions;
    positions.reserve(vertices.size() * 3);
    for (const auto& p : vertices) {
        positions.push_back(p.x);
        positions.push_back(p.y);
        positions.push_back(p.z);
    }

    plane->upload(positions, normals, indices, GL_TRIANGLES);
}

Terrain::Terrain(const char* arg) : Terrain(arg ? std::string(arg) : std::string()) {}

Terrain::Terrain(const std::string mapath)
{
	if (mapath.empty())
		throw std::runtime_error("missing path");
	parse(mapath);
	cfg.title = std::filesystem::path(mapath).filename().string();
	;
	cfg.width  = 1000;
	cfg.height = 700;
	initEnv();

	initHooks();

	return;
}

void Terrain::addWindow(WWindow::Config& newcfg)
{
	cfg.title		 = newcfg.title;
	cfg.width		 = newcfg.width;
	cfg.height		 = newcfg.height;
	cfg.x			 = newcfg.x;
	cfg.y			 = newcfg.y;
	cfg.shareContext = newcfg.shareContext;
	cfg.glMajor		 = newcfg.glMajor;
	cfg.glMinor		 = newcfg.glMinor;
	cfg.coreProfile	 = newcfg.coreProfile;
	cfg.resizable	 = newcfg.resizable;
	cfg.visible		 = newcfg.visible;
	cfg.decorated	 = newcfg.decorated;
	cfg.vsync		 = newcfg.vsync;
	cfg.samples		 = newcfg.samples;
}

void Terrain::initHooks()
{
	env.setUpdate([&](WWindow& w, float dt) {
		Input& in = w.input();
		if (in.isKeyPressed(Key::Code::Escape))
			w.requestClose();

		// Cámara
		float	  speed = 3.f * dt;
		glm::vec3 fwd	= cam.forward();
		glm::vec3 right = cam.right();
		glm::vec3 up	= cam.up();
		glm::vec3 move{0.f};
		if (in.isKeyDown(Key::Code::W))
			move += fwd;
		if (in.isKeyDown(Key::Code::S))
			move -= fwd;
		if (in.isKeyDown(Key::Code::D))
			move += right;
		if (in.isKeyDown(Key::Code::A))
			move -= right;
		if (glm::length(move) > 1e-6f)
			cam.move(glm::normalize(move) * speed);

		if (in.isMouseButtonDown(Mouse::Button::Right)) {
			const float s = 0.02f;
			cam.move(right * (-static_cast<float>(in.mouseDeltaX()) * s) +
					 up * (static_cast<float>(in.mouseDeltaY()) * s));
		}

		if (in.isMouseButtonDown(Mouse::Button::Left)) {
			const float sens = 0.01f;
			rotY += static_cast<float>(in.mouseDeltaX()) * sens;
			rotX += static_cast<float>(in.mouseDeltaY()) * sens;

			const float lim = glm::radians(89.f);
			if (rotX > lim)
				rotX = lim;
			if (rotX < -lim)
				rotX = -lim;
		}

		if (in.scrollDelta() != 0.0)
			cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);

		// Reset de rotación
		if (in.isKeyPressed(Key::Code::R)) {
			rotX = 0.f;
			rotY = 0.f;
		}
		if (in.isKeyPressed(Key::Code::F)) {
			wireframe = !wireframe;
		}
	});
}

void Terrain::initEnv()
{
	env.setInit([&](WWindow& w) {
		shader = std::make_shared<Shader>();
		if (!shader->loadFromFile("./src/terrain/shader/meshVertex.glsl",
								  "./src/terrain/shader/meshFrag.frag")) {
			std::fprintf(stderr, "shader failed\n");
			return;
		}

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
		model			= glm::rotate(model, rotY, glm::vec3(0.f, 1.f, 0.f));
		model			= glm::rotate(model, rotX, glm::vec3(1.f, 0.f, 0.f));

		glm::mat4 vp  = cam.projection(w.aspect()) * cam.view();
		glm::mat4 mvp = vp * model;

        shader->use();
        shader->setMat4("uViewProj", mvp);
        shader->setMat4("uModel", model);
        shader->setFloat("uMinHeight", minHeight);
        shader->setFloat("uMaxHeight", maxHeight);
        shader->setFloat("uTime", static_cast<float>(glfwGetTime()));
        shader->setFloat("uMapHeight", originalPeak);   // NUEVO: pico real en escala de points
                glPolygonMode(
                    GL_FRONT_AND_BACK,
                    wireframe ? GL_LINE : GL_FILL
                );

		plane->draw();

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	});

	env.run();
}