/**
 * @file Mesh.cpp
 * @brief Implementation of the Mesh class, handling OpenGL buffer creation,
 * data uploading, and drawing commands.
 */

#include <glad/gl.h>

#include <Mesh.hpp>

/**
 * @brief Destructor. Ensures all OpenGL resources are released.
 */
Mesh::~Mesh()
{
	release();
}

/**
 * @brief Move constructor. Transfers ownership of OpenGL handles from another Mesh.
 * @param o The Mesh to move from.
 */
Mesh::Mesh(Mesh&& o) noexcept
	: m_vao(o.m_vao), m_vbo(o.m_vbo), m_ebo(o.m_ebo), m_indexCount(o.m_indexCount),
	  m_mode(o.m_mode), m_vbo2(o.m_vbo2), m_vbo3(o.m_vbo3), m_vertexCount(o.m_vertexCount)
{
	o.m_vao = o.m_vbo = o.m_ebo = o.m_vbo2 = o.m_vbo3 = 0;
	o.m_indexCount									  = 0;
	o.m_vertexCount									  = 0;
}

/**
 * @brief Move assignment operator. Releases current resources and transfers
 * ownership from another Mesh.
 * @param o The Mesh to move from.
 * @return A reference to this Mesh.
 */
Mesh& Mesh::operator=(Mesh&& o) noexcept
{
	if (this != &o) {
		release();
		m_vao		  = o.m_vao;
		m_vbo		  = o.m_vbo;
		m_ebo		  = o.m_ebo;
		m_vbo2		  = o.m_vbo2;
		m_vbo3		  = o.m_vbo3;
		m_indexCount  = o.m_indexCount;
		m_vertexCount = o.m_vertexCount;
		m_mode		  = o.m_mode;

		o.m_vao = o.m_vbo = o.m_ebo = o.m_vbo2 = o.m_vbo3 = 0;
		o.m_indexCount									  = 0;
		o.m_vertexCount									  = 0;
	}
	return *this;
}

/**
 * @brief Releases all allocated OpenGL buffers and resets member variables.
 */
void Mesh::release()
{
	if (m_ebo)
		glDeleteBuffers(1, &m_ebo);
	if (m_vbo)
		glDeleteBuffers(1, &m_vbo);
	if (m_vao)
		glDeleteVertexArrays(1, &m_vao);
	if (m_vbo2)
		glDeleteBuffers(1, &m_vbo2);
	if (m_vbo3)
		glDeleteBuffers(1, &m_vbo3);
	m_vao = m_vbo = m_ebo = 0;
	m_vbo2 = m_vbo3 = 0;
	m_indexCount	= 0;
	m_vertexCount	= 0;
}

/**
 * @brief Uploads indexed geometry data (positions only) to the GPU.
 * @param v Flat array of vertex positions (x, y, z).
 * @param idx Array of indices for the triangles.
 * @param mode The primitive type to draw (e.g., GL_TRIANGLES).
 */
void Mesh::upload(const std::vector<float>& v, const std::vector<uint32_t>& idx, GLenum mode)
{
	release();
	m_mode		 = mode;
	m_indexCount = (GLsizei)idx.size();

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(),
				 GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

/**
 * @brief Uploads indexed geometry data with per-vertex colors to the GPU.
 * @note The positions and colors arrays must have the same number of vertices.
 * @param positions Flat array of vertex positions (x, y, z).
 * @param colors Flat array of vertex colors (r, g, b).
 * @param indices Array of indices.
 * @param mode The primitive type to draw.
 */
void Mesh::upload(const std::vector<float>& positions, const std::vector<float>& colors,
				  const std::vector<uint32_t>& indices, GLenum mode)
{
	release();
	m_mode		 = mode;
	m_indexCount = (GLsizei)indices.size();

	const std::size_t  vertexCount = positions.size() / 3;
	std::vector<float> interleaved;
	interleaved.reserve(vertexCount * 6);

	// Interleave positions and colors into a single buffer for cache efficiency.
	for (std::size_t i = 0; i < vertexCount; ++i) {
		interleaved.push_back(positions[i * 3 + 0]);
		interleaved.push_back(positions[i * 3 + 1]);
		interleaved.push_back(positions[i * 3 + 2]);
		interleaved.push_back(colors[i * 3 + 0]);
		interleaved.push_back(colors[i * 3 + 1]);
		interleaved.push_back(colors[i * 3 + 2]);
	}

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, interleaved.size() * sizeof(float), interleaved.data(),
				 GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(),
				 GL_STATIC_DRAW);

	const GLsizei stride = 6 * sizeof(float);

	// Attribute 0: Position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(0);

	// Attribute 1: Color
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

/**
 * @brief Draws the indexed geometry using the currently bound shader.
 */
void Mesh::draw() const
{
	glBindVertexArray(m_vao);
	glDrawElements(m_mode, m_indexCount, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}

/**
 * @brief Uploads point cloud data to the GPU for point rendering.
 * @param positions Flat array of point positions (x, y, z).
 * @param densities Optional array of density values. Mapped to attribute 1.
 * @param velocities Optional array of velocity vectors. Mapped to attribute 2.
 */
void Mesh::uploadPoints(const std::vector<float>& positions, const std::vector<float>& densities,
						const std::vector<float>& velocities)
{
	release();
	m_mode		  = GL_POINTS;
	m_vertexCount = static_cast<GLsizei>(positions.size() / 3);

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(),
				 GL_DYNAMIC_DRAW);

	// Attribute 0: Position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	if (!densities.empty()) {
		glGenBuffers(1, &m_vbo2);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo2);
		glBufferData(GL_ARRAY_BUFFER, densities.size() * sizeof(float), densities.data(),
					 GL_DYNAMIC_DRAW);

		// Attribute 1: Density
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
	}

	if (!velocities.empty()) {
		glGenBuffers(1, &m_vbo3);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo3);
		glBufferData(GL_ARRAY_BUFFER, velocities.size() * sizeof(float), velocities.data(),
					 GL_DYNAMIC_DRAW);
		// Attribute 2: Velocity
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(2);
	}

	glBindVertexArray(0);
}

/**
 * @brief Updates existing point cloud buffers with new data.
 * Assumes the buffers were already allocated with uploadPoints().
 * @param positions New flat array of point positions.
 * @param densities Optional new array of density values.
 * @param velocities Optional new array of velocity vectors.
 */
void Mesh::updatePoints(const std::vector<float>& positions, const std::vector<float>& densities,
						const std::vector<float>& velocities)
{
	if (!m_vbo)
		return;

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(float), positions.data());

	if (m_vbo2 && !densities.empty()) {
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo2);
		glBufferSubData(GL_ARRAY_BUFFER, 0, densities.size() * sizeof(float), densities.data());
	}

	if (m_vbo3 && !velocities.empty()) {
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo3);
		glBufferSubData(GL_ARRAY_BUFFER, 0, velocities.size() * sizeof(float), velocities.data());
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
 * @brief Draws all points in the point cloud buffer.
 */
void Mesh::drawPoints() const
{
	if (!m_vao || m_vertexCount == 0)
		return;
	glBindVertexArray(m_vao);
	glDrawArrays(GL_POINTS, 0, m_vertexCount);
	glBindVertexArray(0);
}

/**
 * @brief Draws a specific number of points from the point cloud buffer.
 * @param count The number of points to render. Clamped to m_vertexCount.
 */
void Mesh::drawPoints(GLsizei count) const
{
	if (!m_vao || count <= 0 || m_vertexCount == 0)
		return;
	if (count > m_vertexCount)
		count = m_vertexCount;
	glBindVertexArray(m_vao);
	glDrawArrays(GL_POINTS, 0, count);
	glBindVertexArray(0);
}

/**
 * @brief Creates a static Mesh containing a cube geometry.
 * @note The method name suggests a triangle, but the vertex and index data
 * actually build a 3D cube (8 vertices, 36 indices).
 * @return The generated Mesh.
 */
Mesh Mesh::triangle()
{
	Mesh m;

	m.upload({-0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
			  -0.5f, -0.5f, 0.5f,  0.5f, -0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  -0.5f, 0.5f, 0.5f},
			 {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 0, 4, 7, 7, 3, 0,
			  1, 5, 6, 6, 2, 1, 3, 7, 6, 6, 2, 3, 0, 1, 5, 5, 4, 0});
	return m;
}

/**
 * @brief Creates a static Mesh containing a flat plane on the XZ axis.
 * @param s The total width and depth of the plane.
 * @return The generated Mesh.
 */
Mesh Mesh::plane(float s)
{
	Mesh  m;
	float h = s * 0.5f;

	m.upload({-h, 0, -h, h, 0, -h, h, 0, h, -h, 0, h}, {0, 1, 2, 2, 3, 0});
	return m;
}
