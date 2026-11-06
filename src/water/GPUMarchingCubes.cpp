#include <GPUMarchingCubes.hpp>

#include <gtc/type_ptr.hpp>

#include <cstdio>
#include <vector>

// ─────────────────────────────────────────────────────────────────────
// Constructores / destructor
// ─────────────────────────────────────────────────────────────────────

GPUMarchingCubes::GPUMarchingCubes() = default;

GPUMarchingCubes::~GPUMarchingCubes() {
    release();
}

// ─────────────────────────────────────────────────────────────────────
// init: carga shaders + crea buffers
// ─────────────────────────────────────────────────────────────────────

bool GPUMarchingCubes::init(const glm::vec3& boundsMin,
                            const glm::vec3& boundsMax,
                            int resolution,
                            const std::string& computePath,
                            const std::string& vertPath,
                            const std::string& fragPath)
{
    if (m_initialized) release();

    if (resolution < 8) {
        std::fprintf(stderr, "GPUMarchingCubes: resolution too small (%d), using 8\n", resolution);
        resolution = 8;
    }

    m_boundsMin  = boundsMin;
    m_boundsMax  = boundsMax;
    m_resolution = resolution;
    m_gridSize   = glm::ivec3(resolution, resolution, resolution);

    // Cargar compute shader (marching_cubes.comp)
    if (!m_computeShader.loadComputeFromFile(computePath)) {
        std::fprintf(stderr, "GPUMarchingCubes: failed to load '%s'\n", computePath.c_str());
        return false;
    }

    // Cargar shaders de renderizado de la malla
    if (!m_meshShader.loadFromFile(vertPath, fragPath)) {
        std::fprintf(stderr, "GPUMarchingCubes: failed to load '%s' or '%s'\n",
                     vertPath.c_str(), fragPath.c_str());
        return false;
    }

    createBuffers();
    m_initialized = true;

    std::fprintf(stderr,
                 "[GPUMarchingCubes] init ok: grid=%dx%dx%d  maxVerts=%u  (%.1f MB)\n",
                 m_gridSize.x, m_gridSize.y, m_gridSize.z,
                 m_maxVertices,
                 (m_maxVertices * sizeof(glm::vec4)) / (1024.0 * 1024.0));
    return true;
}

// ─────────────────────────────────────────────────────────────────────
// createBuffers: crea los 3 SSBOs + el VAO "vacío"
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::createBuffers() {
    // ── 1. Particle SSBO (binding 0 del compute) ────────────────────
    // Capacidad inicial: 4096 partículas. Crece en uploadParticles() si
    // el vector es mayor; nunca decrece (evita reallocaciones).
    m_maxParticles = 4096;
    glGenBuffers(1, &m_particleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 m_maxParticles * sizeof(glm::vec4),
                 nullptr,
                 GL_DYNAMIC_DRAW);

    // ── 2. Vertex SSBO (binding 1) — peor caso 15 verts/voxel ───────
    m_maxVertices = static_cast<GLuint>(m_gridSize.x) *
                    static_cast<GLuint>(m_gridSize.y) *
                    static_cast<GLuint>(m_gridSize.z) * 15u;

    glGenBuffers(1, &m_vertexSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_vertexSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(m_maxVertices) * sizeof(glm::vec4),
                 nullptr,
                 GL_DYNAMIC_DRAW);

    // ── 3. Counter SSBO (binding 2) — 1 uint ────────────────────────
    glGenBuffers(1, &m_counterSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_counterSSBO);
    GLuint zero = 0;
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), &zero, GL_DYNAMIC_DRAW);

    // ── 4. VAO "vacío" para leer el vertex SSBO como atributo ───────
    // El VAO guarda el binding entre attrib 0 y el buffer. Aunque el
    // contenido cambie por el compute, el ID del buffer no cambia,
    // así que este binding sigue siendo válido en cada draw.
    glGenVertexArrays(1, &m_emptyVAO);
    glBindVertexArray(m_emptyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexSSBO);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // Limpieza de bindings
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// ─────────────────────────────────────────────────────────────────────
// uploadParticles: sube posiciones al SSBO de entrada
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::uploadParticles(const std::vector<Particle>& particles) {
    if (!m_initialized || particles.empty())
		return;

	//std::printf("[MC] uploadParticles: %zu\n", particles.size());
    // ¿Hace falta reasignar el SSBO?
    const GLuint needed = static_cast<GLuint>(particles.size());
    if (needed > m_maxParticles) {
        GLuint newCap = m_maxParticles;
        while (newCap < needed)
			newCap *= 2;
        m_maxParticles = newCap;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     m_maxParticles * sizeof(glm::vec4),
                     nullptr,
                     GL_DYNAMIC_DRAW);
    }
	m_particleCount = static_cast<GLuint>(particles.size());
    // Empaquetar position como vec4 (xyz + w=1.0). El shader lo declara
    // como `vec4 particles[]` para respetar el alignment natural de vec4.
    std::vector<glm::vec4> staging(particles.size());
    for (size_t i = 0; i < particles.size(); ++i) {
        staging[i] = glm::vec4(particles[i].position, 1.0f);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(staging.size() * sizeof(glm::vec4)),
                    staging.data());
	std::vector<glm::vec4> dbg(3);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec4)*3, dbg.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	//std::printf("[MC] SSBO check: p0=(%.3f,%.3f,%.3f,%.3f) p1=(%.3f,%.3f,%.3f,%.3f) p2=(%.3f,%.3f,%.3f,%.3f)\n",
	//	dbg[0].x,dbg[0].y,dbg[0].z,dbg[0].w,
	//	dbg[1].x,dbg[1].y,dbg[1].z,dbg[1].w,
	//	dbg[2].x,dbg[2].y,dbg[2].z,dbg[2].w);
}

// ─────────────────────────────────────────────────────────────────────
// generateMesh: reset contador → bind SSBOs → uniforms → dispatch → barrier
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::generateMesh() {

	if (!m_initialized)
		return;
	//std::printf("[MC] generateMesh: particleCount=%u isoLevel=%.4f\n", m_particleCount, m_isoLevel);
	// 1) Reset del contador atómico a 0. Cada dispatch empieza de cero,
    //    si no el draw leería basura del frame anterior.
	{
		GLuint zero = 0;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_counterSSBO);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		m_cachedVertexCount = 0;
	}

	if (m_particleCount == 0) {
		m_cachedVertexCount = 0;
		return;
	}

    // 2) Bindear los 3 SSBOs a sus binding points. Deben coincidir con
    //    los `layout(std430, binding = N)` del .comp.
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_vertexSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_counterSSBO);

    // 3) Uniforms
    m_computeShader.use();
	m_computeShader.setVec3 ("uGridMin",        m_boundsMin);
	m_computeShader.setVec3 ("uGridMax",        m_boundsMax);
	m_computeShader.setIVec3("uGridSize",       m_gridSize);   // ← setIVec3
	// m_computeShader.setFloat("uIsoLevel",       m_isoLevel);
	m_computeShader.setFloat("uParticleRadius", 0.4f);         // ← NUEVO
	m_computeShader.setInt("uParticleCount", static_cast<int>(m_particleCount));

	// std::printf("[MC] dispatching %u x %u x %u groups\n",
    //             (m_gridSize.x + 3) / 4,
    //             (m_gridSize.y + 3) / 4,
    //             (m_gridSize.z + 3) / 4); 

    // 4) Dispatch. Asumimos local_size = (4,4,4) en el .comp.
    m_computeShader.dispatchForGrid(
        static_cast<GLuint>(m_gridSize.x),
        static_cast<GLuint>(m_gridSize.y),
        static_cast<GLuint>(m_gridSize.z),
        4, 4, 4);

    // 5) Barrera de memoria. Cubre los dos consumidores del resultado:
    //    - glGetBufferSubData (leer el counter) → GL_BUFFER_UPDATE_BARRIER_BIT
    //    - vertex attrib (leer los vértices)    → GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT
    //    Un único barrier con ambos bits es suficiente.
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT
                  | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    // 6) Leer cuántos vértices generó realmente el compute
    m_cachedVertexCount = readVertexCounter();
	//std::printf("[MC] vertexCount after dispatch = %u\n", m_cachedVertexCount);
}

// ─────────────────────────────────────────────────────────────────────
// readVertexCounter: glGetBufferSubData del counter SSBO
// ─────────────────────────────────────────────────────────────────────

GLuint GPUMarchingCubes::readVertexCounter() const {
    GLuint count = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_counterSSBO);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &count);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return count;
}

// ─────────────────────────────────────────────────────────────────────
// render: dibuja los triángulos si hay alguno
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::render(const glm::mat4& viewProj) {
    if (!m_initialized || m_cachedVertexCount == 0) return;

    m_meshShader.use();
    m_meshShader.setMat4("uViewProj", viewProj);

    glBindVertexArray(m_emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_cachedVertexCount));
    glBindVertexArray(0);
}

// ─────────────────────────────────────────────────────────────────────
// setBounds: cambia la AABB sin reasignar buffers
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::setBounds(const glm::vec3& lo, const glm::vec3& hi) {
    m_boundsMin = lo;
    m_boundsMax = hi;
    // m_gridSize no cambia → el vertex SSBO mantiene su tamaño.
    // Solo los uniforms uGridMin/uGridMax se actualizarán en el próximo dispatch.
}

// ─────────────────────────────────────────────────────────────────────
// release: libera todos los recursos GL (idempotente)
// ─────────────────────────────────────────────────────────────────────

void GPUMarchingCubes::release() {
    if (m_particleSSBO) { glDeleteBuffers(1, &m_particleSSBO);  m_particleSSBO = 0; }
    if (m_vertexSSBO)   { glDeleteBuffers(1, &m_vertexSSBO);    m_vertexSSBO   = 0; }
    if (m_counterSSBO)  { glDeleteBuffers(1, &m_counterSSBO);   m_counterSSBO  = 0; }
    if (m_emptyVAO)     { glDeleteVertexArrays(1, &m_emptyVAO); m_emptyVAO     = 0; }

    m_maxParticles		= 0;
    m_maxVertices		= 0;
    m_cachedVertexCount	= 0;
	m_particleCount		= 0;
    m_initialized		= false;
}