#pragma once
#include <Mesh.hpp>
#include <Particle.hpp>
#include <functional>
#include <glm.hpp>
#include <unordered_map>
#include <vector>

class Shader;

/**
 * @file Particles.hpp
 * @brief Defines the Particles class, which handles the CPU-based SPH simulation
 * and manages the data flow to the GPU for hybrid rendering.
 */

/**
 * @class Particles
 * @brief Manages the SPH (Smoothed Particle Hydrodynamics) fluid simulation.
 *
 * This class handles particle physics (density, pressure, viscosity, gravity, XSPH),
 * spatial hashing for efficient neighbor searches, boundary collisions, and
 * terrain interaction. It also classifies particles for hybrid rendering
 * (puddle vs. airborne rain drops) and provides the data to the GPU rendering pipeline.
 */
class Particles
{
public:
	/**
	 * @enum Mode
	 * @brief Defines the different behaviors for the fluid simulation.
	 */
	enum class Mode
	{
		UniformRise,  ///< Water rises uniformly from the bottom.
		Wave,		  ///< Water is pushed horizontally to create a wave.
		Rain		  ///< Continuous rain drops falling from the top.
	};

	/**
	 * @struct Config
	 * @brief Configuration parameters for the SPH simulation and rendering.
	 */
	struct Config
	{
		float h			   = 0.12f;	  ///< Smoothing radius (kernel support).
		float restDensity  = 400.f;	  ///< Rest density of the fluid.
		float stiffness	   = 150.f;	  ///< Gas constant for pressure calculation.
		float viscosity	   = 0.08f;	  ///< Viscosity coefficient.
		float gravity	   = -9.8f;	  ///< Gravity acceleration (Y-axis).
		float damping	   = 0.999f;  ///< Velocity damping factor per step.
		int	  substeps	   = 4;		  ///< Number of physics substeps per frame.
		float particleMass = 0.2f;	  ///< Mass of a single particle.

		glm::vec3 boundsMin{-1.f, 0.f, -1.f};  ///< Minimum bounds of the container.
		glm::vec3 boundsMax{1.f, 3.f, 1.f};	   ///< Maximum bounds of the container.
		float	  wallRestitution = 0.3f;	   ///< Bounciness of the walls (0 = no bounce).

		float	  pointSize = 8.f;				 ///< Base size of the points.
		glm::vec3 colorLow{0.2f, 0.6f, 1.0f};	 ///< Color for low-density particles.
		glm::vec3 colorHigh{0.05f, 0.1f, 0.5f};	 ///< Color for high-density particles.
		float	  alpha = 0.7f;					 ///< Alpha opacity for points.

		float rainDropFraction = 0.15f;	 ///< Fraction of particles acting as rain drops.
		float rainSettleSpeed  = 2.0f;	 ///< Speed threshold to consider a drop settled (puddle).
		float rainAirBand =
			0.25f;	///< Height band from the top where drops are considered airborne.
		float	  rainPointSize = 6.0f;			  ///< Point size for rain drops.
		float	  rainAlpha		= 0.75f;		  ///< Alpha for rain drops.
		glm::vec3 rainColor{0.55f, 0.80f, 1.0f};  ///< Color for rain drops.
		float	  rainHideMargin  = 0.06f;		  ///< Margin above puddle surface to hide drops.
		float	  rainSplashSpeed = 0.6f;		  ///< Minimum speed to show a drop as a splash.
	};

	/** @brief Default constructor. */
	Particles();

	/** @brief Default destructor. */
	~Particles();

	/**
	 * @brief Initializes the simulation using the current configuration.
	 * @param count Number of particles to spawn.
	 * @param mode The initial simulation mode.
	 */
	void init(size_t count, Mode mode);

	/**
	 * @brief Initializes the simulation with a specific configuration.
	 * @param count Number of particles to spawn.
	 * @param mode The initial simulation mode.
	 * @param cfg The configuration to use.
	 */
	void init(size_t count, Mode mode, const Config& cfg);

	/**
	 * @brief Updates the simulation (SPH physics, collisions, buffer packing).
	 * @param dt Delta time in seconds.
	 */
	void update(float dt);

	/** @brief Uploads the current particle positions and densities to the GPU. */
	void uploadToGPU();

	/**
	 * @brief Renders all particles using the provided shader.
	 * @param shader The shader to use.
	 * @param viewProj The combined view-projection matrix.
	 */
	void render(Shader& shader, const glm::mat4& viewProj);

	/**
	 * @brief Renders only the airborne rain drops.
	 * @param shader The shader to use.
	 * @param viewProj The combined view-projection matrix.
	 */
	void renderRainDrops(Shader& shader, const glm::mat4& viewProj);

	/** @brief Gets a const reference to the raw particle data (for GPU upload). */
	const std::vector<Particle>& getParticles() const { return m_particles; }

	/**
	 * @brief Resets the simulation to its initial state.
	 * @param mode The mode to reset to.
	 */
	void reset(Mode mode);

	/** @brief Sets a new configuration for the simulation. */
	void setConfig(const Config& cfg) { m_cfg = cfg; }

	/** @brief Typedef for the terrain height callback function. */
	using HeightFn = std::function<float(float x, float z)>;

	/** @brief Sets a callback function to sample terrain height for collisions. */
	void setTerrainHeight(HeightFn fn) { m_heightFn = std::move(fn); }

	/** @brief Gets the total number of particles in the simulation. */
	size_t count() const { return m_particles.size(); }

private:
	/**
	 * @brief Performs a single SPH physics step (density, pressure, forces).
	 * @param dt Delta time for the substep.
	 */
	void stepSPH(float dt);

	/**
	 * @brief Samples the SPH density at an arbitrary point in space.
	 * @param point The world-space point to sample.
	 * @return The density at that point.
	 */
	float sampleDensity(const glm::vec3& point) const;

	/**
	 * @brief Integrates the particle positions using semi-implicit Euler.
	 * @param dt Delta time for the substep.
	 */
	void integrate(float dt);

	/** @brief Handles collisions with the boundaries and terrain. */
	void collide();

	/**
	 * @brief Computes the grid cell for a given position.
	 * @param p The position.
	 * @return The 3D cell index.
	 */
	glm::ivec3 cellOf(const glm::vec3& p) const;

	/**
	 * @brief Computes a unique hash for a grid cell.
	 * @param c The 3D cell index.
	 * @return The hash value.
	 */
	int64_t hashCell(const glm::ivec3& c) const;

	/** @brief Builds the spatial hash map for the current particle positions. */
	void buildHash();

	/**
	 * @brief Executes a callback for each neighbor of a particle.
	 * @tparam Fn Callback type.
	 * @param i Index of the particle.
	 * @param fn The callback function to execute for each neighbor index.
	 */
	template <typename Fn> void forEachNeighbor(size_t i, Fn&& fn) const;

	/**
	 * @brief Spawns all particles based on the specified mode.
	 * @param mode The simulation mode.
	 */
	void spawnAll(Mode mode);

	/**
	 * @brief Spawns or resets a single particle.
	 * @param p The particle to initialize.
	 * @param mode The current simulation mode.
	 * @param initial True if this is the initial spawn, false if it's a recycle.
	 */
	void spawnOne(Particle& p, Mode mode, bool initial);

	/** @brief Recycles settled rain drops back to the top. */
	void recycleRain();

	Config				  m_cfg;					   ///< Current simulation configuration.
	Mode				  m_mode = Mode::UniformRise;  ///< Current simulation mode.
	std::vector<Particle> m_particles;				   ///< CPU array of all particles.

	std::vector<float> m_positions;	 ///< Flattened array of positions (for Mesh).
	std::vector<float> m_densities;	 ///< Flattened array of densities (for Mesh).

	std::vector<float> m_fallPositions;	  ///< Flattened array of airborne drop positions.
	std::vector<float> m_fallDensities;	  ///< Flattened array of airborne drop densities.
	std::vector<float> m_fallVelocities;  ///< Flattened array of airborne drop velocities.

	float  m_puddleTopY = 0.f;	///< Highest Y of settled water (for hiding drops).
	size_t m_fallCount	= 0;	///< Number of airborne drops currently active.

	Mesh m_mesh;	  ///< Mesh/VBO for rendering all particles.
	Mesh m_fallMesh;  ///< Mesh/VBO for rendering airborne rain drops.

	HeightFn m_heightFn;		  ///< Optional terrain height callback for collisions.
	int		 m_debugCounter = 0;  ///< Frame counter for debug printing.

	std::unordered_map<int64_t, std::vector<size_t>>
		m_hash;	 ///< Spatial hash map for neighbor search.
};
