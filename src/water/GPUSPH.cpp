/**
 * @file GPUSPH.cpp
 *
 * Dispatches the compute passes that replace the CPU solver: Clear & Predict,
 * the GPU spatial hash (counting sort over a uniform grid, which replaces the
 * CPU std::unordered_map of Particles::buildHash), the Density & Pressure
 * pass (Poly6 kernel + equation of state over the sorted neighbor ranges),
 * the Forces & XSPH pass (Spiky pressure + Laplacian viscosity + XSPH
 * velocity smoothing over the same ranges) and the Integration pass
 * (semi-implicit Euler, boundary collisions, rain recycling and
 * classification, updating the particle state in place).
 */

#include <GPUSPH.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

namespace
{

/**
 * @struct GPUParticle
 * @brief GPU particle structure stored in the particle SSBO.
 *
 * Must match the GPUParticle struct layout in sph_clear_predict.comp,
 * sph_integrate.comp and density_splat.comp (std430, 48 bytes).
 */
struct GPUParticle
{
	glm::vec4 posWeight;	///< xyz = position, w = splatWeight.
	glm::vec4 velAge;		///< xyz = velocity, w = age.
	glm::vec4 flags;		///< x = rainDrop flag (1 = rain drop), y,z,w = unused.
};

/** @brief Threads per workgroup used by every phase of sph_hash.comp. */
constexpr int HASH_WORKGROUP_SIZE = 1024;

/** @brief Threads per workgroup used by sph_clear_predict.comp. */
constexpr int PREDICT_WORKGROUP_SIZE = 64;

/** @brief Threads per workgroup used by sph_density.comp. */
constexpr int DENSITY_WORKGROUP_SIZE = 64;

/** @brief Threads per workgroup used by sph_forces.comp. */
constexpr int FORCES_WORKGROUP_SIZE = 64;

/** @brief Threads per workgroup used by sph_integrate.comp. */
constexpr int INTEGRATE_WORKGROUP_SIZE = 64;

/**
 * @brief Computes the number of workgroups needed to cover n items.
 * @param n Number of items.
 * @param local Threads per workgroup.
 * @return ceil(n / local).
 */
GLuint groupsFor(size_t n, int local)
{
	return static_cast<GLuint>((n + static_cast<size_t>(local) - 1) / static_cast<size_t>(local));
}

}  // namespace

/** @brief Default constructor. */
GPUSPH::GPUSPH() = default;

/** @brief Destructor. Ensures all GL resources are released. */
GPUSPH::~GPUSPH()
{
	release();
}

/**
 * @brief Initializes the GPU SPH pipeline.
 *
 * Loads the compute shaders of the current pipeline, validates the uniform
 * grid against the hierarchical scan limits, and allocates all SSBOs.
 *
 * @param maxParticles Capacity of the per-particle SSBOs.
 * @param params Physics and grid parameters.
 * @param clearPredictPath Path to the Clear & Predict shader.
 * @param hashPath Path to the spatial hashing shader.
 * @param densityPath Path to the density & pressure shader.
 * @param forcesPath Path to the forces & XSPH shader.
 * @param integratePath Path to the integration shader.
 * @return True if the shaders compiled and the buffers were created successfully.
 */
bool GPUSPH::init(size_t maxParticles, const Params& params, const std::string& clearPredictPath,
				  const std::string& hashPath, const std::string& densityPath,
				  const std::string& forcesPath, const std::string& integratePath)
{
	if (m_initialized)
		release();

	m_params = params;

	// Load the Clear & Predict compute shader (sph_clear_predict.comp)
	if (!m_clearPredictShader.loadComputeFromFile(clearPredictPath)) {
		std::fprintf(stderr, "GPUSPH: failed to load '%s'\n", clearPredictPath.c_str());
		return false;
	}

	// Load the spatial hashing compute shader (sph_hash.comp)
	if (!m_hashShader.loadComputeFromFile(hashPath)) {
		std::fprintf(stderr, "GPUSPH: failed to load '%s'\n", hashPath.c_str());
		return false;
	}

	// Load the density & pressure compute shader (sph_density.comp)
	if (!m_densityShader.loadComputeFromFile(densityPath)) {
		std::fprintf(stderr, "GPUSPH: failed to load '%s'\n", densityPath.c_str());
		return false;
	}

	// Load the forces & XSPH compute shader (sph_forces.comp)
	if (!m_forcesShader.loadComputeFromFile(forcesPath)) {
		std::fprintf(stderr, "GPUSPH: failed to load '%s'\n", forcesPath.c_str());
		return false;
	}

	// Load the integration compute shader (sph_integrate.comp)
	if (!m_integrateShader.loadComputeFromFile(integratePath)) {
		std::fprintf(stderr, "GPUSPH: failed to load '%s'\n", integratePath.c_str());
		return false;
	}

	m_maxParticles	= maxParticles;
	m_particleCount = 0;
	createBuffers();
	m_initialized = true;

	std::fprintf(stderr,
				 "[GPUSPH] init ok: maxParticles=%zu  grid=%dx%dx%d (%d cells, %d scan blocks, "
				 "cellSize=%.3f)\n",
				 m_maxParticles, m_gridSize.x, m_gridSize.y, m_gridSize.z, m_numCells, m_numBlocks,
				 m_cellSize);
	return true;
}

/**
 * @brief (Re)builds the uniform grid dimensions from the bounds and cell size.
 *
 * The cell size starts at the smoothing radius h (so a 3x3x3 cell sweep always
 * covers the kernel support). It is then doubled while the total cell count
 * exceeds what the single-level hierarchical Blelloch scan supports
 * (WORKGROUP_SIZE * WORKGROUP_SIZE cells): the block sums must fit in ONE
 * 1024-thread workgroup of Phase 3. Larger cells remain correct for neighbor
 * search, they only widen the candidate set.
 */
void GPUSPH::rebuildGrid()
{
	m_cellSize = m_params.h;
	const glm::vec3 extent =
		glm::max(m_params.boundsMax - m_params.boundsMin, glm::vec3(m_cellSize));
	glm::ivec3 g = glm::clamp(glm::ivec3(glm::ceil(extent / m_cellSize)), glm::ivec3(1),
							  glm::ivec3(HASH_WORKGROUP_SIZE));

	constexpr int kMaxCells = HASH_WORKGROUP_SIZE * HASH_WORKGROUP_SIZE;
	while (g.x * g.y * g.z > kMaxCells) {
		m_cellSize *= 2.f;
		g = glm::clamp(glm::ivec3(glm::ceil(extent / m_cellSize)), glm::ivec3(1),
					   glm::ivec3(HASH_WORKGROUP_SIZE));
	}

	m_gridSize	= g;
	m_numCells	= g.x * g.y * g.z;
	m_numBlocks = groupsFor(static_cast<size_t>(m_numCells), HASH_WORKGROUP_SIZE);
}

/**
 * @brief Creates and allocates all SSBOs for the current capacity and grid.
 *
 * The per-particle buffers are sized by m_maxParticles; the grid buffers by
 * m_numCells / m_numBlocks (recomputed in rebuildGrid()).
 */
void GPUSPH::createBuffers()
{
	const GLsizeiptr particleBytes = static_cast<GLsizeiptr>(m_maxParticles) * sizeof(GPUParticle);
	const GLsizeiptr vec4Bytes	   = static_cast<GLsizeiptr>(m_maxParticles) * sizeof(glm::vec4);
	const GLsizeiptr uintBytes	   = static_cast<GLsizeiptr>(m_maxParticles) * sizeof(GLuint);

	rebuildGrid();
	const GLsizeiptr cellBytes	= static_cast<GLsizeiptr>(m_numCells) * sizeof(GLuint);
	const GLsizeiptr blockBytes = static_cast<GLsizeiptr>(m_numBlocks) * sizeof(GLuint);

	// Helper: allocate one zero-filled SSBO.
	auto createSSBO = [](GLuint& id, GLsizeiptr bytes) {
		glGenBuffers(1, &id);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
		glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, nullptr, GL_DYNAMIC_DRAW);
	};

	// ── Per-particle state (SoA layout) ─────────────────────────────
	createSSBO(m_particleSSBO, particleBytes);	// binding 0: GPUParticle[]
	createSSBO(m_densPressSSBO, vec4Bytes);		// binding 1: vec4[] density/pressure/splatWeight
	createSSBO(m_forceSSBO, vec4Bytes);			// binding 2
	createSSBO(m_posPredSSBO, vec4Bytes);		// binding 3
	createSSBO(m_cellIdSSBO, uintBytes);		// binding 4
	createSSBO(m_sortedIdxSSBO, uintBytes);		// binding 5

	// ── Spatial hash grid (counting sort) ────────────────────────────
	createSSBO(m_cellCountSSBO, cellBytes);	  // binding 6
	createSSBO(m_cellStartSSBO, cellBytes);	  // binding 7
	createSSBO(m_cellCursorSSBO, cellBytes);  // binding 8
	createSSBO(m_blockSumsSSBO, blockBytes);  // binding 9

	// ── XSPH velocity delta (applied by the integration pass) ────────
	createSSBO(m_xsphVelSSBO, vec4Bytes);  // binding 10: vec4[] XSPH delta

	// ── Kinetic-energy accumulator (one fixed-point scalar) ──────────
	createSSBO(m_energySSBO, static_cast<GLsizeiptr>(sizeof(GLuint)));	// binding 11

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/**
 * @brief Sets new physics/grid parameters, rebuilding the grid if needed.
 * @param params The parameters to adopt.
 */
void GPUSPH::setParams(const Params& params)
{
	const bool gridChanged = params.h != m_params.h || params.boundsMin != m_params.boundsMin ||
							 params.boundsMax != m_params.boundsMax;
	m_params = params;
	if (m_initialized && gridChanged)
		createBuffers();  // reallocates the cell-sized buffers for the new grid
}

/**
 * @brief Uploads the CPU particle data to the GPU particle SSBO.
 *
 * Packs position + splatWeight and velocity + age into the GPUParticle layout.
 * Inputs larger than the capacity are rejected (init() sizes the pipeline).
 *
 * @param particles The CPU particle data to seed the GPU state with.
 */
void GPUSPH::uploadParticles(const std::vector<Particle>& particles)
{
	if (!m_initialized)
		return;

	if (particles.size() > m_maxParticles) {
		std::fprintf(stderr, "GPUSPH: uploadParticles(%zu) exceeds capacity %zu\n",
					 particles.size(), m_maxParticles);
		return;
	}

	m_particleCount = particles.size();
	if (particles.empty())
		return;

	std::vector<GPUParticle> staging(particles.size());
	for (size_t i = 0; i < particles.size(); ++i) {
		staging[i].posWeight = glm::vec4(particles[i].position, particles[i].splatWeight);
		staging[i].velAge	 = glm::vec4(particles[i].velocity, particles[i].age);
		staging[i].flags	 = glm::vec4(particles[i].rainDrop ? 1.f : 0.f, 0.f, 0.f, 0.f);
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
					static_cast<GLsizeiptr>(staging.size() * sizeof(GPUParticle)), staging.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/**
 * @brief Dispatches Clear & Predict and the six counting-sort phases.
 *
 * All SSBOs are bound to their fixed binding points and one memory barrier
 * is inserted between every dispatch, since each phase reads what the
 * previous one wrote.
 *
 * @param subDt Substep delta time passed to the prediction.
 */
void GPUSPH::buildSpatialHash(float subDt)
{
	// ── 1. Clear & Predict: forces = gravity, predicted positions ─────
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_PARTICLES, m_particleSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_FORCES, m_forceSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_POS_PRED, m_posPredSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_ENERGY, m_energySSBO);

	m_clearPredictShader.use();
	m_clearPredictShader.setInt("uParticleCount", static_cast<int>(m_particleCount));
	m_clearPredictShader.setFloat("uDt", subDt);
	m_clearPredictShader.setFloat("uGravity", m_params.gravity);
	m_clearPredictShader.dispatch(groupsFor(m_particleCount, PREDICT_WORKGROUP_SIZE), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// ── 2..7. Counting sort (phases 0..5 of sph_hash.comp) ────────────
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_CELL_IDS, m_cellIdSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_SORTED_IDX, m_sortedIdxSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_CELL_COUNT, m_cellCountSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_CELL_START, m_cellStartSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_CELL_CURSOR, m_cellCursorSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_BLOCK_SUMS, m_blockSumsSSBO);

	m_hashShader.use();
	m_hashShader.setInt("uParticleCount", static_cast<int>(m_particleCount));
	m_hashShader.setInt("uNumCells", m_numCells);
	m_hashShader.setInt("uNumBlocks", m_numBlocks);
	m_hashShader.setIVec3("uGridSize", m_gridSize);
	m_hashShader.setVec3("uGridMin", m_params.boundsMin);
	m_hashShader.setFloat("uCellSize", m_cellSize);

	const GLuint cellGroups = static_cast<GLuint>(m_numBlocks);
	const GLuint partGroups = groupsFor(m_particleCount, HASH_WORKGROUP_SIZE);

	// Phase 0: clear the per-cell counters.
	m_hashShader.setInt("uPhase", 0);
	m_hashShader.dispatch(cellGroups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// Phase 1: count particles per cell (atomics on the cleared counters).
	m_hashShader.setInt("uPhase", 1);
	m_hashShader.dispatch(partGroups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// Phase 2: local Blelloch exclusive scan per 1024-cell block.
	m_hashShader.setInt("uPhase", 2);
	m_hashShader.dispatch(cellGroups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// Phase 3: exclusive scan of the block sums (single workgroup).
	m_hashShader.setInt("uPhase", 3);
	m_hashShader.dispatch(1, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// Phase 4: fold the block offsets back into cellStart, arm cursors.
	m_hashShader.setInt("uPhase", 4);
	m_hashShader.dispatch(cellGroups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// Phase 5: scatter particles into their cell range.
	m_hashShader.setInt("uPhase", 5);
	m_hashShader.dispatch(partGroups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

/**
 * @brief Dispatches the Density & Pressure pass (sph_density.comp).
 *
 * One invocation per particle: 27-cell sweep over the sorted ranges of the
 * spatial hash, Poly6 density accumulation over the predicted positions and
 * the equation of state with the CPU-solver clamps. The result lands in
 * DensPressBuffer, ready for the forces pass.
 */
void GPUSPH::computeDensityPressure()
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_PARTICLES, m_particleSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_DENS_PRESS, m_densPressSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_POS_PRED, m_posPredSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_SORTED_IDX, m_sortedIdxSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_CELL_COUNT, m_cellCountSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_CELL_START, m_cellStartSSBO);

	m_densityShader.use();
	m_densityShader.setInt("uParticleCount", static_cast<int>(m_particleCount));
	m_densityShader.setFloat("uH", m_params.h);
	m_densityShader.setFloat("uMass", m_params.particleMass);
	m_densityShader.setFloat("uRestDensity", m_params.restDensity);
	m_densityShader.setFloat("uStiffness", m_params.stiffness);
	m_densityShader.setInt("uRainMode", m_params.rainMode ? 1 : 0);
	m_densityShader.setFloat("uSettleSpeed", m_params.rainSettleSpeed);
	m_densityShader.setFloat("uAirY", m_params.boundsMax.y - m_params.rainAirBand);
	m_densityShader.setIVec3("uGridSize", m_gridSize);
	m_densityShader.setVec3("uGridMin", m_params.boundsMin);
	m_densityShader.setFloat("uCellSize", m_cellSize);
	m_densityShader.dispatch(groupsFor(m_particleCount, DENSITY_WORKGROUP_SIZE), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

/**
 * @brief Dispatches the Forces & XSPH pass (sph_forces.comp).
 *
 * One invocation per particle: fused 27-cell sweep accumulating the Spiky
 * pressure force, the Laplacian viscosity force (added on top of the gravity
 * already present in ForceBuffer) and the XSPH velocity delta, stored in
 * XsphVelBuffer because applying it in place would race against the neighbor
 * velocity reads of this same pass.
 */
void GPUSPH::computeForces()
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_PARTICLES, m_particleSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_DENS_PRESS, m_densPressSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_FORCES, m_forceSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_POS_PRED, m_posPredSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_SORTED_IDX, m_sortedIdxSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_CELL_COUNT, m_cellCountSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_CELL_START, m_cellStartSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_XSPH_VEL, m_xsphVelSSBO);

	m_forcesShader.use();
	m_forcesShader.setInt("uParticleCount", static_cast<int>(m_particleCount));
	m_forcesShader.setFloat("uH", m_params.h);
	m_forcesShader.setFloat("uViscosity", m_params.viscosity);
	m_forcesShader.setFloat("uXsph", m_params.xsph);
	m_forcesShader.setIVec3("uGridSize", m_gridSize);
	m_forcesShader.setVec3("uGridMin", m_params.boundsMin);
	m_forcesShader.setFloat("uCellSize", m_cellSize);
	m_forcesShader.dispatch(groupsFor(m_particleCount, FORCES_WORKGROUP_SIZE), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

/**
 * @brief Dispatches the Integration pass (sph_integrate.comp).
 *
 * One invocation per particle: applies the force and the XSPH delta to the
 * velocity (with damping and the speed clamp), moves the particle, resolves
 * the ground/wall/ceiling collisions, recycles rain drops that touch the
 * ground and classifies splatWeight on the post-move state.
 *
 * @param subDt Substep delta time applied to the integration.
 */
void GPUSPH::integrateParticles(float subDt)
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_PARTICLES, m_particleSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_FORCES, m_forceSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_XSPH_VEL, m_xsphVelSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_ENERGY, m_energySSBO);

	m_integrateShader.use();
	m_integrateShader.setInt("uParticleCount", static_cast<int>(m_particleCount));
	m_integrateShader.setFloat("uDt", subDt);
	m_integrateShader.setFloat("uDamping", m_params.damping);
	m_integrateShader.setFloat("uMaxSpeed", m_params.maxSpeed);
	m_integrateShader.setVec3("uBoundsMin", m_params.boundsMin);
	m_integrateShader.setVec3("uBoundsMax", m_params.boundsMax);
	m_integrateShader.setFloat("uWallRestitution", m_params.wallRestitution);
	m_integrateShader.setInt("uRainMode", m_params.rainMode ? 1 : 0);
	m_integrateShader.setFloat("uSettleSpeed", m_params.rainSettleSpeed);
	m_integrateShader.setFloat("uAirY", m_params.boundsMax.y - m_params.rainAirBand);
	m_integrateShader.setInt("uSeed", static_cast<int>(m_recycleSeed));
	m_integrateShader.dispatch(groupsFor(m_particleCount, INTEGRATE_WORKGROUP_SIZE), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

/**
 * @brief Runs one frame of the SPH pipeline on the GPU.
 *
 * Each substep executes the full chain: Clear & Predict, Spatial Hash,
 * Density & Pressure, Forces & XSPH and Integration (collisions, rain
 * recycling and classification).
 *
 * @param dt Delta time in seconds.
 */
void GPUSPH::update(float dt)
{
	if (!m_initialized || m_particleCount == 0)
		return;

	if (dt > 0.033f)
		dt = 0.033f;

	const float subDt = dt / static_cast<float>(m_params.substeps);
	for (int s = 0; s < m_params.substeps; ++s) {
		buildSpatialHash(subDt);
		computeDensityPressure();
		computeForces();
		integrateParticles(subDt);

		// Advance the respawn RNG seed: every substep draws a fresh stream.
		++m_recycleSeed;
	}

	// Read back the fixed-point energy accumulator of the last substep and
	// convert it to the mean squared particle speed. The exponential smoothing
	// removes the substep-to-substep jitter before the shader consumes it.
	constexpr float kEnergyScale = 2000.0f;
	GLuint			rawEnergy	 = 0;
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_energySSBO);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &rawEnergy);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	const float meanSquaredSpeed =
		static_cast<float>(rawEnergy) /
		(kEnergyScale * static_cast<float>(m_particleCount));
	const float smoothing = 1.0f - std::exp(-dt * 8.0f);
	m_waterEnergy += (meanSquaredSpeed - m_waterEnergy) * smoothing;
}

/**
 * @brief Renders the airborne rain drops as GL_POINTS.
 *
 * Maps the particle SSBO onto the point vertex attributes (48-byte
 * interleaved GPUParticle records) and draws every particle; the point
 * shader performs the filtering in the vertex stage: only rain-drop
 * flagged particles survive, minus the settled ones. The barrier
 * guarantees the vertex fetch sees the state written by the last compute
 * substep. The density and the splat weight attributes come from the
 * density/pressure SSBO (vec4 records) so the size modulation and the
 * settled-water classification need no CPU readback.
 *
 * @param shader Point shader (waterVertex.glsl + waterFrag.frag).
 * @param viewProj The combined view-projection matrix.
 */
void GPUSPH::renderRainDrops(Shader& shader, const glm::mat4& viewProj)
{
	if (!m_initialized || m_particleCount == 0)
		return;

	// SSBO writes from the last substep must be visible to vertex fetches.
	glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

	constexpr GLsizei kStride = sizeof(float) * 12;	 // GPUParticle: 3 x vec4 = 48 bytes.

	// A buffer object can feed vertex attributes while remaining bound as an
	// SSBO: the pointers capture the current GL_ARRAY_BUFFER binding and the
	// VAO stores them. Configured on every call so a reallocation of the SSBO
	// can never leave the VAO referencing a dead handle.
	if (m_rainVAO == 0)
		glGenVertexArrays(1, &m_rainVAO);
	glBindVertexArray(m_rainVAO);
	// Particle SSBO (stride 48): position at offset 0, velocity at offset 16
	// and the rain-drop flag (flags.x) at offset 32.
	glBindBuffer(GL_ARRAY_BUFFER, m_particleSSBO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(0));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(16));
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(32));
	// Density/pressure SSBO (stride 16): density is its x component and the
	// settled-water splat weight is its z component.
	glBindBuffer(GL_ARRAY_BUFFER, m_densPressSSBO);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
						   reinterpret_cast<const void*>(0));
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
						   reinterpret_cast<const void*>(8));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(4);

	shader.use();
	shader.setMat4("uViewProj", viewProj);
	shader.setFloat("uPointSize", m_params.rainPointSize);
	shader.setVec3("uColorLow", m_params.rainColor);
	shader.setVec3("uColorHigh", m_params.rainColor);
	shader.setFloat("uAlpha", m_params.rainAlpha);
	shader.setFloat("uDensityMin", m_params.restDensity * 0.9f);
	shader.setFloat("uDensityMax", m_params.restDensity * 1.3f);
	// Settled-drop hiding: drops classified as puddle water by the density
	// pass (splatWeight > 0.5) are culled by the vertex shader. The shader
	// re-checks the speed with this uniform, so it must use the same
	// threshold as the density classification (rainSettleSpeed); otherwise
	// drops between rainSplashSpeed and rainSettleSpeed would be marked as
	// puddle water by the GPU pass but still drawn as points.
	shader.setFloat("uSplashSpeed", m_params.rainSettleSpeed);
	glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_particleCount));

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
 * @brief Validates the spatial hash with a synchronous CPU readback.
 *
 * Executes the five structural checks documented in the header. Any failure
 * prints a diagnostic to stderr identifying the broken invariant.
 *
 * @return True if all checks pass.
 */
bool GPUSPH::debugValidateSort() const
{
	if (!m_initialized || m_particleCount == 0)
		return false;

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	const size_t n	   = m_particleCount;
	const size_t cells = static_cast<size_t>(m_numCells);

	std::vector<GLuint>	 counts(cells), starts(cells), sorted(n), ids(n);
	std::vector<GLfloat> pred4(n * 4);

	auto readback = [](GLuint ssbo, void* dst, GLsizeiptr bytes) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, dst);
	};
	readback(m_cellCountSSBO, counts.data(), static_cast<GLsizeiptr>(cells * sizeof(GLuint)));
	readback(m_cellStartSSBO, starts.data(), static_cast<GLsizeiptr>(cells * sizeof(GLuint)));
	readback(m_sortedIdxSSBO, sorted.data(), static_cast<GLsizeiptr>(n * sizeof(GLuint)));
	readback(m_cellIdSSBO, ids.data(), static_cast<GLsizeiptr>(n * sizeof(GLuint)));
	readback(m_posPredSSBO, pred4.data(), static_cast<GLsizeiptr>(n * 4 * sizeof(GLfloat)));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	bool ok = true;

	// 1) + 2) cellCount sums to N and cellStart is its exclusive prefix.
	GLuint sum = 0u;
	for (size_t c = 0; c < cells; ++c) {
		if (sum != starts[c]) {
			std::fprintf(stderr, "[GPUSPH] validate: cellStart[%zu]=%u but running sum=%u\n", c,
						 starts[c], sum);
			ok = false;
			break;
		}
		sum += counts[c];
	}
	if (ok && sum != n) {
		std::fprintf(stderr, "[GPUSPH] validate: sum(cellCount)=%u != N=%zu\n", sum, n);
		ok = false;
	}

	// 3) sortedIdx is a permutation of [0, N).
	if (ok) {
		std::vector<GLuint> reference(sorted.size());
		std::iota(reference.begin(), reference.end(), 0u);
		std::sort(sorted.begin(), sorted.end());
		if (sorted != reference) {
			std::fprintf(stderr, "[GPUSPH] validate: sortedIdx is not a permutation of [0, N)\n");
			ok = false;
		}
	}

	// 4) Every scattered entry lands inside its cell range.
	if (ok) {
		readback(m_sortedIdxSSBO, sorted.data(), static_cast<GLsizeiptr>(n * sizeof(GLuint)));
		for (size_t c = 0; c < cells && ok; ++c)
			for (GLuint k = starts[c]; k < starts[c] + counts[c]; ++k)
				if (ids[sorted[k]] != c) {
					std::fprintf(stderr,
								 "[GPUSPH] validate: sortedIdx[%u]=%u is in cell %zu but claims "
								 "cell %u\n",
								 k, sorted[k], c, ids[sorted[k]]);
					ok = false;
					break;
				}
	}

	// 5) cellIds matches the cell recomputed from the predicted positions.
	if (ok) {
		for (size_t i = 0; i < n; ++i) {
			const glm::vec3	 p(pred4[i * 4 + 0], pred4[i * 4 + 1], pred4[i * 4 + 2]);
			const glm::ivec3 c =
				glm::clamp(glm::ivec3(glm::floor((p - m_params.boundsMin) / m_cellSize)),
						   glm::ivec3(0), m_gridSize - glm::ivec3(1));
			const GLuint expected =
				static_cast<GLuint>(c.x + m_gridSize.x * (c.y + m_gridSize.y * c.z));
			if (ids[i] != expected) {
				std::fprintf(stderr,
							 "[GPUSPH] validate: particle %zu hashed to cell %u, expected %u "
							 "(pred=(%.3f, %.3f, %.3f), cellSize=%.3f)\n",
							 i, ids[i], expected, pred4[i * 4 + 0], pred4[i * 4 + 1],
							 pred4[i * 4 + 2], m_cellSize);
				ok = false;
				break;
			}
		}
	}

	if (ok)
		std::fprintf(stderr, "[GPUSPH] validate: spatial hash OK (N=%zu, cells=%zu)\n", n, cells);
	return ok;
}

/**
 * @brief Reads back the density/pressure buffer (debug/validate only).
 * @param out Destination vector; resized to the particle count.
 */
void GPUSPH::debugReadbackDensity(std::vector<glm::vec4>& out) const
{
	out.clear();
	if (!m_initialized || m_particleCount == 0)
		return;

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	out.resize(m_particleCount);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_densPressSSBO);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
					   static_cast<GLsizeiptr>(m_particleCount * sizeof(glm::vec4)), out.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/**
 * @brief Reads back the force and XSPH buffers (debug/validate only).
 * @param outForce Destination for the forces; resized to the particle count.
 * @param outXsph Destination for the XSPH deltas; resized to the particle count.
 */
void GPUSPH::debugReadbackForces(std::vector<glm::vec4>& outForce,
								 std::vector<glm::vec4>& outXsph) const
{
	outForce.clear();
	outXsph.clear();
	if (!m_initialized || m_particleCount == 0)
		return;

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	const GLsizeiptr bytes =
		static_cast<GLsizeiptr>(m_particleCount) * static_cast<GLsizeiptr>(sizeof(glm::vec4));
	outForce.resize(m_particleCount);
	outXsph.resize(m_particleCount);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_forceSSBO);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, outForce.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_xsphVelSSBO);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, outXsph.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/**
 * @brief Reads back the particle state (debug/validate only).
 * @param outPosWeight Destination for positions + splatWeight.
 * @param outVelAge Destination for velocities + age.
 * @param outFlags Destination for the flags plane (x = rainDrop).
 */
void GPUSPH::debugReadbackParticles(std::vector<glm::vec4>& outPosWeight,
									std::vector<glm::vec4>& outVelAge,
									std::vector<glm::vec4>& outFlags) const
{
	outPosWeight.clear();
	outVelAge.clear();
	outFlags.clear();
	if (!m_initialized || m_particleCount == 0)
		return;

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	// The SSBO stores interleaved 48-byte particles (posWeight, velAge,
	// flags): fetch the raw stream once and de-interleave on the CPU.
	std::vector<glm::vec4> raw(m_particleCount * 3);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
					   static_cast<GLsizeiptr>(raw.size() * sizeof(glm::vec4)), raw.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	outPosWeight.resize(m_particleCount);
	outVelAge.resize(m_particleCount);
	outFlags.resize(m_particleCount);
	for (size_t i = 0; i < m_particleCount; ++i) {
		outPosWeight[i] = raw[i * 3 + 0];
		outVelAge[i]	= raw[i * 3 + 1];
		outFlags[i]		= raw[i * 3 + 2];
	}
}

/**
 * @brief Releases all OpenGL resources.
 *
 * Deletes every SSBO owned by the pipeline and the rain-drop VAO. Shader
 * programs are released by their own destructors.
 */
void GPUSPH::release()
{
	const GLuint ssbos[] = {m_particleSSBO,	  m_densPressSSBO, m_forceSSBO,		m_posPredSSBO,
							m_cellIdSSBO,	  m_sortedIdxSSBO, m_cellCountSSBO, m_cellStartSSBO,
							m_cellCursorSSBO, m_blockSumsSSBO, m_xsphVelSSBO,	m_energySSBO};
	glDeleteBuffers(sizeof(ssbos) / sizeof(ssbos[0]), ssbos);

	if (m_rainVAO) {
		glDeleteVertexArrays(1, &m_rainVAO);
		m_rainVAO = 0;
	}

	m_particleSSBO	 = 0;
	m_densPressSSBO	 = 0;
	m_forceSSBO		 = 0;
	m_posPredSSBO	 = 0;
	m_cellIdSSBO	 = 0;
	m_sortedIdxSSBO	 = 0;
	m_cellCountSSBO	 = 0;
	m_cellStartSSBO	 = 0;
	m_cellCursorSSBO = 0;
	m_blockSumsSSBO	 = 0;
	m_xsphVelSSBO	 = 0;
	m_energySSBO	 = 0;

	m_waterEnergy	 = 0.f;
	m_initialized	= false;
	m_particleCount = 0;
}
