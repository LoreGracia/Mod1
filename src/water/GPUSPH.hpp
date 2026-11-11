#pragma once
#include <glad/gl.h>

#include <Particle.hpp>
#include <Shader.hpp>
#include <glm.hpp>
#include <string>
#include <vector>

/**
 * @file GPUSPH.hpp
 * @brief Defines the GPUSPH class, which runs the SPH physics entirely on the
 * GPU through compute shaders.
 */

/**
 * @class GPUSPH
 * @brief GPU compute pipeline for the SPH fluid simulation.
 *
 * Owns the GPU-side particle state (SoA layout spread over several SSBOs) and
 * dispatches the compute passes of the solver: Clear & Predict, GPU spatial
 * hash, Density & Pressure, Forces & XSPH and Integration (collisions and
 * rain recycling included).
 * The particle SSBO uses the same GPUParticle layout as the Marching Cubes
 * splat pass (2 x vec4), so it can be shared with GPUMarchingCubes without
 * CPU round-trips once the migration is complete.
 *
 * @note This class is non-copyable due to its ownership of unique GPU resources
 * and Shader objects. Use std::shared_ptr or std::unique_ptr if needed.
 */
class GPUSPH
{
public:
	/**
	 * @struct Params
	 * @brief Physics and grid parameters consumed by the compute passes.
	 *
	 * Mirrors the physics subset of Particles::Config so the CPU presets can
	 * be transferred verbatim when the migration is wired in main.cpp.
	 */
	struct Params
	{
		float h = 0.12f;  ///< Smoothing radius (kernel support). Also the base grid cell size.
		float restDensity	  = 400.f;	 ///< Rest density of the fluid.
		float stiffness		  = 150.f;	 ///< Gas constant for the pressure calculation.
		float viscosity		  = 0.08f;	 ///< Viscosity coefficient.
		float xsph			  = 0.10f;	 ///< XSPH velocity smoothing factor.
		float gravity		  = -9.8f;	 ///< Gravity acceleration (Y axis).
		float damping		  = 0.999f;	 ///< Velocity damping factor per substep.
		float particleMass	  = 0.2f;	 ///< Mass of a single particle.
		int	  substeps		  = 4;		 ///< Number of physics substeps per frame.
		float maxSpeed		  = 10.f;	 ///< Velocity clamp for stability.
		float wallRestitution = 0.3f;	 ///< Bounciness of the container walls.

		bool  rainMode = false;	 ///< Enables the rain airborne classification for splatWeight.
		float rainSettleSpeed = 2.0f;  ///< Speed above which a particle counts as airborne.
		float rainAirBand = 0.25f;	///< Height band from the top where particles count as airborne.

		glm::vec3 boundsMin{-1.f, 0.f, -1.f};  ///< Minimum bounds of the container.
		glm::vec3 boundsMax{1.f, 3.f, 1.f};	   ///< Maximum bounds of the container.
	};

	/** @brief Default constructor. */
	GPUSPH();

	/** @brief Destructor. Calls release() to free GPU resources. */
	~GPUSPH();

	// Shader is non-copyable; GPUSPH is also non-copyable.
	GPUSPH(const GPUSPH&)			 = delete;
	GPUSPH& operator=(const GPUSPH&) = delete;

	/**
	 * @brief Initializes the pipeline: loads the compute shaders, sizes the
	 * uniform grid and allocates all SSBOs.
	 *
	 * @param maxParticles Capacity of the per-particle SSBOs (the pipeline is
	 * sized once; uploadParticles() rejects larger inputs).
	 * @param params Physics and grid parameters.
	 * @param clearPredictPath Path to the Clear & Predict shader (sph_clear_predict.comp).
	 * @param hashPath Path to the spatial hashing shader (sph_hash.comp).
	 * @param densityPath Path to the density & pressure shader (sph_density.comp).
	 * @param forcesPath Path to the forces & XSPH shader (sph_forces.comp).
	 * @param integratePath Path to the integration shader (sph_integrate.comp).
	 * @return True if the shaders compiled and the buffers were created successfully.
	 */
	bool init(size_t maxParticles, const Params& params, const std::string& clearPredictPath,
			  const std::string& hashPath, const std::string& densityPath,
			  const std::string& forcesPath, const std::string& integratePath);

	/**
	 * @brief Uploads the initial particle state (CPU -> GPU seeding).
	 *
	 * Packs position + splatWeight and velocity + age into the particle SSBO.
	 * Called once at startup/reset; afterwards the GPU state is authoritative
	 * and no per-frame upload is needed.
	 *
	 * @param particles CPU particle data used to seed the GPU state.
	 */
	void uploadParticles(const std::vector<Particle>& particles);

	/**
	 * @brief Runs one frame of the SPH pipeline on the GPU.
	 *
	 * Each substep dispatches the full chain: Clear & Predict, the six
	 * counting-sort phases of the spatial hash, Density & Pressure, Forces &
	 * XSPH and Integration (collisions + rain recycling + classification).
	 *
	 * @param dt Delta time in seconds.
	 */
	void update(float dt);

	/**
	 * @brief Sets new physics/grid parameters.
	 *
	 * Rebuilds the uniform grid and reallocates the cell-sized SSBOs when the
	 * smoothing radius or the bounds change.
	 *
	 * @param params The parameters to adopt.
	 */
	void setParams(const Params& params);

	/** @brief Gets a const reference to the current parameters. */
	const Params& params() const { return m_params; }

	/**
	 * @brief Gets the particle SSBO handle (GPUParticle layout, std430).
	 *
	 * Intended for the zero-copy handoff to GPUMarchingCubes
	 * (density_splat.comp can consume this exact buffer).
	 */
	GLuint particleBuffer() const { return m_particleSSBO; }

	/** @brief Gets the number of valid particles in the SSBOs. */
	size_t particleCount() const { return m_particleCount; }

	/** @brief Gets the number of grid cells of the spatial hash. */
	int numCells() const { return m_numCells; }

	/**
	 * @brief Validates the spatial hash with a synchronous CPU readback.
	 *
	 * Reads back cellCount / cellStart / cellCursor / sortedIdx / cellIds /
	 * predicted and checks that:
	 *   1. sum(cellCount) == particleCount,
	 *   2. cellStart is a valid exclusive prefix (ranges are contiguous),
	 *   3. sortedIdx is a permutation of [0, particleCount),
	 *   4. every sorted entry belongs to the cell range it was scattered to,
	 *   5. cellIds matches the cell recomputed from the predicted positions.
	 *
	 * @note This introduces a CPU-GPU stall: NEVER call it in the render loop.
	 * @return True if all checks pass.
	 */
	bool debugValidateSort() const;

	/**
	 * @brief Reads back the density/pressure buffer (debug/validate only).
	 *
	 * Retrieves DensPressBuffer as (density, pressure, splatWeight, 0) per
	 * particle. Used by the offline validation harness; the render loop must
	 * never stall on this.
	 *
	 * @note This introduces a CPU-GPU stall: NEVER call it in the render loop.
	 * @param out Destination vector; resized to the particle count.
	 */
	void debugReadbackDensity(std::vector<glm::vec4>& out) const;

	/**
	 * @brief Reads back the force and XSPH buffers (debug/validate only).
	 *
	 * Retrieves ForceBuffer (gravity + pressure + viscosity, xyz) and
	 * XsphVelBuffer (the velocity delta to apply when integrating, xyz).
	 * Used by the offline validation harness; the render loop must never
	 * stall on this.
	 *
	 * @note This introduces a CPU-GPU stall: NEVER call it in the render loop.
	 * @param outForce Destination for the forces; resized to the particle count.
	 * @param outXsph Destination for the XSPH deltas; resized to the particle count.
	 */
	void debugReadbackForces(std::vector<glm::vec4>& outForce,
							 std::vector<glm::vec4>& outXsph) const;

	/**
	 * @brief Reads back the particle state (debug/validate only).
	 *
	 * Retrieves the particle SSBO split into its three vec4 planes:
	 * position + splatWeight, velocity + age and flags (x = rainDrop).
	 * Used by the offline validation harness; the render loop must never
	 * stall on this.
	 *
	 * @note This introduces a CPU-GPU stall: NEVER call it in the render loop.
	 * @param outPosWeight Destination for positions + splatWeight.
	 * @param outVelAge Destination for velocities + age.
	 * @param outFlags Destination for the flags plane.
	 */
	void debugReadbackParticles(std::vector<glm::vec4>& outPosWeight,
								 std::vector<glm::vec4>& outVelAge,
								 std::vector<glm::vec4>& outFlags) const;

	/**
	 * @brief Releases all OpenGL resources.
	 * @note Idempotent. Called from the destructor; can be called manually.
	 */
	void release();

private:
	/** @brief SSBO binding points shared by all SPH compute passes. */
	enum Binding : GLuint
	{
		BINDING_PARTICLES = 0,	///< ParticleBuffer: GPUParticle[] (posWeight, velAge).
		BINDING_DENS_PRESS =
			1,	///< DensPressBuffer: vec4[] (density, pressure, splatWeight, unused).
		BINDING_FORCES		= 2,  ///< ForceBuffer: vec4[] (force.xyz, unused).
		BINDING_POS_PRED	= 3,  ///< PosPredBuffer: vec4[] (predicted position, unused).
		BINDING_CELL_IDS	= 4,  ///< CellIdBuffer: uint[] cell index of each particle.
		BINDING_SORTED_IDX	= 5,  ///< SortedIndexBuffer: uint[] particles grouped by cell.
		BINDING_CELL_COUNT	= 6,  ///< CellCountBuffer: uint[] particles per cell.
		BINDING_CELL_START	= 7,  ///< CellStartBuffer: uint[] exclusive prefix (range start).
		BINDING_CELL_CURSOR = 8,  ///< CellCursorBuffer: uint[] scatter write cursors.
		BINDING_BLOCK_SUMS	= 9,  ///< BlockSumsBuffer: uint[] scan segment totals/offsets.
		BINDING_XSPH_VEL =
			10	///< XsphVelBuffer: vec4[] XSPH velocity delta (applied when integrating).
	};

	/** @brief Creates and allocates all SSBOs for the current capacity/grid. */
	void createBuffers();

	/** @brief (Re)builds the uniform grid dimensions from bounds and cell size. */
	void rebuildGrid();

	/**
	 * @brief Dispatches Clear & Predict and the six counting-sort phases.
	 *
	 * Binds the SSBOs, sets the shared uniforms and issues the dispatch
	 * sequence of sph_clear_predict.comp + sph_hash.comp with a memory
	 * barrier between every phase.
	 *
	 * @param subDt Substep delta time passed to the prediction.
	 */
	void buildSpatialHash(float subDt);

	/**
	 * @brief Dispatches the Density & Pressure pass (sph_density.comp).
	 *
	 * Binds the SSBOs, sets the pass uniforms and issues the dispatch with a
	 * trailing memory barrier, since the following passes read the results.
	 */
	void computeDensityPressure();

	/**
	 * @brief Dispatches the Forces & XSPH pass (sph_forces.comp).
	 *
	 * Binds the SSBOs, sets the pass uniforms and issues the dispatch with a
	 * trailing memory barrier. Adds the pressure (Spiky) and viscosity
	 * (Laplacian) terms on top of the gravity in ForceBuffer and stores the
	 * XSPH velocity delta in XsphVelBuffer.
	 */
	void computeForces();

	/**
	 * @brief Dispatches the Integration pass (sph_integrate.comp).
	 *
	 * Binds the SSBOs, sets the pass uniforms and issues the dispatch with a
	 * trailing memory barrier. Applies the forces and the XSPH delta to the
	 * velocities, moves the particles, resolves the boundary collisions and
	 * the rain recycling, and classifies splatWeight.
	 *
	 * @param subDt Substep delta time applied to the integration.
	 */
	void integrateParticles(float subDt);

	Params m_params;  ///< Current physics and grid parameters.

	GLuint m_particleSSBO	= 0;  ///< GPUParticle[] state (binding 0).
	GLuint m_densPressSSBO	= 0;  ///< vec4[] density/pressure/splatWeight (binding 1).
	GLuint m_forceSSBO		= 0;  ///< vec4[] forces (binding 2).
	GLuint m_posPredSSBO	= 0;  ///< vec4[] predicted positions (binding 3).
	GLuint m_cellIdSSBO		= 0;  ///< uint[] cell of each particle (binding 4).
	GLuint m_sortedIdxSSBO	= 0;  ///< uint[] sorted particle indices (binding 5).
	GLuint m_cellCountSSBO	= 0;  ///< uint[] per-cell counts (binding 6).
	GLuint m_cellStartSSBO	= 0;  ///< uint[] per-cell range start (binding 7).
	GLuint m_cellCursorSSBO = 0;  ///< uint[] scatter cursors (binding 8).
	GLuint m_blockSumsSSBO	= 0;  ///< uint[] scan block sums (binding 9).
	GLuint m_xsphVelSSBO	= 0;  ///< vec4[] XSPH velocity delta (binding 10).

	Shader m_clearPredictShader;  ///< sph_clear_predict.comp.
	Shader m_hashShader;		  ///< sph_hash.comp (phases multiplexed via uPhase).
	Shader m_densityShader;			///< sph_density.comp (Poly6 density + equation of state).
	Shader m_forcesShader;			///< sph_forces.comp (Spiky + Laplacian forces + XSPH delta).
	Shader m_integrateShader;		///< sph_integrate.comp (Euler integration + collisions + recycling).

	glm::ivec3 m_gridSize{0};	   ///< Grid cells per axis.
	int		   m_numCells  = 0;	   ///< Total number of grid cells (gx*gy*gz).
	int		   m_numBlocks = 0;	   ///< Scan workgroups covering the grid (ceil(numCells/1024)).
	float	   m_cellSize  = 0.f;  ///< Effective cell edge length (>= h, see rebuildGrid).

	size_t m_maxParticles	= 0;		///< Capacity of the per-particle SSBOs.
	size_t m_particleCount	= 0;		///< Valid particles in the SSBOs.
	unsigned m_recycleSeed = 0;	///< Seed of the respawn RNG, advanced every substep.
	bool   m_initialized	= false;	///< Flag indicating if the pipeline is ready.
};
