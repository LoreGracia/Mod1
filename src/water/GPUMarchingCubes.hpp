#pragma once
#include <glad/gl.h>
#include <glm.hpp>

#include <string>
#include <vector>

#include "Shader.hpp"            // Shader m_computeShader / m_meshShader
#include "Particle.hpp"    // std::vector<Particle> (ajusta la ruta si no usas -Isrc)

class GPUMarchingCubes {
public:
    GPUMarchingCubes();
    ~GPUMarchingCubes();

    // Shader es no-copyable; GPUMarchingCubes también.
    // Si lo necesitas en main, usa std::shared_ptr o std::unique_ptr.
    GPUMarchingCubes(const GPUMarchingCubes&)            = delete;
    GPUMarchingCubes& operator=(const GPUMarchingCubes&) = delete;

    // Carga compute + mesh shaders, crea SSBOs y VAO.
    //   resolution = lados de la rejilla cúbica (32, 64, 128...).
    //   Devuelve false si algún shader falla al compilar.
    bool init(const glm::vec3& boundsMin,
              const glm::vec3& boundsMax,
              int resolution,
              const std::string& splatPath,     // density_splat.comp
              const std::string& computePath,   // marching_cubes.comp
              const std::string& vertPath,      // meshVertex.glsl
              const std::string& fragPath);     // meshFrag.frag

    // Sube las posiciones de las partículas al SSBO de entrada.
    // Si el número de partículas ha crecido, reasigna el buffer (solo crece).
    void uploadParticles(const std::vector<Particle>& particles);

    // Reset del counter → bind SSBOs → uniforms → dispatch → barrier → readback.
    void generateMesh();

    // Lee el comando indirecto y dibuja los triángulos si hay > 0.
    // Debe llamarse después de generateMesh() en el mismo frame.
    void render(const glm::mat4& viewProj,
                const glm::vec3& cameraPos,
                float time);

    // Ajustes sin reasignar buffers.
    void setIsoLevel(float iso)               { m_isoLevel = iso; }
    void setBounds(const glm::vec3& lo, const glm::vec3& hi);

    // Accesores de introspección (debug / UI).
    int    resolution()    const { return m_resolution; }
    float  isoLevel()      const { return m_isoLevel; }
    bool   isInitialized() const { return m_initialized; }

    // Solo para depuración: fuerza un readback. NO usar en el bucle de render.
    GLuint debugReadDrawCount() const;

    // Libera todos los recursos GL. Idempotente.
    // Se llama desde el destructor; puedes llamarlo antes si quieres.
    void release();

private:
    // Sólo usados internamente por init() y generateMesh().
    void createBuffers();

    // ── GPU buffers ─────────────────────────────────────────────
    GLuint m_particleSSBO = 0;   // binding 0 del splat: vec4 por partícula
    GLuint m_vertexSSBO   = 0;   // binding 1: vec4 por vértice de salida
    GLuint m_drawCmdSSBO  = 0;   // binding 2: DrawArraysIndirectCommand
    GLuint m_emptyVAO     = 0;   // VAO sin VBO; lee del m_vertexSSBO
    GLuint m_densityTex   = 0;   // textura 3D GL_R32UI de densidad

    // ── Shaders ─────────────────────────────────────────────────
    Shader m_splatShader;        // density_splat.comp
    Shader m_computeShader;      // marching_cubes.comp
    Shader m_meshShader;         // meshVertex + meshFrag

    // ── Configuración (fija tras init) ──────────────────────────
    glm::vec3  m_boundsMin{0.f};
    glm::vec3  m_boundsMax{1.f};
    glm::ivec3 m_gridSize{32, 32, 32};
    glm::ivec3 m_densitySize{33, 33, 33}; // gridSize + 1 para las esquinas
    int        m_resolution = 32;
    float      m_isoLevel   = 0.5f;

    // ── Estado de los buffers ───────────────────────────────────
    GLuint m_maxParticles = 0;        // capacidad actual del particle SSBO
    GLuint m_maxVertices  = 0;        // capacidad del vertex SSBO (grid.x*grid.y*grid.z*15)
    GLuint m_particleCount = 0;       // partículas válidas en el SSBO
    bool   m_initialized  = false;
};
