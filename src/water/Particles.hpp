#pragma once
#include <Mesh.hpp>
#include <Particle.hpp>

#include <glm.hpp>
#include <functional>
#include <unordered_map>
#include <vector>

class Shader;

class Particles {
	public:
		enum class Mode { UniformRise, Wave, Rain };

		struct Config {
			// SPH
			float h				= 0.12f;     // radio de influencia
			float restDensity	= 1000.f;
			float stiffness		= 250.f;
			float viscosity		= 0.08f;
			float gravity		= -9.8f;
			float damping		= 0.999f;
			int   substeps		= 4;

			// Colisión
			glm::vec3 boundsMin{-1.f, 0.f, -1.f};
			glm::vec3 boundsMax{ 1.f, 3.f,  1.f};
			float wallRestitution = 0.3f;

			// Render
			float     pointSize = 8.f;
			glm::vec3 colorLow {0.2f, 0.6f, 1.0f};
			glm::vec3 colorHigh{0.05f, 0.1f, 0.5f};
			float     alpha    = 0.7f;
		};

		Particles();
		~Particles();

		void init(size_t count, Mode mode);
		void update(float dt);
		void uploadToGPU();
		void render(Shader& shader, const glm::mat4& viewProj);

		void reset(Mode mode);
		void setConfig(const Config& cfg) { m_cfg = cfg; }

		// Colisión con terreno. Si no se pone, se usa plana a y = boundsMin.y.
		using HeightFn = std::function<float(float x, float z)>;
		void setTerrainHeight(HeightFn fn) { m_heightFn = std::move(fn); }

		size_t count() const { return m_particles.size(); }

	private:
		// ── Física ──────────────────────────────────────────────────────
		void stepSPH(float dt);
		void integrate(float dt);
		void collide();

		// ── Spatial hashing ────────────────────────────────────────────
		glm::ivec3 cellOf(const glm::vec3& p) const;
		int64_t    hashCell(const glm::ivec3& c) const;
		void       buildHash();
		template <typename Fn> void forEachNeighbor(size_t i, Fn&& fn) const;

		// ── Spawn ──────────────────────────────────────────────────────
		void spawnAll(Mode mode);
		void spawnOne(Particle& p, Mode mode, bool initial);

		// ── Estado ─────────────────────────────────────────────────────
		Config					m_cfg;
		std::vector<Particle>	m_particles;
		std::vector<float>		m_positions;   // flat vec3
		std::vector<float>		m_densities;   // flat float
		Mesh					m_mesh;
		HeightFn				m_heightFn;

		// Spatial hash: cell hash -> índices de partículas en esa celda
		std::unordered_map<int64_t, std::vector<size_t>> m_hash;
	};