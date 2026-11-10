/**
 * @file Particles.cpp
 * @brief Implementation of the SPH (Smoothed Particle Hydrodynamics) simulation.
 *
 * Handles the CPU-based physics (density, pressure, viscosity, XSPH), spatial
 * hashing for neighbor search, boundary collisions, rain recycling, and
 * data preparation for the GPU rendering pipeline.
 */

#include <Particles.hpp>
#include <Shader.hpp>
#include <algorithm>
#include <cmath>
#include <gtc/type_ptr.hpp>
#include <random>

namespace
{

constexpr float PI = 3.14159265358979323846f;

/**
 * @brief Poly6 kernel for density calculation.
 * W(r) = 315/(64 * pi * h^9) * (h^2 - r^2)^3
 * @param r2 Squared distance between particles.
 * @param h Smoothing radius.
 * @param h2 Squared smoothing radius.
 * @return The kernel value.
 */
float kernelPoly6(float r2, float h, float h2)
{
	if (r2 >= h2)
		return 0.f;
	float diff = h2 - r2;
	return 315.f / (64.f * PI * std::pow(h, 9.f)) * diff * diff * diff;
}

/**
 * @brief Spiky kernel gradient magnitude for pressure forces.
 * F(r) = -45/(pi * h^6) * (h - r)^2
 * @param r Distance between particles.
 * @param h Smoothing radius.
 * @return The gradient magnitude.
 */
float kernelSpikyGrad(float r, float h)
{
	if (r >= h || r < 1e-6f)
		return 0.f;
	float diff = h - r;
	return -45.f / (PI * std::pow(h, 6.f)) * diff * diff;
}

/**
 * @brief Laplacian kernel for viscosity forces.
 * Laplacian W(r) = 45/(pi * h^6) * (h - r)
 * @param r Distance between particles.
 * @param h Smoothing radius.
 * @return The Laplacian value.
 */
float kernelLaplacian(float r, float h)
{
	if (r >= h)
		return 0.f;
	return 45.f / (PI * std::pow(h, 6.f)) * (h - r);
}

}  // namespace

/** @brief Default constructor. */
Particles::Particles() = default;

/** @brief Default destructor. */
Particles::~Particles() = default;

/**
 * @brief Computes the spatial hash grid cell for a given position.
 * @param p The particle position.
 * @return The 3D integer coordinates of the cell.
 */
glm::ivec3 Particles::cellOf(const glm::vec3& p) const
{
	return glm::ivec3(glm::floor(p / m_cfg.h));
}

/**
 * @brief Computes a unique hash key for a 3D grid cell.
 * @param c The 3D cell coordinates.
 * @return The hashed integer key.
 */
int64_t Particles::hashCell(const glm::ivec3& c) const
{
	constexpr int64_t p1 = 73856093;
	constexpr int64_t p2 = 19349663;
	constexpr int64_t p3 = 83492791;
	return (int64_t(c.x) * p1) ^ (int64_t(c.y) * p2) ^ (int64_t(c.z) * p3);
}

/**
 * @brief Builds the spatial hash map for the current particle positions.
 */
void Particles::buildHash()
{
	m_hash.clear();
	m_hash.reserve(m_particles.size() * 2);
	for (size_t i = 0; i < m_particles.size(); ++i)
		m_hash[hashCell(cellOf(m_particles[i].position))].push_back(i);
}

/**
 * @brief Iterates over all valid neighbors of a particle within the smoothing radius.
 * @tparam Fn The callback function type.
 * @param i Index of the particle to find neighbors for.
 * @param fn The function to execute for each neighbor index `j`.
 */
template <typename Fn> void Particles::forEachNeighbor(size_t i, Fn&& fn) const
{
	glm::ivec3 c = cellOf(m_particles[i].position);
	for (int dx = -1; dx <= 1; ++dx)
		for (int dy = -1; dy <= 1; ++dy)
			for (int dz = -1; dz <= 1; ++dz) {
				auto it = m_hash.find(hashCell(c + glm::ivec3(dx, dy, dz)));
				if (it == m_hash.end())
					continue;
				for (size_t j : it->second) {
					if (i == j)
						continue;
					fn(j);
				}
			}
}

/**
 * @brief Performs a single SPH physics step (density, pressure, forces, XSPH).
 * @param dt Delta time for the step.
 * @note buildHash() is NOT called here; it must be called externally once per frame.
 */
void Particles::stepSPH(float dt)
{
	const float	 h	= m_cfg.h;
	const float	 h2 = h * h;
	const size_t n	= m_particles.size();

	// 1. Density and pressure computation.
	for (size_t i = 0; i < n; ++i) {
		Particle& pi	  = m_particles[i];
		float	  density = 0.f;
		forEachNeighbor(i, [&](size_t j) {
			const glm::vec3 d = pi.position - m_particles[j].position;
			density += m_cfg.particleMass * kernelPoly6(glm::dot(d, d), h, h2);
		});
		pi.density = std::max(density, m_cfg.restDensity * 0.1f);

		// Calculate pressure.
		pi.pressure = m_cfg.stiffness * (pi.density - m_cfg.restDensity);

		// KEY: Prevent tensile instability.
		// Phase 5: Weaker cohesion => water spreads and flattens instead
		// of forming gelatinous clumps.
		float minPressure = -0.002f * m_cfg.stiffness;
		pi.pressure		  = std::max(pi.pressure, minPressure);

		pi.force = glm::vec3(0.f, m_cfg.gravity, 0.f);
	}

	// 2. Forces (pressure + viscosity).
	for (size_t i = 0; i < n; ++i) {
		Particle& pi = m_particles[i];
		glm::vec3 fPress{0.f};
		glm::vec3 fVisc{0.f};
		forEachNeighbor(i, [&](size_t j) {
			const Particle& pj = m_particles[j];
			glm::vec3		d  = pi.position - pj.position;
			float			r  = glm::length(d);
			if (r >= h || r < 1e-6f)
				return;
			glm::vec3 dir = d / r;

			float pTerm = (pi.pressure + pj.pressure) / (pi.density * pj.density);
			fPress -= dir * (pTerm * kernelSpikyGrad(r, h));

			fVisc += (pj.velocity - pi.velocity) *
					 (m_cfg.viscosity * kernelLaplacian(r, h) / pj.density);
		});
		pi.force += fPress + fVisc;
	}

	// 3. XSPH: normalized velocity smoothing. Removes jitter and clumps
	// without adding excessive artificial viscosity (Phase 5).
	const float xsph = 0.10f;
	for (size_t i = 0; i < n; ++i) {
		Particle& pi = m_particles[i];
		glm::vec3 dv{0.f};
		float	  wsum = 0.f;
		forEachNeighbor(i, [&](size_t j) {
			const Particle& pj = m_particles[j];
			glm::vec3		d  = pi.position - pj.position;
			float			r2 = glm::dot(d, d);
			if (r2 < h2) {
				float w = kernelPoly6(r2, h, h2);
				dv += (pj.velocity - pi.velocity) * w;
				wsum += w;
			}
		});
		if (wsum > 1e-12f)
			pi.velocity += xsph * (dv / wsum);
	}
	(void)dt;
}

/**
 * @brief Integrates the particle positions using semi-implicit Euler.
 * @param dt Delta time.
 */
void Particles::integrate(float dt)
{
	for (auto& p : m_particles) {
		p.velocity += p.force * dt;	 // Semi-implicit Euler
		p.velocity *= m_cfg.damping;

		float speed = glm::length(p.velocity);
		if (speed > 10.0f) {
			p.velocity = (p.velocity / speed) * 10.0f;	// Clamp maximum speed
		}

		p.position += p.velocity * dt;
		p.age += dt;
	}
}

/**
 * @brief Handles collisions with the ground (or terrain) and boundary walls.
 */
void Particles::collide()
{
	const glm::vec3 lo = m_cfg.boundsMin;
	const glm::vec3 hi = m_cfg.boundsMax;
	const float		e  = m_cfg.wallRestitution;

	for (auto& p : m_particles) {
		// Ground collision (terrain or flat plane at lo.y)
		const float ground = m_heightFn ? m_heightFn(p.position.x, p.position.z) : lo.y;
		if (p.position.y < ground) {
			p.position.y = ground;
			if (p.velocity.y < 0.f)
				p.velocity.y = -p.velocity.y * e;
			p.velocity.x *= 0.98f;
			p.velocity.z *= 0.98f;
		}

		// Container walls
		if (p.position.x < lo.x) {
			p.position.x = lo.x;
			p.velocity.x = -p.velocity.x * e;
		}
		if (p.position.x > hi.x) {
			p.position.x = hi.x;
			p.velocity.x = -p.velocity.x * e;
		}
		if (p.position.z < lo.z) {
			p.position.z = lo.z;
			p.velocity.z = -p.velocity.z * e;
		}
		if (p.position.z > hi.z) {
			p.position.z = hi.z;
			p.velocity.z = -p.velocity.z * e;
		}
		if (p.position.y > hi.y) {
			p.position.y = hi.y;
			p.velocity.y = -p.velocity.y * e;
		}
	}
}

/**
 * @brief Recycles rain drops that have hit the ground by respawning them at the top.
 */
void Particles::recycleRain()
{
	const float ground = m_cfg.boundsMin.y;

	for (auto& p : m_particles) {
		if (!p.rainDrop)
			continue;
		const float g = m_heightFn ? m_heightFn(p.position.x, p.position.z) : ground;
		if (p.position.y <= g + 0.05f)
			spawnOne(p, Mode::Rain, /*initial=*/false);
	}
}

/**
 * @brief Main update loop for the particle simulation.
 *
 * Executes the physics substeps, classifies particles for the GPU hybrid
 * rendering (puddle vs. falling drops), and updates the rendering buffers.
 *
 * @param dt Delta time since the last frame.
 */
void Particles::update(float dt)
{
	if (m_particles.empty())
		return;

	if (dt > 0.033f)
		dt = 0.033f;
	buildHash();

	const float subDt = dt / static_cast<float>(m_cfg.substeps);
	for (int s = 0; s < m_cfg.substeps; ++s) {
		stepSPH(subDt);
		integrate(subDt);
		collide();
		if (m_mode == Mode::Rain)
			recycleRain();
	}

	// GPU buffers + hybrid water classification.
	// In Rain: fast drops or in the upper band -> points only.
	// Slow water (puddle) -> splatWeight = 1 -> Marching Cubes.
	const bool	rain = (m_mode == Mode::Rain);
	const float airY = m_cfg.boundsMax.y - m_cfg.rainAirBand;
	m_fallCount		 = 0;

	// Phase 5: Real puddle surface = max Y of particles that contribute
	// to density (non-airborne). Used to hide drops exactly at the iso-level,
	// rather than using a fixed threshold.
	float puddleTop = m_cfg.boundsMin.y;

	for (size_t i = 0; i < m_particles.size(); ++i) {
		Particle& p = m_particles[i];

		const bool airborne =
			rain &&
			(p.rainDrop || glm::length(p.velocity) > m_cfg.rainSettleSpeed || p.position.y > airY);
		p.splatWeight = airborne ? 0.f : 1.f;
		if (!airborne)
			puddleTop = std::max(puddleTop, p.position.y);

		m_positions[i * 3 + 0] = p.position.x;
		m_positions[i * 3 + 1] = p.position.y;
		m_positions[i * 3 + 2] = p.position.z;
		m_densities[i]		   = p.density;

		if (airborne) {
			const size_t k				= m_fallCount++;
			m_fallPositions[k * 3 + 0]	= p.position.x;
			m_fallPositions[k * 3 + 1]	= p.position.y;
			m_fallPositions[k * 3 + 2]	= p.position.z;
			m_fallVelocities[k * 3 + 0] = p.velocity.x;
			m_fallVelocities[k * 3 + 1] = p.velocity.y;
			m_fallVelocities[k * 3 + 2] = p.velocity.z;
			m_fallDensities[k]			= p.density;
		}
	}

	m_puddleTopY = puddleTop;

	if (rain)
		m_fallMesh.updatePoints(m_fallPositions, m_fallDensities, m_fallVelocities);

	// Debug: every 30 frames
	if (++m_debugCounter % 30 == 0) {
		float avg = 0.f;
		for (const auto& p : m_particles)
			avg += p.density;
		avg /= static_cast<float>(m_particles.size());
		std::printf("[sph] avg density = %.1f  (rest = %.1f)\n", avg, m_cfg.restDensity);
	}
}

/**
 * @brief Spawns/resets a single particle based on the simulation mode.
 * @param p Reference to the particle to initialize.
 * @param mode The current simulation mode.
 * @param initial True if this is the initial spawn, false if it's a recycled drop.
 */
void Particles::spawnOne(Particle& p, Mode mode, bool initial)
{
	static std::mt19937					  rng(0xC0FFEE);
	std::uniform_real_distribution<float> ux(m_cfg.boundsMin.x + 0.05f, m_cfg.boundsMax.x - 0.05f);
	std::uniform_real_distribution<float> uz(m_cfg.boundsMin.z + 0.05f, m_cfg.boundsMax.z - 0.05f);
	std::uniform_real_distribution<float> u01(0.f, 1.f);

	const float x	   = ux(rng);
	const float z	   = uz(rng);
	const float ground = m_heightFn ? m_heightFn(x, z) : m_cfg.boundsMin.y;

	switch (mode) {
	case Mode::UniformRise:
		p.position = {x, ground + 0.02f + (initial ? u01(rng) * 0.5f : 0.f), z};
		p.velocity = {0.f, 0.f, 0.f};
		break;
	case Mode::Wave:
		p.position = {m_cfg.boundsMin.x + 0.1f, ground + 0.1f + u01(rng) * 0.3f, z};
		p.velocity = {3.f + u01(rng) * 0.5f, 0.f, 0.f};
		break;
	case Mode::Rain:
		if (initial) {
			const float span = m_cfg.boundsMax.y - ground;
			p.position		 = {x, ground + 0.1f + u01(rng) * std::max(span - 0.2f, 0.f), z};
			p.velocity		 = {0.f, -3.0f - u01(rng) * 2.0f, 0.f};
		} else {
			p.position = {x, m_cfg.boundsMax.y - u01(rng) * 0.15f, z};
			p.velocity = {0.f, -3.0f - u01(rng) * 1.5f, 0.f};
		}
		break;
	}
	p.age		  = initial ? u01(rng) * 2.f : 0.f;
	p.density	  = m_cfg.restDensity;
	p.pressure	  = 0.f;
	p.splatWeight = 1.f;
}

/**
 * @brief Spawns all particles based on the specified mode.
 * @param mode The simulation mode.
 */
void Particles::spawnAll(Mode mode)
{
	m_fallCount = 0;
	m_particles.resize(m_positions.size() / 3);

	const size_t rainDrops =
		(mode == Mode::Rain)
			? static_cast<size_t>(static_cast<float>(m_particles.size()) * m_cfg.rainDropFraction)
			: 0;

	for (size_t i = 0; i < m_particles.size(); ++i) {
		spawnOne(m_particles[i], mode, /*initial=*/true);
		m_particles[i].rainDrop = (i < rainDrops);
	}

	m_densities.assign(m_particles.size(), m_cfg.restDensity);
}

/**
 * @brief Initializes the simulation with a specified number of particles.
 * @param count Number of particles.
 * @param mode The initial simulation mode.
 */
void Particles::init(size_t count, Mode mode)
{
	init(count, mode, m_cfg);  // uses current config (setConfig or defaults)
}

/**
 * @brief Initializes the simulation with a specific configuration.
 * @param count Number of particles.
 * @param mode The initial simulation mode.
 * @param cfg The configuration to use.
 */
void Particles::init(size_t count, Mode mode, const Config& cfg)
{
	m_cfg		   = cfg;
	m_mode		   = mode;
	m_debugCounter = 0;

	m_particles.clear();
	m_particles.reserve(count);
	m_positions.assign(count * 3, 0.f);
	m_densities.assign(count, m_cfg.restDensity);
	m_fallPositions.assign(count * 3, 0.f);
	m_fallDensities.assign(count, 0.f);
	m_fallVelocities.assign(count * 3, 0.f);
	m_fallCount = 0;

	spawnAll(mode);
	m_mesh.uploadPoints(m_positions, m_densities);
	m_fallMesh.uploadPoints(m_fallPositions, m_fallDensities, m_fallVelocities);
}

/**
 * @brief Resets the simulation to its initial state.
 * @param mode The simulation mode to reset to.
 */
void Particles::reset(Mode mode)
{
	m_mode = mode;
	spawnAll(mode);
	m_mesh.updatePoints(m_positions, m_densities);
}

/** @brief Uploads the particle position and density data to the GPU. */
void Particles::uploadToGPU()
{
	m_mesh.updatePoints(m_positions, m_densities);
}

/**
 * @brief Renders all particles using the provided shader.
 * @param shader The shader to use for rendering.
 * @param viewProj The combined view-projection matrix.
 */
void Particles::render(Shader& shader, const glm::mat4& viewProj)
{
	shader.use();
	shader.setMat4("uViewProj", viewProj);
	shader.setFloat("uPointSize", m_cfg.pointSize);
	shader.setVec3("uColorLow", m_cfg.colorLow);
	shader.setVec3("uColorHigh", m_cfg.colorHigh);
	shader.setFloat("uAlpha", m_cfg.alpha);
	shader.setFloat("uDensityMin", m_cfg.restDensity * 0.9f);
	shader.setFloat("uDensityMax", m_cfg.restDensity * 1.3f);
	shader.setFloat("uHideBelowY", -1e30f);
	shader.setFloat("uSplashSpeed", m_cfg.rainSplashSpeed);
	m_mesh.drawPoints();
}

/**
 * @brief Renders only the airborne rain drops.
 * @param shader The shader to use for rendering.
 * @param viewProj The combined view-projection matrix.
 */
void Particles::renderRainDrops(Shader& shader, const glm::mat4& viewProj)
{
	if (m_fallCount == 0)
		return;

	shader.use();
	shader.setMat4("uViewProj", viewProj);
	shader.setFloat("uPointSize", m_cfg.rainPointSize);
	shader.setVec3("uColorLow", m_cfg.rainColor);
	shader.setVec3("uColorHigh", m_cfg.rainColor);
	shader.setFloat("uAlpha", m_cfg.rainAlpha);
	shader.setFloat("uDensityMin", m_cfg.restDensity * 0.9f);
	shader.setFloat("uDensityMax", m_cfg.restDensity * 1.3f);
	// Phase 5: dynamic threshold locked to the real puddle surface.
	// Drops falling below this height are hidden (if at rest); splashes
	// remain visible thanks to uSplashSpeed.
	shader.setFloat("uHideBelowY", m_puddleTopY + m_cfg.rainHideMargin);
	shader.setFloat("uSplashSpeed", m_cfg.rainSplashSpeed);
	m_fallMesh.drawPoints(static_cast<GLsizei>(m_fallCount));
}
