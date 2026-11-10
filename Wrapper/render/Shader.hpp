#pragma once
#include <glad/gl.h>

#include <string>
#include <type_ptr.hpp>
#include <unordered_map>

/**
 * @file Shader.hpp
 * @brief Defines the Shader class, a wrapper for compiling, linking, and managing
 * OpenGL shader programs (Graphics and Compute).
 */

/**
 * @class Shader
 * @brief Manages OpenGL shader programs.
 *
 * This class handles the compilation of vertex, fragment, and compute shaders,
 * links them into programs, and provides helper methods to set uniforms and
 * dispatch compute workloads. It also caches uniform locations to reduce
 * expensive queries to the GPU driver.
 */
class Shader
{
public:
	/** @brief Default constructor. */
	Shader() = default;

	/** @brief Destructor. Releases the OpenGL program if it exists. */
	~Shader();

	/** @brief Move constructor. Transfers ownership of the GL program and uniform cache. */
	Shader(Shader&&) noexcept;

	/** @brief Move assignment operator. Transfers ownership of the GL program and uniform cache. */
	Shader& operator=(Shader&&) noexcept;

	/** @brief Copy constructor is deleted. GL programs are unique. */
	Shader(const Shader&) = delete;

	/** @brief Copy assignment operator is deleted. GL programs are unique. */
	Shader& operator=(const Shader&) = delete;

	/**
	 * @brief Compiles and links a vertex and fragment shader from source strings.
	 * @param vertSrc Vertex shader source code.
	 * @param fragSrc Fragment shader source code.
	 * @return True if successful, false otherwise.
	 */
	bool loadFromSource(const std::string& vertSrc, const std::string& fragSrc);

	/**
	 * @brief Compiles and links a vertex and fragment shader from files.
	 * @param vertPath Path to the vertex shader file.
	 * @param fragPath Path to the fragment shader file.
	 * @return True if successful, false otherwise.
	 */
	bool loadFromFile(const std::string& vertPath, const std::string& fragPath);


	// ── Compute Shaders ─────────────────────────────────────────────
	/**
	 * @brief Compiles and links a compute shader from a source string.
	 * @param computeSrc Compute shader source code.
	 * @return True if successful, false otherwise.
	 */
	bool loadComputeFromSource(const std::string& computeSrc);

	/**
	 * @brief Compiles and links a compute shader from a file.
	 * @param computePath Path to the compute shader file.
	 * @return True if successful, false otherwise.
	 */
	bool loadComputeFromFile(const std::string& computePath);

	/**
	 * @brief Dispatches a compute shader.
	 *
	 * Internally binds the program and calls glDispatchCompute.
	 * @note The caller MUST call glMemoryBarrier(...) afterwards, specifying
	 * which buffers will be read, as the shader does not know this.
	 *
	 * @param groupsX Number of work groups on the X axis.
	 * @param groupsY Number of work groups on the Y axis.
	 * @param groupsZ Number of work groups on the Z axis.
	 */
	void dispatch(GLuint groupsX, GLuint groupsY, GLuint groupsZ) const;

	/**
	 * @brief Helper to dispatch a compute shader over a grid.
	 *
	 * Calculates the necessary number of work groups to cover a grid
	 * (gridX, gridY, gridZ) assuming a local_size defined in the .comp file.
	 *
	 * @param gridX Total grid size X.
	 * @param gridY Total grid size Y.
	 * @param gridZ Total grid size Z.
	 * @param localX Local workgroup size X. Defaults to 4.
	 * @param localY Local workgroup size Y. Defaults to 4.
	 * @param localZ Local workgroup size Z. Defaults to 4.
	 */
	void dispatchForGrid(GLuint gridX, GLuint gridY, GLuint gridZ, GLuint localX = 4,
						 GLuint localY = 4, GLuint localZ = 4) const;

	/** @brief Binds this shader program for rendering or compute. */
	void use() const { glUseProgram(m_program); }

	/** @brief Gets the raw OpenGL program ID. */
	GLuint id() const { return m_program; }

	/** @brief Sets an integer uniform variable. */
	void setInt(const char* name, int v) const;

	/** @brief Sets a float uniform variable. */
	void setFloat(const char* name, float v) const;

	/** @brief Sets a vec3 uniform variable. */
	void setVec3(const char* name, const glm::vec3& v) const;

	/** @brief Sets a vec4 uniform variable. */
	void setVec4(const char* name, const glm::vec4& v) const;

	/** @brief Sets a mat4 uniform variable. */
	void setMat4(const char* name, const glm::mat4& m) const;

	/** @brief Sets an ivec3 uniform variable. */
	void setIVec3(const char* name, const glm::ivec3& v) const;

private:
	/**
	 * @brief Retrieves the location of a uniform variable, utilizing an internal cache.
	 * @param name The name of the uniform.
	 * @return The location (GLint), or -1 if not found.
	 */
	GLint location(const char* name) const;

	/**
	 * @brief Compiles a single shader stage from source code.
	 * @param type The shader type (GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER).
	 * @param src The source code.
	 * @return The compiled shader handle, or 0 on failure.
	 */
	GLuint compile(GLenum type, const std::string& src);

	GLuint m_program = 0;  ///< The OpenGL program handle.
	mutable std::unordered_map<std::string, GLint>
		m_uniformCache;	 ///< Cache for uniform locations to avoid redundant driver queries.
};
