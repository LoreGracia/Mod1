#pragma once
#include <glad/gl.h>

#include <cstdint>
#include <vector>

/**
 * @file Mesh.hpp
 * @brief Defines the Mesh class, an abstraction for OpenGL Vertex Array Objects (VAO),
 * Vertex Buffer Objects (VBO), and Element Buffer Objects (EBO).
 */

/**
 * @class Mesh
 * @brief Manages OpenGL geometry data, including vertex, color, density, and velocity buffers.
 *
 * This class provides an RAII wrapper around OpenGL buffers. It supports both indexed
 * mesh rendering (triangles) and point-cloud rendering (used for SPH particles).
 * It handles the creation, uploading, and drawing of VAOs, VBOs, and EBOs.
 */
class Mesh
{
public:
	/** @brief Default constructor. */
	Mesh() = default;

	/** @brief Destructor. Releases all allocated OpenGL buffers. */
	~Mesh();

	/** @brief Move constructor. Transfers ownership of GL resources. */
	Mesh(Mesh&&) noexcept;

	/** @brief Move assignment operator. Transfers ownership of GL resources. */
	Mesh& operator=(Mesh&&) noexcept;

	/** @brief Copy constructor is deleted. GL resources are unique. */
	Mesh(const Mesh&) = delete;

	/** @brief Copy assignment operator is deleted. GL resources are unique. */
	Mesh& operator=(const Mesh&) = delete;

	/**
	 * @brief Uploads indexed geometry data (positions only) to the GPU.
	 * @param vertices Flat array of vertex positions (x, y, z).
	 * @param indices Array of indices for the triangles/lines.
	 * @param mode The primitive type to draw (e.g., GL_TRIANGLES, GL_LINES). Defaults to
	 * GL_TRIANGLES.
	 */
	void upload(const std::vector<float>& vertices, const std::vector<uint32_t>& indices,
				GLenum mode = GL_TRIANGLES);

	/**
	 * @brief Uploads indexed geometry data with per-vertex colors to the GPU.
	 * @param positions Flat array of vertex positions (x, y, z).
	 * @param colors Flat array of vertex colors (r, g, b).
	 * @param indices Array of indices.
	 * @param mode The primitive type to draw. Defaults to GL_TRIANGLES.
	 */
	void upload(const std::vector<float>& positions, const std::vector<float>& colors,
				const std::vector<uint32_t>& indices, GLenum mode = GL_TRIANGLES);

	/** @brief Draws the indexed geometry using the currently bound shader. */
	void draw() const;

	/**
	 * @brief Uploads point cloud data to the GPU.
	 * @param positions Flat array of point positions (x, y, z).
	 * @param densities Optional array of per-point density values (attrib 1).
	 * @param velocities Optional array of per-point velocity vectors (attrib 2).
	 */
	void uploadPoints(const std::vector<float>& positions, const std::vector<float>& densities = {},
					  const std::vector<float>& velocities = {});

	/**
	 * @brief Updates existing point cloud buffers without reallocating them.
	 * Useful for per-frame updates of particle systems.
	 * @param positions Flat array of point positions.
	 * @param densities Optional array of density values.
	 * @param velocities Optional array of velocity vectors.
	 */
	void updatePoints(const std::vector<float>& positions, const std::vector<float>& densities = {},
					  const std::vector<float>& velocities = {});

	/** @brief Draws all points in the point cloud buffer. */
	void drawPoints() const;

	/**
	 * @brief Draws a specific number of points from the point cloud buffer.
	 * @param count The number of points to render.
	 */
	void drawPoints(GLsizei count) const;

	/** @brief Creates a static Mesh containing a single unit triangle. */
	static Mesh triangle();

	/** @brief Creates a static Mesh containing a unit cube. */
	static Mesh cube();

	/**
	 * @brief Creates a static Mesh containing a flat plane on the XZ axis.
	 * @param size The total width and depth of the plane. Defaults to 1.0.
	 */
	static Mesh plane(float size = 1.f);

private:
	/** @brief Internal helper to release all OpenGL resources. */
	void release();

	GLuint	m_vao = 0, m_vbo = 0, m_ebo = 0;  ///< OpenGL handles for the VAO, VBO, and EBO.
	GLsizei m_indexCount  = 0;				  ///< Number of indices in the EBO.
	GLenum	m_mode		  = GL_TRIANGLES;	  //< Primitive mode used for drawing.
	GLuint	m_vbo2		  = 0;	///< Secondary VBO handle (used for densities, attrib 1).
	GLuint	m_vbo3		  = 0;	///< Tertiary VBO handle (used for velocities, attrib 2).
	GLsizei m_vertexCount = 0;	///< Number of vertices in the point cloud buffer.
};
