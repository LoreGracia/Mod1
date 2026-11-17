#include <Terrain.hpp>
#include <utils.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <regex>
#include <algorithm>
#include <cmath>

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
		points.emplace_back(numbers[i], numbers[i + 2], numbers[i + 1]);
	}
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
    constexpr int resolution = 100;
    const float h = size * 0.5f;

	const float gridSpacing = size / (resolution - 1);
	// Controla el ancho de los montículos.
	const float influenceScale = 0.5f;

	// Límites del radio de influencia.
	const float minInfluenceRadius = gridSpacing * 0.5f;
	const float maxInfluenceRadius = size * 0.3f;

	// Radio de influencia de cada punto.
	std::vector<float> influenceRadii(points.size(), 0.f);

	for (std::size_t i = 0; i < points.size(); ++i)
	{
		float nearestDistance = size;

		for (std::size_t j = 0; j < points.size(); ++j)
		{
			if (i == j)
				continue;

			const float dx = points[i].x - points[j].x;
			const float dz = points[i].z - points[j].z;

			const float distance = std::sqrt(dx * dx + dz * dz);
			nearestDistance = std::min(nearestDistance, distance);
		}

		influenceRadii[i] = std::clamp(
			nearestDistance * influenceScale,
			minInfluenceRadius,
			maxInfluenceRadius
		);
	}

	auto sampleHeight = [&](float x, float z) -> float
{
    float height = 0.f;

    // Distancia del vértice actual a cada lado del terreno.
    const float left   = x + h;
    const float right  = h - x;
    const float near   = z + h;
    const float far    = h - z;

    // Smoothstep manual.
    auto smoothStep = [](float edge0, float edge1, float value)
    {
        const float t = std::clamp(
            (value - edge0) / (edge1 - edge0),
            0.f,
            1.f
        );

        return t * t * (3.f - 2.f * t);
    };

    // Calcular la influencia de cada punto.
    for (std::size_t i = 0; i < points.size(); ++i)
    {
			const glm::vec3& point = points[i];

			const float dx = x - point.x;
			const float dz = z - point.z;

			const float distanceSquared = dx * dx + dz * dz;
			const float radius = influenceRadii[i];

			const float influence = std::exp(
				-distanceSquared / (2.f * radius * radius)
			);

			// Distancia del centro del montículo a cada borde.
			const float pointLeft  = point.x + h;
			const float pointRight = h - point.x;
			const float pointNear  = point.z + h;
			const float pointFar   = h - point.z;

			// Cada montículo adapta su transición al perímetro.
			const float fadeLeft = smoothStep(
				0.f, std::max(pointLeft, gridSpacing), left
			);

			const float fadeRight = smoothStep(
				0.f, std::max(pointRight, gridSpacing), right
			);

			const float fadeNear = smoothStep(
				0.f, std::max(pointNear, gridSpacing), near
			);

			const float fadeFar = smoothStep(
				0.f, std::max(pointFar, gridSpacing), far
			);

			const float borderFade =
				fadeLeft * fadeRight * fadeNear * fadeFar;

			const float contribution =
				point.y * influence * borderFade;

			height = std::max(height, contribution);
		}

		return height;
	};

    // --------------------------------------------------
    // 6. Crear la cuadrícula regular.
    // --------------------------------------------------

    for (int z = 0; z < resolution; ++z)
    {
        for (int x = 0; x < resolution; ++x)
        {
            const float worldX =
                -h + size * static_cast<float>(x) /
                (resolution - 1);

            const float worldZ =
                -h + size * static_cast<float>(z) /
                (resolution - 1);

            const float height = sampleHeight(worldX, worldZ);

            vertices.emplace_back(worldX, height, worldZ);
        }
    }

    // --------------------------------------------------
    // 7. Construir los triángulos de la cuadrícula.
    // --------------------------------------------------

    for (int z = 0; z < resolution - 1; ++z)
    {
        for (int x = 0; x < resolution - 1; ++x)
        {
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
	
	// // 7.5 
	// // Suavizado geométrico de las alturas
	// const int smoothIterations = 5;
	// const float smoothStrength = 0.25f;

	// std::vector<float> heights(vertices.size());
	// std::vector<float> newHeights(vertices.size());
	// std::vector<bool> fixed(vertices.size(), false);

	// // Guardar las alturas actuales
	// for (std::size_t i = 0; i < vertices.size(); ++i)
	// 	heights[i] = vertices[i].y;

	// // Mantener fijos los bordes
	// for (int z = 0; z < resolution; ++z) {
	// 	for (int x = 0; x < resolution; ++x) {
	// 		if (x == 0 || z == 0 ||
	// 			x == resolution - 1 || z == resolution - 1) {
	// 			fixed[z * resolution + x] = true;
	// 		}
	// 	}
	// }

	// // Proteger las posiciones correspondientes a los puntos originales
	// const float halfSize = size * 0.5f;

	// for (const auto& point : points) {
	// 	int x = static_cast<int>(std::round(
	// 		(point.x + halfSize) / size * (resolution - 1)
	// 	));

	// 	int z = static_cast<int>(std::round(
	// 		(point.z + halfSize) / size * (resolution - 1)
	// 	));

	// 	x = std::clamp(x, 0, resolution - 1);
	// 	z = std::clamp(z, 0, resolution - 1);

	// 	fixed[z * resolution + x] = true;
	// }

	// // Aplicar suavizado iterativo
	// for (int iteration = 0; iteration < smoothIterations; ++iteration) {
	// 	newHeights = heights;

	// 	for (int z = 1; z < resolution - 1; ++z) {
	// 		for (int x = 1; x < resolution - 1; ++x) {
	// 			const int index = z * resolution + x;

	// 			if (fixed[index])
	// 				continue;

	// 			const float average =
	// 				heights[index - resolution] +
	// 				heights[index + resolution] +
	// 				heights[index - 1] +
	// 				heights[index + 1];

	// 			const float neighborAverage = average * 0.25f;

	// 			newHeights[index] =
	// 				heights[index] +
	// 				smoothStrength * (neighborAverage - heights[index]);
	// 		}
	// 	}

	// 	heights.swap(newHeights);
	// }

	// // Aplicar las alturas suavizadas a la geometría
	// for (std::size_t i = 0; i < vertices.size(); ++i)
	// 	vertices[i].y = heights[i];

    // --------------------------------------------------
    // 8. Calcular normales.
    // --------------------------------------------------

    getNormals();

    // --------------------------------------------------
    // 9. Preparar posiciones.
    // --------------------------------------------------

    std::vector<float> positions;
    positions.reserve(vertices.size() * 3);

    for (const auto& p : vertices)
    {
        positions.push_back(p.x);
        positions.push_back(p.y);
        positions.push_back(p.z);
    }

    // --------------------------------------------------
    // 10. Subir geometría a OpenGL.
    // --------------------------------------------------

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
			cam.move(right * (static_cast<float>(in.mouseDeltaX()) * s) +
					 up * (-static_cast<float>(in.mouseDeltaY()) * s));
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
		model			= glm::rotate(model, rotY, glm::vec3(0.f, 1.f, 0.f));
		model			= glm::rotate(model, rotX, glm::vec3(1.f, 0.f, 0.f));

		glm::mat4 vp  = cam.projection(w.aspect()) * cam.view();
		glm::mat4 mvp = vp * model;

		shader->use();
		shader->setMat4("uViewProj", mvp);
		shader->setMat4("uModel", model);

		glPolygonMode(
			GL_FRONT_AND_BACK,
			wireframe ? GL_LINE : GL_FILL
		);

		plane->draw();

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	});

	env.run();
}