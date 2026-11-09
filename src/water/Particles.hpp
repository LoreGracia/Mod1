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
			float restDensity	= 400.f;
			float stiffness		= 150.f;
			float viscosity		= 0.08f;
			float gravity		= -9.8f;
			float damping		= 0.999f;
			int   substeps		= 4;
			float particleMass = 0.2f;

			// Colisión
			glm::vec3 boundsMin{-1.f, 0.f, -1.f};
			glm::vec3 boundsMax{ 1.f, 3.f,  1.f};
			float wallRestitution = 0.3f;

			// Render
			float     pointSize = 8.f;
			glm::vec3 colorLow {0.2f, 0.6f, 1.0f};
			glm::vec3 colorHigh{0.05f, 0.1f, 0.5f};
			float     alpha    = 0.7f;

			// Lluvia híbrida (puntos en el aire + malla en el charco)
			float     rainDropFraction = 0.15f; // % reciclado para lluvia continua
			float     rainSettleSpeed = 2.0f;  // |v| menor => ya es charco
			float     rainAirBand     = 0.25f; // banda superior siempre "en el aire"
			float     rainPointSize   = 6.0f;
			float     rainAlpha       = 0.75f;
			glm::vec3 rainColor{0.55f, 0.80f, 1.0f};
			// Fase 3.7: altura (sobre boundsMin.y) por debajo de la cual una
			// gota ya es charco y deja de dibujarse como punto. Debe quedar
			// un poco por encima de uSplatMaxHeight para tapar los puntos
			// que quedarían dentro de la lámina de Marching Cubes.
			float     rainHideBelowHeight = 0.30f;
		};

		Particles();
		~Particles();

		void init(size_t count, Mode mode);
		void init(size_t count, Mode mode, const Config& cfg);
		void update(float dt);
		void uploadToGPU();
		void render(Shader& shader, const glm::mat4& viewProj);
		void renderRainDrops(Shader& shader, const glm::mat4& viewProj);
		const std::vector<Particle>& getParticles() const { return m_particles; }

		void reset(Mode mode);
		void setConfig(const Config& cfg) { m_cfg = cfg; }

		// Colisión con terreno. Si no se pone, se usa plana a y = boundsMin.y.
		using HeightFn = std::function<float(float x, float z)>;
		void setTerrainHeight(HeightFn fn) { m_heightFn = std::move(fn); }

		size_t count() const { return m_particles.size(); }

	private:
		// ── Física ──────────────────────────────────────────────────────
		void stepSPH(float dt);
		float sampleDensity(const glm::vec3& point) const;
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
		void recycleRain();

		// ── Estado ─────────────────────────────────────────────────────
		Config					m_cfg;
		Mode					m_mode = Mode::UniformRise;
		std::vector<Particle>	m_particles;
		std::vector<float>		m_positions;   // flat vec3
		std::vector<float>		m_densities;   // flat float
		std::vector<float>		m_fallPositions; // gotas en el aire (subconjunto)
		std::vector<float>		m_fallDensities;
		size_t					m_fallCount = 0;
		Mesh					m_mesh;
		Mesh					m_fallMesh;
		HeightFn				m_heightFn;
		int						m_debugCounter = 0;

		// Spatial hash: cell hash -> índices de partículas en esa celda
		std::unordered_map<int64_t, std::vector<size_t>> m_hash;
	};