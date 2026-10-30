#include <Particles.hpp>
#include <Shader.hpp>

#include <gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <random>

namespace {

constexpr float PI = 3.14159265358979323846f;

	// ── Kernels SPH ─────────────────────────────────────────────────────

	// Poly6: densidad. W(r) = 315/(64π h^9) (h² - r²)³
	float kernelPoly6(float r2, float h, float h2)
	{
		if (r2 >= h2) return 0.f;
		float diff = h2 - r2;
		return 315.f / (64.f * PI * std::pow(h, 9.f)) * diff * diff * diff;
	}

	// Spiky gradient magnitude. F(r) = -45/(π h^6) (h - r)²
	float kernelSpikyGrad(float r, float h)
	{
		if (r >= h || r < 1e-6f) return 0.f;
		float diff = h - r;
		return -45.f / (PI * std::pow(h, 6.f)) * diff * diff;
	}

	// Laplacian (viscosidad). ∇²W(r) = 45/(π h^6) (h - r)
	float kernelLaplacian(float r, float h)
	{
		if (r >= h) return 0.f;
		return 45.f / (PI * std::pow(h, 6.f)) * (h - r);
	}

} // namespace

Particles::Particles() = default;
Particles::~Particles() = default;

// ── Spatial hashing ─────────────────────────────────────────────────

glm::ivec3 Particles::cellOf(const glm::vec3& p) const
{
	return glm::ivec3(glm::floor(p / m_cfg.h));
}

int64_t Particles::hashCell(const glm::ivec3& c) const
{
	// Hash de Cantor / primes para 3D
	constexpr int64_t p1 = 73856093;
	constexpr int64_t p2 = 19349663;
	constexpr int64_t p3 = 83492791;
	return (int64_t(c.x) * p1) ^ (int64_t(c.y) * p2) ^ (int64_t(c.z) * p3);
}

void Particles::buildHash()
{
	m_hash.clear();
	m_hash.reserve(m_particles.size() * 2);
	for (size_t i = 0; i < m_particles.size(); ++i)
		m_hash[hashCell(cellOf(m_particles[i].position))].push_back(i);
}

template <typename Fn>
void Particles::forEachNeighbor(size_t i, Fn&& fn) const
{
	glm::ivec3 c = cellOf(m_particles[i].position);
	for (int dx = -1; dx <= 1; ++dx)
	for (int dy = -1; dy <= 1; ++dy)
	for (int dz = -1; dz <= 1; ++dz) {
		auto it = m_hash.find(hashCell(c + glm::ivec3(dx, dy, dz)));
		if (it == m_hash.end()) continue;
		for (size_t j : it->second) {
			if (i == j) continue;
			fn(j);
		}
	}
}

// ── SPH ─────────────────────────────────────────────────────────────

void Particles::stepSPH(float dt)
{
	const float h  = m_cfg.h;
	const float h2 = h * h;
	const size_t n = m_particles.size();

	buildHash();

	// 1. Densidad y presión
	for (size_t i = 0; i < n; ++i) {
		Particle& pi = m_particles[i];
		float density = 0.f;
		forEachNeighbor(i, [&](size_t j) {
			const glm::vec3 d = pi.position - m_particles[j].position;
			density += kernelPoly6(glm::dot(d, d), h, h2);
		});
		pi.density  = std::max(density, m_cfg.restDensity * 0.1f);
		pi.pressure = m_cfg.stiffness * (pi.density - m_cfg.restDensity);
		pi.force    = glm::vec3(0.f, m_cfg.gravity, 0.f);
	}

	// 2. Fuerzas (presión + viscosidad)
	for (size_t i = 0; i < n; ++i) {
		Particle& pi = m_particles[i];
		glm::vec3 fPress{0.f};
		glm::vec3 fVisc {0.f};
		forEachNeighbor(i, [&](size_t j) {
			const Particle& pj = m_particles[j];
			glm::vec3 d = pi.position - pj.position;
			float r     = glm::length(d);
			if (r >= h || r < 1e-6f) return;
			glm::vec3 dir = d / r;

			// Presión (Spiky gradient)
			float pTerm = (pi.pressure + pj.pressure) / (2.f * pj.density);
			fPress += dir * (pTerm * kernelSpikyGrad(r, h));

			// Viscosidad (Laplacian)
			fVisc += (pj.velocity - pi.velocity) *
					(m_cfg.viscosity * kernelLaplacian(r, h) / pj.density);
		});
		pi.force += fPress + fVisc;
	}
	(void)dt;
}

void Particles::integrate(float dt)
{
	for (auto& p : m_particles) {
		// Euler semi-implícito
		p.velocity += p.force * dt;
		p.velocity *= m_cfg.damping;
		p.position += p.velocity * dt;
		p.age      += dt;
	}
}

void Particles::collide()
{
	const glm::vec3 lo = m_cfg.boundsMin;
	const glm::vec3 hi = m_cfg.boundsMax;
	const float     e  = m_cfg.wallRestitution;

	for (auto& p : m_particles) {
		// Suelo del terreno
		float ground = m_heightFn ? m_heightFn(p.position.x, p.position.z)
								: lo.y;
		if (p.position.y < ground) {
			p.position.y = ground;
			if (p.velocity.y < 0.f) p.velocity.y = -p.velocity.y * e;
			p.velocity.x *= 0.98f;
			p.velocity.z *= 0.98f;
		}

		// Paredes del contenedor
		if (p.position.x < lo.x) { p.position.x = lo.x; p.velocity.x = -p.velocity.x * e; }
		if (p.position.x > hi.x) { p.position.x = hi.x; p.velocity.x = -p.velocity.x * e; }
		if (p.position.z < lo.z) { p.position.z = lo.z; p.velocity.z = -p.velocity.z * e; }
		if (p.position.z > hi.z) { p.position.z = hi.z; p.velocity.z = -p.velocity.z * e; }
		if (p.position.y > hi.y) { p.position.y = hi.y; p.velocity.y = -p.velocity.y * e; }
	}
}

void Particles::update(float dt)
{
	if (m_particles.empty()) return;

	const float subDt = dt / static_cast<float>(m_cfg.substeps);
	for (int s = 0; s < m_cfg.substeps; ++s) {
		stepSPH(subDt);
		integrate(subDt);
		collide();
	}

	// Preparar buffers para GPU
	for (size_t i = 0; i < m_particles.size(); ++i) {
		m_positions[i * 3 + 0] = m_particles[i].position.x;
		m_positions[i * 3 + 1] = m_particles[i].position.y;
		m_positions[i * 3 + 2] = m_particles[i].position.z;
		m_densities[i]         = m_particles[i].density;
	}
}

// ── Spawn ───────────────────────────────────────────────────────────

void Particles::spawnOne(Particle& p, Mode mode, bool initial)
{
	static std::mt19937 rng(0xC0FFEE);
	std::uniform_real_distribution<float> ux(m_cfg.boundsMin.x + 0.05f, m_cfg.boundsMax.x - 0.05f);
	std::uniform_real_distribution<float> uz(m_cfg.boundsMin.z + 0.05f, m_cfg.boundsMax.z - 0.05f);
	std::uniform_real_distribution<float> u01(0.f, 1.f);

	const float x = ux(rng);
	const float z = uz(rng);
	const float ground = m_heightFn ? m_heightFn(x, z) : m_cfg.boundsMin.y;

	switch (mode) {
		case Mode::UniformRise:
			p.position = {x, ground + 0.02f + (initial ? u01(rng) * 0.5f : 0.f), z};
			p.velocity = glm::vec3{0.f};
			break;
		case Mode::Wave:
			p.position = {m_cfg.boundsMin.x + 0.1f, ground + 0.1f + u01(rng) * 0.3f, z};
			p.velocity = {3.f + u01(rng) * 0.5f, 0.f, 0.f};
			break;
		case Mode::Rain:
			p.position = {x, m_cfg.boundsMax.y - u01(rng) * 0.5f, z};
			p.velocity = {0.f, -1.f, 0.f};
			break;
	}
	p.age = initial ? u01(rng) * 2.f : 0.f;
	p.density  = m_cfg.restDensity;
	p.pressure = 0.f;
}

void Particles::spawnAll(Mode mode)
{
	m_particles.resize(m_positions.size() / 3);
	for (auto& p : m_particles) spawnOne(p, mode, /*initial=*/true);
	m_densities.assign(m_particles.size(), m_cfg.restDensity);
}

void Particles::init(size_t count, Mode mode)
{
	const Config& cfg = {};
	
	m_cfg = cfg;
	m_particles.clear();
	m_particles.reserve(count);
	m_positions.assign(count * 3, 0.f);
	m_densities.assign(count, cfg.restDensity);

	spawnAll(mode);
	m_mesh.uploadPoints(m_positions, m_densities);
}

void Particles::reset(Mode mode)
{
	spawnAll(mode);
	m_mesh.updatePoints(m_positions, m_densities);
}

// ── GPU ─────────────────────────────────────────────────────────────

void Particles::uploadToGPU()
{
	m_mesh.updatePoints(m_positions, m_densities);
}

void Particles::render(Shader& shader, const glm::mat4& viewProj)
{
	shader.use();
	shader.setMat4("uViewProj",   viewProj);
	shader.setFloat("uPointSize", m_cfg.pointSize);
	shader.setVec3 ("uColorLow",  m_cfg.colorLow);
	shader.setVec3 ("uColorHigh", m_cfg.colorHigh);
	shader.setFloat("uAlpha",     m_cfg.alpha);
	shader.setFloat("uDensityMin", m_cfg.restDensity * 0.9f);
	shader.setFloat("uDensityMax", m_cfg.restDensity * 1.3f);
	m_mesh.drawPoints();
}
