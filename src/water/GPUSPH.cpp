/**
 * @file GPUSPH.cpp
 *
 * Dispatches the compute passes that replace the CPU solver: Clear & Predict
 * and the GPU spatial hash (counting sort over a uniform grid, which replaces
 * the CPU std::unordered_map of Particles::buildHash).
 */

#include <GPUSPH.hpp>
#include <algorithm>
#include <cstdio>
#include <numeric>
#include <vector>

namespace
{

/**
 * @struct GPUParticle
 * @brief GPU particle structure stored in the particle SSBO.
 *
 * Must match the GPUParticle struct layout in sph_clear_predict.comp and
 * density_splat.comp (std430, 32 bytes).
 */
struct GPUParticle
{
	glm::vec4 posWeight;	///< xyz = position, w = splatWeight.
	glm::vec4 velAge;		///< xyz = velocity, w = age.
};

/** @brief Threads per workgroup used by every phase of sph_hash.comp. */
constexpr int HASH_WORKGROUP_SIZE = 1024;

/** @brief Threads per workgroup used by sph_clear_predict.comp. */
constexpr int PREDICT_WORKGROUP_SIZE = 64;

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
 * Loads the two compute shaders of the current step, validates the uniform
 * grid against the hierarchical scan limits, and allocates all SSBOs.
 *
 * @param maxParticles Capacity of the per-particle SSBOs.
 * @param params Physics and grid parameters.
 * @param clearPredictPath Path to the Clear & Predict shader.
 * @param hashPath Path to the spatial hashing shader.
 * @return True if the shaders compiled and the buffers were created successfully.
 */
bool GPUSPH::init(size_t maxParticles, const Params& params, const std::string& clearPredictPath,
				  const std::string& hashPath)
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

	m_maxParticles   = maxParticles;
	m_particleCount  = 0;
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
	m_cellSize		 = m_params.h;
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

	m_gridSize  = g;
	m_numCells  = g.x * g.y * g.z;
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
	const GLsizeiptr cellBytes  = static_cast<GLsizeiptr>(m_numCells) * sizeof(GLuint);
	const GLsizeiptr blockBytes = static_cast<GLsizeiptr>(m_numBlocks) * sizeof(GLuint);

	// Helper: allocate one zero-filled SSBO.
	auto createSSBO = [](GLuint& id, GLsizeiptr bytes) {
		glGenBuffers(1, &id);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
		glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, nullptr, GL_DYNAMIC_DRAW);
	};

	// ── Per-particle state (SoA layout) ─────────────────────────────
	createSSBO(m_particleSSBO, particleBytes);   // binding 0: GPUParticle[]
	createSSBO(m_densPressSSBO, vec4Bytes);	   // binding 1: Phase 6.3
	createSSBO(m_forceSSBO, vec4Bytes);		   // binding 2
	createSSBO(m_posPredSSBO, vec4Bytes);	   // binding 3
	createSSBO(m_cellIdSSBO, uintBytes);		   // binding 4
	createSSBO(m_sortedIdxSSBO, uintBytes);	   // binding 5

	// ── Spatial hash grid (counting sort) ────────────────────────────
	createSSBO(m_cellCountSSBO, cellBytes);	   // binding 6
	createSSBO(m_cellStartSSBO, cellBytes);	   // binding 7
	createSSBO(m_cellCursorSSBO, cellBytes);	   // binding 8
	createSSBO(m_blockSumsSSBO, blockBytes);	   // binding 9

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
		std::fprintf(stderr, "GPUSPH: uploadParticles(%zu) exceeds capacity %zu\n", particles.size(),
					 m_maxParticles);
		return;
	}

	m_particleCount = particles.size();
	if (particles.empty())
		return;

	std::vector<GPUParticle> staging(particles.size());
	for (size_t i = 0; i < particles.size(); ++i) {
		staging[i].posWeight = glm::vec4(particles[i].position, particles[i].splatWeight);
		staging[i].velAge	 = glm::vec4(particles[i].velocity, particles[i].age);
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
					static_cast<GLsizeiptr>(staging.size() * sizeof(GPUParticle)),
					staging.data());
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
 * @brief Runs one frame of the SPH pipeline on the GPU.
 *
 * Each substep currently executes Clear & Predict + Spatial Hash; the
 * remaining passes of the roadmap are dispatched from here as they land.
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

		// TODO Phase 6.3: sph_density.comp — Poly6 density + pressure, reading
		// sortedIdx/cellStart/cellCount (27-cell sweep) into m_densPressSSBO.
		// TODO Phase 6.4: sph_forces.comp — Spiky pressure + Laplacian viscosity
		// + XSPH, accumulating on m_forceSSBO (gravity already injected).
		// TODO Phase 6.5: sph_integrate.comp — semi-implicit Euler, speed clamp
		// and boundary collisions, writing m_particleSSBO in place.
	}
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

	std::vector<GLuint>  counts(cells), starts(cells), sorted(n), ids(n);
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
			const glm::vec3 p(pred4[i * 4 + 0], pred4[i * 4 + 1], pred4[i * 4 + 2]);
			const glm::ivec3 c = glm::clamp(
				glm::ivec3(glm::floor((p - m_params.boundsMin) / m_cellSize)), glm::ivec3(0),
				m_gridSize - glm::ivec3(1));
			const GLuint expected = static_cast<GLuint>(
				c.x + m_gridSize.x * (c.y + m_gridSize.y * c.z));
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
 * @brief Releases all OpenGL resources.
 *
 * Deletes every SSBO owned by the pipeline. Shader programs are released by
 * their own destructors.
 */
void GPUSPH::release()
{
	const GLuint ssbos[] = {m_particleSSBO,	m_densPressSSBO, m_forceSSBO,	 m_posPredSSBO,
							m_cellIdSSBO,	m_sortedIdxSSBO, m_cellCountSSBO, m_cellStartSSBO,
							m_cellCursorSSBO, m_blockSumsSSBO};
	glDeleteBuffers(sizeof(ssbos) / sizeof(ssbos[0]), ssbos);

	m_particleSSBO = 0;
	m_densPressSSBO = 0;
	m_forceSSBO = 0;
	m_posPredSSBO = 0;
	m_cellIdSSBO = 0;
	m_sortedIdxSSBO = 0;
	m_cellCountSSBO = 0;
	m_cellStartSSBO = 0;
	m_cellCursorSSBO = 0;
	m_blockSumsSSBO = 0;

	m_initialized	= false;
	m_particleCount = 0;
}
