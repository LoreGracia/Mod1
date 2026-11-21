#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <Terrain.hpp>
#include <GLFW/glfw3.h>
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

	shader = std::move(t.shader);
	plane  = std::move(t.plane);

	m_heightTexture = t.m_heightTexture;
	t.m_heightTexture = 0;

	size		 = t.size;
	m_minHeight	 = t.m_minHeight;
	m_maxHeight	 = t.m_maxHeight;
	originalPeak = t.originalPeak;

	points	 = std::move(t.points);
	indices	 = std::move(t.indices);
	vertices = std::move(t.vertices);
	normals	 = std::move(t.normals);
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
    m_resolution = resolution;
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
    m_minHeight = vertices.front().y;
    m_maxHeight = vertices.front().y;
    for (const auto& v : vertices) {
        m_minHeight = std::min(m_minHeight, v.y);
        m_maxHeight = std::max(m_maxHeight, v.y);
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

    /**
     * @brief Publishes the generated heightfield as an R32F texture.
     *
     * Texel (x, z) stores the same elevation as terrain vertex (x, z), so the
     * SPH collision pass and the water shaders sample the exact same surface
     * that is rendered by the terrain mesh. Linear filtering smooths the
     * height between vertices and clamp-to-edge keeps the border stable.
     */
    std::vector<float> heightData(static_cast<std::size_t>(resolution) *
                                  static_cast<std::size_t>(resolution));
    for (int z = 0; z < resolution; ++z) {
        for (int x = 0; x < resolution; ++x) {
            heightData[static_cast<std::size_t>(z) * resolution + x] =
                vertices[static_cast<std::size_t>(z) * resolution + x].y;
        }
    }

    if (m_heightTexture)
        glDeleteTextures(1, &m_heightTexture);
    glGenTextures(1, &m_heightTexture);
    glBindTexture(GL_TEXTURE_2D, m_heightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, resolution, resolution, 0,
                 GL_RED, GL_FLOAT, heightData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Terrain::Terrain(const char* arg) : Terrain(arg ? std::string(arg) : std::string()) {}

Terrain::Terrain(const std::string mapath)
{
	if (mapath.empty())
		throw std::runtime_error("missing path");
	parse(mapath);
}

/**
 * @brief Creates the GPU resources of the terrain.
 *
 * Must be called once a valid OpenGL context is current. The shader, the mesh
 * and the shared height texture are built from the parsed heightfield.
 */
void Terrain::init()
{
	shader = std::make_shared<Shader>();
	if (!shader->loadFromFile("./src/terrain/shader/meshVertex.glsl",
							  "./src/terrain/shader/meshFrag.frag")) {
		std::fprintf(stderr, "shader failed\n");
		return;
	}

	plane = std::make_shared<Mesh>();
	generateTerrain();
}

/**
 * @brief Samples the generated heightfield at a world-space XZ position.
 *
 * Bilinear interpolation matches the R32F texture sampled by the water
 * shaders. Coordinates outside the footprint clamp to the edge texels.
 *
 * @param xz World-space X and Z coordinates.
 * @return Interpolated terrain elevation in world units.
 */
float Terrain::heightAt(glm::vec2 xz) const
{
	if (vertices.empty() || m_resolution < 2)
		return m_minHeight;

	const glm::vec2 lo = footprintMin();
	const glm::vec2 hi = footprintMax();
	const glm::vec2 extent = glm::max(hi - lo, glm::vec2(1e-4f));
	const glm::vec2 uv = glm::clamp((xz - lo) / extent, 0.0f, 1.0f);

	const float gx = uv.x * static_cast<float>(m_resolution - 1);
	const float gz = uv.y * static_cast<float>(m_resolution - 1);
	const int x0 = std::min(static_cast<int>(gx), m_resolution - 2);
	const int z0 = std::min(static_cast<int>(gz), m_resolution - 2);
	const float fx = glm::clamp(gx - static_cast<float>(x0), 0.0f, 1.0f);
	const float fz = glm::clamp(gz - static_cast<float>(z0), 0.0f, 1.0f);

	const auto sample = [&](int x, int z) {
		return vertices[static_cast<std::size_t>(z) * static_cast<std::size_t>(m_resolution) +
						static_cast<std::size_t>(x)].y;
	};

	return glm::mix(glm::mix(sample(x0, z0), sample(x0 + 1, z0), fx),
					glm::mix(sample(x0, z0 + 1), sample(x0 + 1, z0 + 1), fx), fz);
}

/**
 * @brief Draws the terrain with an identity model matrix.
 *
 * Keeping the model at the identity guarantees that the terrain, the CPU rain
 * drops and the Marching Cubes mesh share one global coordinate system.
 *
 * @param viewProj Combined view-projection matrix of the active camera.
 */
void Terrain::render(const glm::mat4& viewProj)
{
	shader->use();
	shader->setMat4("uViewProj", viewProj);
	shader->setMat4("uModel", glm::mat4(1.0f));
	shader->setFloat("uMinHeight", m_minHeight);
	shader->setFloat("uMaxHeight", m_maxHeight);
	shader->setFloat("uTime", static_cast<float>(glfwGetTime()));
	shader->setFloat("uMapHeight", originalPeak);

	plane->draw();
}

/**
 * @brief Releases the OpenGL height texture owned by this instance.
 */
void Terrain::release()
{
	if (m_heightTexture) {
		glDeleteTextures(1, &m_heightTexture);
		m_heightTexture = 0;
	}
}

Terrain::~Terrain()
{
	release();
}
