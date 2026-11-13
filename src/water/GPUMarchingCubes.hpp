#pragma once
#include <glad/gl.h>

#include <Particle.hpp>
#include <Shader.hpp>
#include <glm.hpp>
#include <string>
#include <vector>

/**
 * @file GPUMarchingCubes.hpp
 * @brief Defines the GPUMarchingCubes class, which orchestrates the GPU-based
 * Marching Cubes pipeline (density splatting + surface extraction).
 */

/**
 * @class GPUMarchingCubes
 * @brief Manages the full GPU Marching Cubes pipeline.
 *
 * This class owns the 3D density texture, the compute shaders (splatting and
 * Marching Cubes), the vertex/draw command SSBOs, and the VAO used for
 * indirect rendering. It transforms CPU particles into a rendered water mesh
 * entirely on the GPU. When an external particle SSBO is bound through
 * setExternalParticleBuffer(), the splat pass consumes GPU-resident
 * particles directly and the CPU upload path is skipped entirely.
 *
 * @note This class is non-copyable due to its ownership of unique GPU resources
 * and Shader objects. Use std::shared_ptr or std::unique_ptr if needed.
 */
class GPUMarchingCubes
{
public:
	/** @brief Default constructor. */
	GPUMarchingCubes();

	/** @brief Destructor. Calls release() to free GPU resources. */
	~GPUMarchingCubes();

	// Shader is non-copyable; GPUMarchingCubes is also non-copyable.
	// If needed in main, use std::shared_ptr or std::unique_ptr.
	GPUMarchingCubes(const GPUMarchingCubes&)			 = delete;
	GPUMarchingCubes& operator=(const GPUMarchingCubes&) = delete;

	/**
	 * @brief Initializes the pipeline: loads shaders, creates SSBOs and the VAO.
	 *
	 * @param boundsMin Minimum world-space bound of the simulation grid.
	 * @param boundsMax Maximum world-space bound of the simulation grid.
	 * @param resolution Side length of the cubic grid (32, 64, 128...).
	 * @param splatPath Path to the density splatting compute shader (density_splat.comp).
	 * @param computePath Path to the Marching Cubes compute shader (marching_cubes.comp).
	 * @param vertPath Path to the mesh vertex shader (meshVertex.glsl).
	 * @param fragPath Path to the mesh fragment shader (meshFrag.frag).
	 * @return True if all shaders compiled and buffers were created successfully.
	 */
	bool init(const glm::vec3& boundsMin, const glm::vec3& boundsMax, int resolution,
			  const std::string& splatPath,	   // density_splat.comp
			  const std::string& computePath,  // marching_cubes.comp
			  const std::string& vertPath,	   // meshVertex.glsl
			  const std::string& fragPath);	   // meshFrag.frag

	/**
	 * @brief Uploads particle data to the input SSBO.
	 *
	 * If the number of particles has grown, reallocates the buffer
	 * (it only grows, never shrinks).
	 *
	 * @param particles The CPU particle data to upload.
	 */
	void uploadParticles(const std::vector<Particle>& particles);

	/**
	 * @brief Binds an externally owned particle SSBO (zero-copy handoff).
	 *
	 * The splat pass consumes the given buffer directly as its binding 0
	 * instead of this class' own SSBO, so no per-frame CPU upload happens
	 * while an external buffer is set. The buffer must contain GPUParticle
	 * records (48 bytes: posWeight, velAge, flags) with std430 layout, as
	 * written by the GPU SPH solver.
	 *
	 * @param bufferId OpenGL handle of the external particle SSBO.
	 * @param particleCount Number of valid particles in the buffer.
	 */
	void setExternalParticleBuffer(GLuint bufferId, GLuint particleCount);

	/**
	 * @brief Executes the mesh generation pipeline on the GPU.
	 *
	 * Clears the density texture, dispatches the splatting compute shader,
	 * then dispatches the Marching Cubes compute shader. Memory barriers
	 * are inserted between passes.
	 */
	void generateMesh();

	/**
	 * @brief Renders the generated mesh using indirect drawing.
	 *
	 * Must be called after generateMesh() in the same frame.
	 *
	 * @param viewProj The combined view-projection matrix.
	 * @param cameraPos The world-space position of the camera.
	 * @param time The current application time (for wave animation).
	 */
	void render(const glm::mat4& viewProj, const glm::vec3& cameraPos, float time);

	/** @brief Sets the isosurface extraction threshold. */
	void setIsoLevel(float iso) { m_isoLevel = iso; }

	/**
	 * @brief Updates the simulation bounds without reallocating buffers.
	 * @param lo The new minimum bound.
	 * @param hi The new maximum bound.
	 */
	void setBounds(const glm::vec3& lo, const glm::vec3& hi);

	/** @brief Sets the density scale factor for fixed-point conversion. */
	void setDensityScale(float s) { m_densityScale = s; }

	/** @brief Sets the radius of influence for the metaball kernel. */
	void setParticleRadius(float r) { m_particleRadius = r; }

	/** @brief Sets the vertical speed cutoff below which particles are ignored. */
	void setFallSpeedCutoff(float vy) { m_fallSpeedCutoff = vy; }

	/** @brief Sets the vertical band (relative to boundsMin.y) for splatting. */
	void setSplatHeight(float h) { m_splatHeight = h; }

	/** @brief Sets the absolute ceiling above which density is never injected. */
	void setSplatMaxHeight(float h) { m_splatMaxHeight = h; }

	/** @brief Sets the maximum vertical speed for a particle to be considered settled. */
	void setSettleSpeed(float v) { m_settleSpeed = v; }

	/** @brief Sets the vertical compression factor for the metaball kernel (>1 flattens). */
	void setSplatYScale(float s) { m_splatYScale = s; }

	/**
	 * @brief Sets the fluid energy used to attenuate the shader micro-waves.
	 * @param energy Mean squared particle speed of the simulation, usually
	 * queried from GPUSPH::waterEnergy().
	 */
	void setWaterEnergy(float energy) { m_waterEnergy = energy; }

	/**
	 * @brief Sets the world-space height of the settled water surface.
	 * @param level Height used by the vertex shader when flattening the mesh.
	 */
	void setWaterLevel(float level) { m_waterLevel = level; }

	/** @brief Sets the fill level scale (multiplier for the particle radius). */
	void setFillScale(float s) { m_fillScale = s; }

	/** @brief Gets the grid resolution per axis. */
	int resolution() const { return m_resolution; }

	/** @brief Gets the current isosurface level. */
	float isoLevel() const { return m_isoLevel; }

	/** @brief Checks if the pipeline has been initialized. */
	bool isInitialized() const { return m_initialized; }

	/**
	 * @brief Forces a synchronous readback of the draw command.
	 * @note This breaks the GPU pipeline and should NEVER be used in the render loop.
	 * Only for debugging purposes.
	 * @return The current vertex count.
	 */
	GLuint debugReadDrawCount() const;

	/**
	 * @brief Releases all OpenGL resources.
	 * @note Idempotent. Called from the destructor; can be called manually if needed.
	 */
	void release();

private:
	/** @brief Internal helper to create SSBOs, 3D texture, and VAO. */
	void createBuffers();

	GLuint m_particleSSBO = 0;	///< Binding 0 of splat: GPUParticle struct.
	GLuint m_vertexSSBO	  = 0;	///< Binding 1: GPUVertex struct per output vertex.
	GLuint m_drawCmdSSBO  = 0;	///< Binding 2: DrawArraysIndirectCommand.
	GLuint m_emptyVAO	  = 0;	///< VAO without VBO; reads from m_vertexSSBO.
	GLuint m_densityTex	  = 0;	///< 3D GL_R32UI density texture.

	Shader m_splatShader;	 ///< density_splat.comp
	Shader m_computeShader;	 ///< marching_cubes.comp
	Shader m_meshShader;	 ///< meshVertex + meshFrag

	glm::vec3  m_boundsMin{0.f};		///< Minimum world-space bound.
	glm::vec3  m_boundsMax{1.f};		///< Maximum world-space bound.
	glm::ivec3 m_gridSize{32, 32, 32};	///< Number of voxels per axis.
	glm::ivec3 m_densitySize{
		33, 33, 33};  ///< gridSize + 1 for the corners.  // gridSize + 1 para las esquinas
	int m_resolution = 32;

	// Iso level in "effective particles" units: 1.0 = one particle's peak.
	// Values >1.0 mean a single particle never generates a surface; multiple
	// overlapping particles (compact puddle) produce the sheet.
	float m_isoLevel		= 1.05f;
	float m_densityScale	= 2000.0f;	///< Density normalized to uint for f
	float m_particleRadius	= 0.25f;	///< Radius of influence for the metaball.
	float m_fallSpeedCutoff = -1.0f;	///< vy < this => no splat (falling drop).

	float m_splatHeight	   = 0.15f;	 ///< Relative to boundsMin.y.
	float m_splatMaxHeight = 0.22f;	 ///< Absolute ceiling (never splat above).
	float m_settleSpeed	   = 0.5f;	 ///< |vy| <= this => settled.
	float m_splatYScale	   = 2.5f;	 ///< >1 compresses Y and flattens the iso.
	float m_waterEnergy	   = 0.0f;	 ///< Mean squared particle speed; gates the micro-waves.
	float m_waterLevel	   = 0.0f;	 ///< World-space height used to flatten the settled mesh.

	float m_fillScale	   = 1.0f;

	GLuint m_maxParticles  = 0;		 ///< Current capacity of the particle SSBO.
	GLuint m_maxVertices   = 0;		 ///< Capacity of the vertex SSBO (grid.x*grid.y*grid.z*15).
	GLuint m_particleCount = 0;		 ///< Valid particles currently in the SSBO.
	bool   m_initialized   = false;	 ///< Flag indicating if the pipeline is ready.

	GLuint m_externalParticleSSBO  = 0;	///< External particle SSBO (zero-copy handoff), 0 = unused.
	GLuint m_externalParticleCount = 0;	///< Valid particles in the external SSBO.
	bool   m_useExternalParticles  = false;	///< True while an external SSBO is bound.
};
