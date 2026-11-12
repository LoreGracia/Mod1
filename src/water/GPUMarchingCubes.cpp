/**
 * @file GPUMarchingCubes.cpp
 * @brief Implementation of the GPUMarchingCubes pipeline.
 *
 * Manages the 3D density texture, the two compute shader passes (density
 * splatting and Marching Cubes), and the indirect draw of the generated mesh.
 */

#include <GPUMarchingCubes.hpp>
#include <cstdio>
#include <gtc/type_ptr.hpp>
#include <vector>

namespace
{
/**
 * @struct GPUVertex
 * @brief Output vertex structure written by the Marching Cubes compute shader.
 * Must match the Vertex struct layout in marching_cubes.comp (std430).
 */
struct GPUVertex
{
	glm::vec4 position;	 ///< xyz = position, w = 1.0
	glm::vec4 normal;	 ///< xyz = smooth normal, w = 0.0
};

/**
 * @struct GPUParticle
 * @brief Input particle structure consumed by the density splatting compute shader.
 * Must match the GPUParticle struct layout in density_splat.comp (std430, 48 bytes).
 */
struct GPUParticle
{
	glm::vec4 posWeight;  ///< xyz = position, w = splatWeight
	glm::vec4 velAge;	  ///< xyz = velocity, w = age
	glm::vec4 flags;	  ///< x = rainDrop flag (1 = rain drop), y,z,w = unused
};
}  // namespace

/** @brief Default constructor. */
GPUMarchingCubes::GPUMarchingCubes() = default;

/** @brief Destructor. Ensures all GL resources are released. */
GPUMarchingCubes::~GPUMarchingCubes()
{
	release();
}

/**
 * @brief Initializes the Marching Cubes pipeline.
 *
 * Loads the three required shaders (splat, compute/MC, mesh rendering),
 * validates the grid resolution, and allocates all necessary GPU resources.
 *
 * @param boundsMin The minimum world-space bound of the simulation grid.
 * @param boundsMax The maximum world-space bound of the simulation grid.
 * @param resolution The voxel resolution per axis.
 * @param splatPath Path to the density splatting compute shader.
 * @param computePath Path to the Marching Cubes compute shader.
 * @param vertPath Path to the mesh vertex shader.
 * @param fragPath Path to the mesh fragment shader.
 * @return True if all shaders compiled and buffers were created successfully.
 */
bool GPUMarchingCubes::init(const glm::vec3& boundsMin, const glm::vec3& boundsMax, int resolution,
							const std::string& splatPath, const std::string& computePath,
							const std::string& vertPath, const std::string& fragPath)
{
	if (m_initialized)
		release();

	if (resolution < 8) {
		std::fprintf(stderr, "GPUMarchingCubes: resolution too small (%d), using 8\n", resolution);
		resolution = 8;
	}

	m_boundsMin	  = boundsMin;
	m_boundsMax	  = boundsMax;
	m_resolution  = resolution;
	m_gridSize	  = glm::ivec3(resolution, resolution, resolution);
	m_densitySize = m_gridSize + glm::ivec3(1);

	// Load the density splatting compute shader (density_splat.comp)
	if (!m_splatShader.loadComputeFromFile(splatPath)) {
		std::fprintf(stderr, "GPUMarchingCubes: failed to load '%s'\n", splatPath.c_str());
		return false;
	}

	// Load the Marching Cubes compute shader (marching_cubes.comp)
	if (!m_computeShader.loadComputeFromFile(computePath)) {
		std::fprintf(stderr, "GPUMarchingCubes: failed to load '%s'\n", computePath.c_str());
		return false;
	}

	// Load the mesh rendering shaders
	if (!m_meshShader.loadFromFile(vertPath, fragPath)) {
		std::fprintf(stderr, "GPUMarchingCubes: failed to load '%s' or '%s'\n", vertPath.c_str(),
					 fragPath.c_str());
		return false;
	}

	createBuffers();
	m_initialized = true;

	std::fprintf(
		stderr,
		"[GPUMarchingCubes] init ok: grid=%dx%dx%d  density=%dx%dx%d  maxVerts=%u  (%.1f MB)\n",
		m_gridSize.x, m_gridSize.y, m_gridSize.z, m_densitySize.x, m_densitySize.y, m_densitySize.z,
		m_maxVertices, (m_maxVertices * sizeof(GPUVertex)) / (1024.0 * 1024.0));
	return true;
}

/**
 * @brief Allocates all OpenGL buffers, textures, and vertex array objects.
 *
 * Creates the particle SSBO, vertex SSBO, draw command SSBO, the 3D density
 * texture, and configures the empty VAO for vertex pulling.
 */
void GPUMarchingCubes::createBuffers()
{
	// ── 1. Particle SSBO (binding 0 of the splat shader) ────────────
	// Initial capacity: 4096 particles. Grows in uploadParticles() if
	// the vector is larger; never shrinks (avoids reallocations).
	m_maxParticles = 4096;
	glGenBuffers(1, &m_particleSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, m_maxParticles * sizeof(GPUParticle), nullptr,
				 GL_DYNAMIC_DRAW);

	// ── 2. Vertex SSBO (binding 1) — worst case 15 verts/voxel ──────
	m_maxVertices = static_cast<GLuint>(m_gridSize.x) * static_cast<GLuint>(m_gridSize.y) *
					static_cast<GLuint>(m_gridSize.z) * 15u;

	glGenBuffers(1, &m_vertexSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_vertexSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
				 static_cast<GLsizeiptr>(m_maxVertices) * sizeof(GPUVertex), nullptr,
				 GL_DYNAMIC_DRAW);

	// ── 3. Draw command SSBO (binding 2) ────────────────────────────
	// { vertexCount, instanceCount, firstVertex, baseInstance }
	// Reset by density_splat.comp and incremented by marching_cubes.comp.
	glGenBuffers(1, &m_drawCmdSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_drawCmdSSBO);
	const GLuint drawCmdInit[4] = {0u, 1u, 0u, 0u};
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(drawCmdInit), drawCmdInit, GL_DYNAMIC_DRAW);

	// ── 4. 3D Density Texture (R32UI, N+1 per axis) ─────────────────
	glGenTextures(1, &m_densityTex);
	glBindTexture(GL_TEXTURE_3D, m_densityTex);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, m_densitySize.x, m_densitySize.y, m_densitySize.z);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_3D, 0);

	// ── 5. "Empty" VAO to read the vertex SSBO as attributes ────────
	// The VAO stores the binding between the attributes and the buffer.
	// Although the content changes due to compute, the buffer ID does not,
	// so this binding remains valid for every draw call.
	glGenVertexArrays(1, &m_emptyVAO);
	glBindVertexArray(m_emptyVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_vertexSSBO);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GPUVertex), reinterpret_cast<void*>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GPUVertex),
						  reinterpret_cast<void*>(sizeof(glm::vec4)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	// Cleanup bindings
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/**
 * @brief Uploads the CPU particle data to the GPU particle SSBO.
 *
 * Packs the particle data into the GPUParticle struct (posWeight and velAge)
 * and uploads it. Reallocates the SSBO if the particle count exceeds the
 * current capacity.
 *
 * @param particles A vector containing the current CPU particles.
 */
void GPUMarchingCubes::uploadParticles(const std::vector<Particle>& particles)
{
	if (!m_initialized)
		return;

	m_particleCount = static_cast<GLuint>(particles.size());
	if (particles.empty())
		return;

	// Reallocation check
	const GLuint needed = static_cast<GLuint>(particles.size());
	if (needed > m_maxParticles) {
		GLuint newCap = m_maxParticles;
		while (newCap < needed)
			newCap *= 2;
		m_maxParticles = newCap;

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, m_maxParticles * sizeof(GPUParticle), nullptr,
					 GL_DYNAMIC_DRAW);
	}

	// Pack position + splatWeight and velocity + age. The shader declares
	// the SSBO as GPUParticle to respect the vec4 alignment.
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
 * @brief Binds an externally owned particle SSBO (zero-copy handoff).
 *
 * Once called, generateMesh() reads the given buffer (GPU SPH solver
 * particle state) instead of the CPU-uploaded SSBO.
 *
 * @param bufferId OpenGL handle of the external particle SSBO.
 * @param particleCount Number of valid particles in the buffer.
 */
void GPUMarchingCubes::setExternalParticleBuffer(GLuint bufferId, GLuint particleCount)
{
	if (!m_initialized || bufferId == 0)
		return;

	m_externalParticleSSBO  = bufferId;
	m_externalParticleCount = particleCount;
	m_useExternalParticles  = true;
}

/**
 * @brief Executes the full mesh generation pipeline on the GPU.
 *
 * 1. Clears the 3D density texture (essential to prevent accumulation).
 * 2. Runs the density splatting compute shader (particles -> texture).
 * 3. Runs the Marching Cubes compute shader (texture -> vertex SSBO).
 * Inserts the necessary memory barriers between passes.
 */
void GPUMarchingCubes::generateMesh()
{
	if (!m_initialized)
		return;

	constexpr GLuint kSplatGroup = 64u;

	// ── 1. Clear the 3D density texture to 0 ───────────────────────
	// CRITICAL: without this clear, imageAtomicAdds accumulate frame after
	// frame and the entire volume eventually exceeds the iso-level -> solid cube.
	// If GL_ARB_clear_texture is unavailable, upload a zero block using
	// glTexSubImage3D (portable fallback).
	if (glad_glClearTexImage != nullptr) {
		const GLuint zero = 0u;
		glClearTexImage(m_densityTex, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
	} else {
		std::vector<GLuint> zeros(static_cast<size_t>(m_densitySize.x) *
									  static_cast<size_t>(m_densitySize.y) *
									  static_cast<size_t>(m_densitySize.z),
								  0u);
		glBindTexture(GL_TEXTURE_3D, m_densityTex);
		glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, m_densitySize.x, m_densitySize.y,
						m_densitySize.z, GL_RED_INTEGER, GL_UNSIGNED_INT, zeros.data());
		glBindTexture(GL_TEXTURE_3D, 0);
	}
	// Make the clear visible to the splatting pass (imageAtomicAdd).
	glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// ── 2. Splatting pass: particles -> 3D texture ─────────────────
	// Zero-copy handoff: with an external particle SSBO bound (produced by
	// the GPU SPH solver), consume it directly instead of the CPU-uploaded
	// buffer; the per-frame uploadParticles() call is skipped entirely.
	const GLuint particleSSBO  = m_useExternalParticles ? m_externalParticleSSBO : m_particleSSBO;
	const GLuint particleCount = m_useExternalParticles ? m_externalParticleCount : m_particleCount;

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_drawCmdSSBO);
	glBindImageTexture(0, m_densityTex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);

	m_splatShader.use();
	m_splatShader.setVec3("uGridMin", m_boundsMin);
	m_splatShader.setVec3("uGridMax", m_boundsMax);
	m_splatShader.setIVec3("uGridSize", m_gridSize);
	m_splatShader.setFloat("uParticleRadius", m_particleRadius);
	m_splatShader.setFloat("uDensityScale", m_densityScale);
	m_splatShader.setFloat("uFallSpeedCutoff", m_fallSpeedCutoff);
	m_splatShader.setFloat("uSplatHeight", m_splatHeight);
	m_splatShader.setFloat("uSplatMaxHeight", m_splatMaxHeight);
	m_splatShader.setFloat("uSettleSpeed", m_settleSpeed);
	m_splatShader.setFloat("uSplatYScale", m_splatYScale);
	m_splatShader.setInt("uParticleCount", static_cast<int>(particleCount));

	GLuint splatGroups = (particleCount + kSplatGroup - 1u) / kSplatGroup;
	if (splatGroups == 0u)
		splatGroups = 1u;  // at least 1 to reset the command
	m_splatShader.dispatch(splatGroups, 1, 1);

	// Texture written as image + draw command reset by the shader.
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

	// ── 3. Marching Cubes: reads texture and writes vertices ───────
	glBindImageTexture(0, m_densityTex, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_vertexSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_drawCmdSSBO);

	m_computeShader.use();
	m_computeShader.setVec3("uGridMin", m_boundsMin);
	m_computeShader.setVec3("uGridMax", m_boundsMax);
	m_computeShader.setIVec3("uGridSize", m_gridSize);
	m_computeShader.setFloat("uIsoLevel", m_isoLevel);
	m_computeShader.setFloat("uDensityScale", m_densityScale);

	m_computeShader.dispatchForGrid(static_cast<GLuint>(m_gridSize.x),
									static_cast<GLuint>(m_gridSize.y),
									static_cast<GLuint>(m_gridSize.z), 4, 4, 4);

	// Vertices ready for the VAO and command ready for indirect draw.
	glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

/**
 * @brief Reads the generated vertex count from the GPU draw command buffer.
 * @note This introduces a synchronous CPU-GPU stall and should ONLY be used
 * for debugging purposes, never in the hot path.
 * @return The current vertex count.
 */
GLuint GPUMarchingCubes::debugReadDrawCount() const
{
	GLuint count = 0;
	if (!m_initialized)
		return 0;

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_drawCmdSSBO);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &count);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	return count;
}

/**
 * @brief Renders the generated Marching Cubes mesh.
 *
 * Uses glDrawArraysIndirect with the draw command populated entirely on the GPU.
 * Sets up the required rendering uniforms (lighting, camera, water colors).
 *
 * @param viewProj The combined view-projection matrix.
 * @param cameraPos The world-space position of the camera.
 * @param time The current application time (for wave animation).
 */
void GPUMarchingCubes::render(const glm::mat4& viewProj, const glm::vec3& cameraPos, float time)
{
	if (!m_initialized)
		return;

	m_meshShader.use();
	m_meshShader.setMat4("uViewProj", viewProj);
	m_meshShader.setVec3("uCameraPos", cameraPos);
	m_meshShader.setVec3("uSunDir", glm::normalize(glm::vec3(0.55f, 1.0f, 0.35f)));
	m_meshShader.setVec3("uSunColor", glm::vec3(1.00f, 0.95f, 0.82f));
	// Procedural sky sampled by the water reflection vector: warm horizon,
	// deep zenith and a warm sun halo.
	m_meshShader.setVec3("uSkyHorizon", glm::vec3(0.82f, 0.66f, 0.50f));
	m_meshShader.setVec3("uSkyZenith", glm::vec3(0.06f, 0.18f, 0.42f));
	m_meshShader.setVec3("uSunGlowColor", glm::vec3(1.00f, 0.83f, 0.58f));
	m_meshShader.setVec3("uWaterDeep", glm::vec3(0.015f, 0.10f, 0.28f));
	m_meshShader.setVec3("uWaterShallow", glm::vec3(0.10f, 0.48f, 0.78f));
	m_meshShader.setVec3("uBoundsMin", m_boundsMin);
	m_meshShader.setVec3("uBoundsMax", m_boundsMax);
	m_meshShader.setFloat("uTime", time);

	// Procedural foam driven by surface curvature, the wave-field flow and
	// fractal noise. Tuned to appear as sparse patches on the wave crests.
	m_meshShader.setVec3("uFoamColor", glm::vec3(0.92f, 0.96f, 1.00f));
	m_meshShader.setFloat("uFoamIntensity", 1.40f);
	m_meshShader.setFloat("uFoamThreshold", 0.35f);
	m_meshShader.setFloat("uFoamScale", 4.50f);
	m_meshShader.setFloat("uFoamSpeed", 2.50f);

	glBindVertexArray(m_emptyVAO);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCmdSSBO);
	glDrawArraysIndirect(GL_TRIANGLES, nullptr);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
}

/**
 * @brief Updates the world-space bounds of the simulation grid.
 * @param lo The new minimum bound.
 * @param hi The new maximum bound.
 */
void GPUMarchingCubes::setBounds(const glm::vec3& lo, const glm::vec3& hi)
{
	m_boundsMin = lo;
	m_boundsMax = hi;
	// m_gridSize does not change -> the vertex SSBO keeps its size.
	// Only the uGridMin/uGridMax uniforms will update on the next dispatch.
}

/**
 * @brief Releases all allocated OpenGL resources.
 */
void GPUMarchingCubes::release()
{
	if (m_particleSSBO) {
		glDeleteBuffers(1, &m_particleSSBO);
		m_particleSSBO = 0;
	}
	if (m_vertexSSBO) {
		glDeleteBuffers(1, &m_vertexSSBO);
		m_vertexSSBO = 0;
	}
	if (m_drawCmdSSBO) {
		glDeleteBuffers(1, &m_drawCmdSSBO);
		m_drawCmdSSBO = 0;
	}
	if (m_emptyVAO) {
		glDeleteVertexArrays(1, &m_emptyVAO);
		m_emptyVAO = 0;
	}
	if (m_densityTex) {
		glDeleteTextures(1, &m_densityTex);
		m_densityTex = 0;
	}

	m_maxParticles	= 0;
	m_maxVertices	= 0;
	m_particleCount = 0;
	m_initialized	= false;
}
