#pragma once
#include <glad/gl.h>

class Renderer {
public:
	void setViewport(int w, int h);

	void setClearColor(float r, float g, float b, float a = 1.f);
	void clear(bool color = true, bool depth = true, bool stencil = false);

	void depthTest(bool on);
	void depthWrite(bool on);
	void depthFunc(GLenum func);

	void cullFace(bool on);
	void cullMode(GLenum mode);
	void frontFace(GLenum winding);

	void blend(bool on);
	void blendFunc(GLenum src, GLenum dst);

	void syncToDefaults();

private:
	int    m_vpW = -1, m_vpH = -1;
	float  m_clearR = 0.f, m_clearG = 0.f,
		m_clearB = 0.f, m_clearA = 1.f;
	bool   m_clearColorKnown = false;

	bool   m_depthTest       = false;
	bool   m_depthWrite      = true;
	GLenum m_depthFunc       = GL_LESS;

	bool   m_cullFace        = false;
	GLenum m_cullMode        = GL_BACK;
	GLenum m_frontFace       = GL_CCW;

	bool   m_blend           = false;
	GLenum m_blendSrc        = GL_ONE;
	GLenum m_blendDst        = GL_ZERO;
};
