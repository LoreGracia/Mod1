#include "render/Renderer.hpp"

void Renderer::setViewport(int w, int h) {
	if (w == m_vpW && h == m_vpH)
		return;
	m_vpW = w; m_vpH = h;
	glViewport(0, 0, w, h);
}

void Renderer::setClearColor(float r, float g, float b, float a) {
	if (m_clearColorKnown &&
		r == m_clearR && g == m_clearG && b == m_clearB && a == m_clearA)
		return;
	m_clearR = r; m_clearG = g; m_clearB = b; m_clearA = a;
	m_clearColorKnown = true;
	glClearColor(r, g, b, a);
}

void Renderer::clear(bool color, bool depth, bool stencil) {
	GLbitfield mask = 0;

	if (color)
		mask |= GL_COLOR_BUFFER_BIT;
	if (depth)
		mask |= GL_DEPTH_BUFFER_BIT;
	if (stencil)
		mask |= GL_STENCIL_BUFFER_BIT;
	if (mask)
		glClear(mask);
}

void Renderer::depthTest(bool on) {
	if (on == m_depthTest)
		return;
	m_depthTest = on;
	on ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
}

void Renderer::depthWrite(bool on) {
	if (on == m_depthWrite)
		return;
	m_depthWrite = on;
	glDepthMask(on ? GL_TRUE : GL_FALSE);
}

void Renderer::depthFunc(GLenum func) {
	if (func == m_depthFunc)
		return;
	m_depthFunc = func;
	glDepthFunc(func);
}

void Renderer::cullFace(bool on) {
	if (on == m_cullFace)
		return;
	m_cullFace = on;
	on ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
}

void Renderer::cullMode(GLenum mode) {
	if (mode == m_cullMode)
		return;
	m_cullMode = mode;
	glCullFace(mode);
}

void Renderer::frontFace(GLenum winding) {
	if (winding == m_frontFace)
		return;
	m_frontFace = winding;
	glFrontFace(winding);
}

void Renderer::blend(bool on) {
	if (on == m_blend) return;
	m_blend = on;
	on ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
}

void Renderer::blendFunc(GLenum src, GLenum dst) {
	if (src == m_blendSrc && dst == m_blendDst) return;
	m_blendSrc = src; m_blendDst = dst;
	glBlendFunc(src, dst);
}

void Renderer::syncToDefaults() {
	m_vpW = m_vpH = -1;
	m_clearColorKnown = false;
	m_depthTest  = !m_depthTest;

	depthTest(!m_depthTest);
	m_cullFace   = !m_cullFace;

	cullFace(!m_cullFace);
	m_blend      = !m_blend;

	blend(!m_blend);
}