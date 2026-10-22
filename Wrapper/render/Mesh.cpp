// src/render/Mesh.cpp
#include <glad/gl.h>
#include "render/Mesh.hpp"

Mesh::~Mesh() { release(); }

Mesh::Mesh(Mesh &&o) noexcept
	: m_vao(o.m_vao), m_vbo(o.m_vbo), m_ebo(o.m_ebo),
	m_indexCount(o.m_indexCount), m_mode(o.m_mode)
{ o.m_vao = o.m_vbo = o.m_ebo = 0; o.m_indexCount = 0; }

Mesh& Mesh::operator=(Mesh&& o) noexcept {
	if (this != &o) {
		release();
		m_vao = o.m_vao; m_vbo = o.m_vbo; m_ebo = o.m_ebo;
		m_indexCount = o.m_indexCount; m_mode = o.m_mode;
		o.m_vao = o.m_vbo = o.m_ebo = 0; o.m_indexCount = 0;
	}
	return *this;
}

void Mesh::release() {
	if (m_ebo)
		glDeleteBuffers(1, &m_ebo);
	if (m_vbo)
		glDeleteBuffers(1, &m_vbo);
	if (m_vao)
		glDeleteVertexArrays(1, &m_vao);
	m_vao = m_vbo = m_ebo = 0; m_indexCount = 0;
}

void Mesh::upload(const std::vector<float>& v,
				const std::vector<uint32_t>& idx,
				GLenum mode) {
	release();
	m_mode = mode;
	m_indexCount = (GLsizei)idx.size();

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

void Mesh::draw() const {
	glBindVertexArray(m_vao);
	glDrawElements(m_mode, m_indexCount, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}

Mesh Mesh::triangle() {
	Mesh m;
	m.upload({-0.5f,-0.5f,0.f,  0.5f,-0.5f,0.f,  0.f,0.5f,0.f}, {0,1,2});
	return m;
}

Mesh Mesh::cube() {
	Mesh m;
	m.upload({
		-0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,0.5f,-0.5f,  -0.5f,0.5f,-0.5f,
		-0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f,0.5f, 0.5f,  -0.5f,0.5f, 0.5f
	}, {
		0,1,2, 2,3,0,   4,5,6, 6,7,4,
		0,4,7, 7,3,0,   1,5,6, 6,2,1,
		3,7,6, 6,2,3,   0,1,5, 5,4,0
	});
	return m;
}

Mesh Mesh::plane(float s) {
	Mesh m;
	float h = s * 0.5f;
	m.upload({-h,0,-h,  h,0,-h,  h,0,h,  -h,0,h}, {0,1,2, 2,3,0});
	return m;
}
