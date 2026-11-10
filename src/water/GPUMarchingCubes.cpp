#include <GPUMarchingCubes.hpp>
#include <cstdio>
#include <gtc/type_ptr.hpp>
#include <vector>

namespace
{
struct GPUVertex
{
	glm::vec4 position;
	glm::vec4 normal;
};

// Debe coincidir con GPUParticle de density_splat.comp (std430, 32 bytes).
struct GPUParticle
{
	glm::vec4 posWeight;  // xyz = posición, w = splatWeight
	glm::vec4 velAge;	  // xyz = velocidad,  w = edad
};
}  // namespace

// ─────────────────────────────────────────────────────────────────────
// Constructores / destructor
// ─────────────────────────────────────────────────────────────────────

GPUMarchingCubes::GPUMarchingCubes() = default;

GPUMarchingCubes::~GPUMarchingCubes()
{
	release();
}

// ─────────────────────────────────────────────────────────────────────
// init: carga shaders + crea buffers
// ─────────────────────────────────────────────────────────────────────

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

	// Cargar compute shader de splatting (density_splat.comp)
	if (!m_splatShader.loadComputeFromFile(splatPath)) {
		std::fprintf(stderr, "GPUMarchingCubes: failed to load '%s'\n", splatPath.c_str());
		return false;
	}

	// Cargar compute shader (marching_cubes.comp)
	if (!m_computeShader.loadComputeFromFile(computePath)) {
		std::fprintf(stderr, "GPUMarchingCubes: failed to load '%s'\n", computePath.c_str());
		return false;
	}

	// Cargar shaders de renderizado de la malla
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

// ─────────────────────────────────────────────────────────────────────
// createBuffers: SSBOs + textura 3D de densidad + VAO "vacío"
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::createBuffers()
{
	// ── 1. Particle SSBO (binding 0 del splat) ─────────────────────
	// Capacidad inicial: 4096 partículas. Crece en uploadParticles() si
	// el vector es mayor; nunca decrece (evita reallocaciones).
	m_maxParticles = 4096;
	glGenBuffers(1, &m_particleSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, m_maxParticles * sizeof(GPUParticle), nullptr,
				 GL_DYNAMIC_DRAW);

	// ── 2. Vertex SSBO (binding 1) — peor caso 15 verts/voxel ───────
	m_maxVertices = static_cast<GLuint>(m_gridSize.x) * static_cast<GLuint>(m_gridSize.y) *
					static_cast<GLuint>(m_gridSize.z) * 15u;

	glGenBuffers(1, &m_vertexSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_vertexSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
				 static_cast<GLsizeiptr>(m_maxVertices) * sizeof(GPUVertex), nullptr,
				 GL_DYNAMIC_DRAW);

	// ── 3. Draw command SSBO (binding 2) ────────────────────────────
	// { vertexCount, instanceCount, firstVertex, baseInstance }
	// Lo reinicia density_splat.comp y lo incrementa marching_cubes.comp.
	glGenBuffers(1, &m_drawCmdSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_drawCmdSSBO);
	const GLuint drawCmdInit[4] = {0u, 1u, 0u, 0u};
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(drawCmdInit), drawCmdInit, GL_DYNAMIC_DRAW);

	// ── 4. Textura 3D de densidad (R32UI, N+1 por eje) ──────────────
	glGenTextures(1, &m_densityTex);
	glBindTexture(GL_TEXTURE_3D, m_densityTex);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, m_densitySize.x, m_densitySize.y, m_densitySize.z);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_3D, 0);

	// ── 5. VAO "vacío" para leer el vertex SSBO como atributo ───────
	// El VAO guarda el binding entre attrib 0 y el buffer. Aunque el
	// contenido cambie por el compute, el ID del buffer no cambia,
	// así que este binding sigue siendo válido en cada draw.
	glGenVertexArrays(1, &m_emptyVAO);
	glBindVertexArray(m_emptyVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_vertexSSBO);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GPUVertex), reinterpret_cast<void*>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GPUVertex),
						  reinterpret_cast<void*>(sizeof(glm::vec4)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	// Limpieza de bindings
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// ─────────────────────────────────────────────────────────────────────
// uploadParticles: sube posiciones al SSBO de entrada
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::uploadParticles(const std::vector<Particle>& particles)
{
	if (!m_initialized)
		return;

	m_particleCount = static_cast<GLuint>(particles.size());
	if (particles.empty())
		return;

	// ¿Hace falta reasignar el SSBO?
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

	// Empaquetar posición + splatWeight y velocidad + edad. El shader
	// declara el SSBO como GPUParticle para respetar el alignment de vec4.
	std::vector<GPUParticle> staging(particles.size());
	for (size_t i = 0; i < particles.size(); ++i) {
		staging[i].posWeight = glm::vec4(particles[i].position, particles[i].splatWeight);
		staging[i].velAge	 = glm::vec4(particles[i].velocity, particles[i].age);
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
					static_cast<GLsizeiptr>(staging.size() * sizeof(GPUParticle)), staging.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// ─────────────────────────────────────────────────────────────────────
// generateMesh: splat de densidad → Marching Cubes → comando indirecto
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::generateMesh()
{
	if (!m_initialized)
		return;

	constexpr GLuint kSplatGroup = 64u;

	// ── 1. Limpiar la textura 3D de densidad a 0 ───────────────────
	// CRÍTICO: sin este clear los imageAtomicAdd se acumulan frame tras
	// frame y todo el volumen acaba superando el iso → cubo sólido.
	// Si GL_ARB_clear_texture no está disponible, se sube un bloque de
	// ceros con glTexSubImage3D (fallback portable).
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
	// Hacer visible el clear al pase de splatting (imageAtomicAdd).
	glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// ── 2. Pase de splatting: partículas → textura 3D ──────────────
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particleSSBO);
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
	m_splatShader.setInt("uParticleCount", static_cast<int>(m_particleCount));

	GLuint splatGroups = (m_particleCount + kSplatGroup - 1u) / kSplatGroup;
	if (splatGroups == 0u)
		splatGroups = 1u;  // al menos 1 para resetear el comando
	m_splatShader.dispatch(splatGroups, 1, 1);

	// Textura escrita como imagen + draw command reiniciado por el shader.
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

	// ── 3. Marching Cubes: lee la textura y escribe vértices ───────
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

	// Vértices listos para el VAO y comando listo para el draw indirecto.
	glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

// ─────────────────────────────────────────────────────────────────────
// debugReadDrawCount: readback puntual para depuración (rompe el pipeline)
// ─────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────
// render: dibuja usando el comando generado en GPU (sin readback)
// ─────────────────────────────────────────────────────────────────────
void GPUMarchingCubes::render(const glm::mat4& viewProj, const glm::vec3& cameraPos, float time)
{
	if (!m_initialized)
		return;

	m_meshShader.use();
	m_meshShader.setMat4("uViewProj", viewProj);
	m_meshShader.setVec3("uCameraPos", cameraPos);
	m_meshShader.setVec3("uSunDir", glm::normalize(glm::vec3(0.55f, 1.0f, 0.35f)));
	m_meshShader.setVec3("uSunColor", glm::vec3(1.00f, 0.95f, 0.82f));
	m_meshShader.setVec3("uWaterDeep", glm::vec3(0.015f, 0.10f, 0.28f));
	m_meshShader.setVec3("uWaterShallow", glm::vec3(0.10f, 0.48f, 0.78f));
	m_meshShader.setVec3("uBoundsMin", m_boundsMin);
	m_meshShader.setVec3("uBoundsMax", m_boundsMax);
	m_meshShader.setFloat("uTime", time);

	glBindVertexArray(m_emptyVAO);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCmdSSBO);
	glDrawArraysIndirect(GL_TRIANGLES, nullptr);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
}

// ─────────────────────────────────────────────────────────────────────
// setBounds: cambia la AABB sin reasignar buffers
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::setBounds(const glm::vec3& lo, const glm::vec3& hi)
{
	m_boundsMin = lo;
	m_boundsMax = hi;
	// m_gridSize no cambia → el vertex SSBO mantiene su tamaño.
	// Solo los uniforms uGridMin/uGridMax se actualizarán en el próximo dispatch.
}

// ─────────────────────────────────────────────────────────────────────
// release: libera todos los recursos GL (idempotente)
// ─────────────────────────────────────────────────────────────────────

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