#include <Particles.hpp>
#include <Shader.hpp>

#include <gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <random>

namespace {

constexpr float PI = 3.14159265358979323846f;

// ── Kernels SPH ─────────────────────────────────────────────────────

// Poly6: densidad. W(r) = 315/(64π h⁹) (h² - r²)³
float kernelPoly6(float r2, float h2, float coeff)
{
    if (r2 >= h2) return 0.f;
    float diff = h2 - r2;
    return coeff * diff * diff * diff;
}

// Spiky gradient magnitude. F(r) = -45/(π h⁶) (h - r)²
float kernelSpikyGrad(float r, float h, float coeff)
{
    if (r >= h || r < 1e-6f) return 0.f;
    float diff = h - r;
    return coeff * diff * diff;
}

// Laplacian (viscosidad). ∇²W(r) = 45/(π h⁶) (h - r)
float kernelLaplacian(float r, float h, float coeff)
{
    if (r >= h) return 0.f;
    return coeff * (h - r);
}

} // namespace

Particles::Particles() = default;
Particles::~Particles() = default;

void Particles::updateKernelCoefficients() {
    const float h = m_cfg.h;
    m_poly6Coeff = 315.f / (64.f * PI * std::pow(h, 9.f));
    m_spikyCoeff = -45.f / (PI * std::pow(h, 6.f));
    m_viscCoeff  =  45.f / (PI * std::pow(h, 6.f));
}

// ── Spatial hashing ─────────────────────────────────────────────────

glm::ivec3 Particles::cellOf(const glm::vec3& p) const
{
    return glm::ivec3(glm::floor(p / m_cfg.h));
}

int64_t Particles::hashCell(const glm::ivec3& c) const
{
    constexpr int64_t p1 = 73856093;
    constexpr int64_t p2 = 19349663;
    constexpr int64_t p3 = 83492791;
    return (int64_t(c.x) * p1) ^ (int64_t(c.y) * p2) ^ (int64_t(c.z) * p3);
}

void Particles::buildHash()
{
	for (auto& kv : m_hash)
		kv.second.clear();
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

    // ¡OJO! Aquí ya NO llamamos a buildHash()

    // 1. Densidad y presión
    for (size_t i = 0; i < n; ++i) {
        Particle& pi = m_particles[i];
        float density = 0.f;
        forEachNeighbor(i, [&](size_t j) {
            const glm::vec3 d = pi.position - m_particles[j].position;
            density += m_cfg.particleMass * kernelPoly6(glm::dot(d, d), h2, m_poly6Coeff);
        });
        pi.density = std::max(density, m_cfg.restDensity * 0.1f);
        
        // ── FIX: CALCULAR LA PRESIÓN ──
        pi.pressure = m_cfg.stiffness * (pi.density - m_cfg.restDensity);
        
        // CLAVE: Evitar inestabilidad de tracción (Tensile Instability).
        // Permitimos una presión negativa MUY leve para cohesión, pero no más.
        float minPressure = -0.001f * m_cfg.stiffness; 
        pi.pressure = std::max(pi.pressure, minPressure);
		// pi.pressure = std::max(pi.pressure, 0.f);
        
        pi.force = glm::vec3(0.f, m_cfg.gravity, 0.f);
    }

    // 2. Fuerzas (presión + viscosidad) -> igual que lo tenías
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

            float pTerm = (pi.pressure + pj.pressure) / (pi.density * pj.density);
            fPress -= dir * (pTerm * kernelSpikyGrad(r, h, m_spikyCoeff));

			fVisc += (pj.velocity - pi.velocity) *
					(m_cfg.viscosity * kernelLaplacian(r, h, m_viscCoeff) / pj.density);
        });
        pi.force += fPress + fVisc;
    }
    // (void)dt;

	// 3. Viscosidad XSPH (Suavizado de velocidad para eliminar el "jitter")
    // Esto promedia la velocidad de cada partícula con sus vecinas.
    // const float xsph_epsilon = 0.001f; 
    // for (size_t i = 0; i < n; ++i) {
    //     Particle& pi = m_particles[i];
    //     glm::vec3 vDelta{0.f};
    //     forEachNeighbor(i, [&](size_t j) {
    //         const Particle& pj = m_particles[j];
    //         glm::vec3 d = pi.position - pj.position;
    //         float r2 = glm::dot(d, d);
    //         if (r2 < h2) {
    //             // Usamos un kernel de densidad normalizado
    //             vDelta += (pj.velocity - pi.velocity) * kernelPoly6(r2, h2, m_poly6Coeff);
    //         }
    //     });
    //     // Aplicamos un porcentaje del cambio de velocidad
    //     pi.velocity += vDelta * xsph_epsilon;
    // }
    (void)dt;
}

void Particles::integrate(float dt)
{
    for (auto& p : m_particles) {
        p.velocity += p.force * dt;      // Euler semi-implícito
        // p.velocity *= m_cfg.damping;

        // ── LÍMITE DE VELOCIDAD (Evita que exploten y desaparezcan) ──
        float speed = glm::length(p.velocity);
        if (speed > 10.0f) {
            p.velocity = (p.velocity / speed) * 10.0f;
        }
        // ─────────────────────────────────────────────────────────────

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
        // Suelo (terreno o plano en lo.y)
        const float ground = m_heightFn ? m_heightFn(p.position.x, p.position.z)
                                        : lo.y;
        if (p.position.y < ground) {
            p.position.y = ground;
            if (p.velocity.y < 0.f)
                p.velocity.y = -p.velocity.y * e;
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
    if (m_particles.empty())
		return;

	if (dt > 0.033f)
		dt = 0.033f; 

    const float subDt = dt / static_cast<float>(m_cfg.substeps);
    for (int s = 0; s < m_cfg.substeps; ++s) {
		buildHash();
        stepSPH(subDt);
        integrate(subDt);
        collide();
    }
	for (auto& p : m_particles)
		p.velocity *= m_cfg.damping;
    // Buffers para GPU
    for (size_t i = 0; i < m_particles.size(); ++i) {
        m_positions[i * 3 + 0] = m_particles[i].position.x;
        m_positions[i * 3 + 1] = m_particles[i].position.y;
        m_positions[i * 3 + 2] = m_particles[i].position.z;
        m_densities[i]         = m_particles[i].density;
    }

    // Debug: cada 30 frames
    if (++m_debugCounter % 30 == 0) {
        float avg = 0.f;
        for (const auto& p : m_particles) avg += p.density;
        avg /= static_cast<float>(m_particles.size());
        std::printf("[sph] avg density = %.1f  (rest = %.1f)\n",
                    avg, m_cfg.restDensity);
    }
}

// ── Spawn ───────────────────────────────────────────────────────────

void Particles::spawnOne(Particle& p, Mode mode, bool initial)
{
    static std::mt19937 rng(0xC0FFEE);
    std::uniform_real_distribution<float> ux(m_cfg.boundsMin.x + 0.05f,
                                             m_cfg.boundsMax.x - 0.05f);
    std::uniform_real_distribution<float> uz(m_cfg.boundsMin.z + 0.05f,
                                             m_cfg.boundsMax.z - 0.05f);
    std::uniform_real_distribution<float> u01(0.f, 1.f);

    const float x      = ux(rng);
    const float z      = uz(rng);
    const float ground = m_heightFn ? m_heightFn(x, z) : m_cfg.boundsMin.y;

    switch (mode) {
        case Mode::UniformRise:
            p.position = {x, ground + 0.02f + (initial ? u01(rng) * 0.5f : 0.f), z};
            p.velocity = {0.f, 0.f, 0.f};
            break;
        case Mode::Wave:
            p.position = {m_cfg.boundsMin.x + 0.1f,
                          ground + 0.1f + u01(rng) * 0.3f,
                          z};
            p.velocity = {3.f + u01(rng) * 0.5f, 0.f, 0.f};
            break;
        case Mode::Rain:
            p.position = {x, m_cfg.boundsMax.y - u01(rng) * 0.5f, z};
            p.velocity = {0.f, -1.f, 0.f};
            break;
    }
    p.age      = initial ? u01(rng) * 2.f : 0.f;
    p.density  = m_cfg.restDensity;
    p.pressure = 0.f;
}

void Particles::spawnAll(Mode mode)
{
	m_particles.resize(m_positions.size() / 3);
	for (auto& p : m_particles)
		spawnOne(p, mode, /*initial=*/true);

	m_densities.assign(m_particles.size(), m_cfg.restDensity);

	for (size_t i = 0; i < m_particles.size(); ++i) {
		m_positions[i * 3 + 0] = m_particles[i].position.x;
		m_positions[i * 3 + 1] = m_particles[i].position.y;
		m_positions[i * 3 + 2] = m_particles[i].position.z;
	}
}

// ── Init / Reset ────────────────────────────────────────────────────

void Particles::init(size_t count, Mode mode)
{
    init(count, mode, m_cfg);   // usa el config actual (setConfig o defaults)
}

void Particles::init(size_t count, Mode mode, const Config& cfg)
{
    m_cfg = cfg;
    m_debugCounter = 0;

    m_particles.clear();
    m_particles.reserve(count);
    m_positions.assign(count * 3, 0.f);
    m_densities.assign(count, m_cfg.restDensity);
	updateKernelCoefficients();

    spawnAll(mode);
    m_mesh.uploadPoints(m_positions, m_densities);
}

void Particles::reset(Mode mode)
{
    spawnAll(mode);
	updateKernelCoefficients();
    m_mesh.updatePoints(m_positions, m_densities);
	m_debugCounter = 0;
}

// ── GPU ─────────────────────────────────────────────────────────────

void Particles::uploadToGPU()
{
    m_mesh.updatePoints(m_positions, m_densities);
}

void Particles::render(Shader& shader, const glm::mat4& viewProj)
{
    shader.use();
    shader.setMat4 ("uViewProj",   viewProj);
    shader.setFloat("uPointSize",  m_cfg.pointSize);
    shader.setVec3 ("uColorLow",   m_cfg.colorLow);
    shader.setVec3 ("uColorHigh",  m_cfg.colorHigh);
    shader.setFloat("uAlpha",      m_cfg.alpha);
    shader.setFloat("uDensityMin", m_cfg.restDensity * 0.9f);
    shader.setFloat("uDensityMax", m_cfg.restDensity * 1.3f);
    m_mesh.drawPoints();
}
