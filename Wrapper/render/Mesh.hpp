// inc/render/Mesh.hpp
#pragma once
#include <glad/gl.h>

#include <cstdint>
#include <vector>

class Mesh
{
public:
	Mesh() = default;
	~Mesh();
	Mesh(Mesh&&) noexcept;
	Mesh& operator=(Mesh&&) noexcept;
	Mesh(const Mesh&)			 = delete;
	Mesh& operator=(const Mesh&) = delete;

	void upload(const std::vector<float>& vertices, const std::vector<uint32_t>& indices,
				GLenum mode = GL_TRIANGLES);
	void upload(const std::vector<float>& positions, const std::vector<float>& colors,
				const std::vector<uint32_t>& indices, GLenum mode = GL_TRIANGLES);
	void draw() const;

	void uploadPoints(const std::vector<float>& positions, const std::vector<float>& densities = {},
					  const std::vector<float>& velocities = {});
	void updatePoints(const std::vector<float>& positions, const std::vector<float>& densities = {},
					  const std::vector<float>& velocities = {});
	void drawPoints() const;
	// Dibuja solo los primeros `count` puntos del buffer actual.
	void drawPoints(GLsizei count) const;

	static Mesh triangle();
	static Mesh cube();
	static Mesh plane(float size = 1.f);

private:
	void	release();
	GLuint	m_vao = 0, m_vbo = 0, m_ebo = 0;
	GLsizei m_indexCount  = 0;
	GLenum	m_mode		  = GL_TRIANGLES;
	GLuint	m_vbo2		  = 0;	// densidades (attrib 1)
	GLuint	m_vbo3		  = 0;	// velocidades (attrib 2, Fase 4)
	GLsizei m_vertexCount = 0;
};
