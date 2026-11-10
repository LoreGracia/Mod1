/**
 * @file Renderer.cpp
 * @brief Implementation of the Renderer class.
 * Contains optimized OpenGL state management functions that cache state
 * to avoid redundant driver calls.
 */

#include <Renderer.hpp>

/**
 * @brief Sets the viewport dimensions.
 * Caches the last known viewport size to avoid redundant GL calls.
 * @param w The width of the viewport.
 * @param h The height of the viewport.
 */
void Renderer::setViewport(int w, int h)
{
	if (w == m_vpW && h == m_vpH)
		return;
	m_vpW = w;
	m_vpH = h;

	glViewport(0, 0, w, h);
}

/**
 * @brief Sets the clear color.
 * Caches the last known color to avoid redundant GL calls.
 * @param r Red component (0.0 to 1.0).
 * @param g Green component (0.0 to 1.0).
 * @param b Blue component (0.0 to 1.0).
 * @param a Alpha component (0.0 to 1.0).
 */
void Renderer::setClearColor(float r, float g, float b, float a)
{
	if (m_clearColorKnown && r == m_clearR && g == m_clearG && b == m_clearB && a == m_clearA)
		return;
	m_clearR		  = r;
	m_clearG		  = g;
	m_clearB		  = b;
	m_clearA		  = a;
	m_clearColorKnown = true;

	glClearColor(r, g, b, a);
}

/**
 * @brief Clears the specified buffers.
 * @param color True to clear the color buffer.
 * @param depth True to clear the depth buffer.
 * @param stencil True to clear the stencil buffer.
 */
void Renderer::clear(bool color, bool depth, bool stencil)
{
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

/**
 * @brief Enables or disables depth testing (GL_DEPTH_TEST).
 * @param on True to enable, false to disable.
 */
void Renderer::depthTest(bool on)
{
	if (on == m_depthTest)
		return;
	m_depthTest = on;
	on ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
}

/**
 * @brief Enables or disables writing to the depth buffer.
 * @param on True to enable writing, false to disable (read-only).
 */
void Renderer::depthWrite(bool on)
{
	if (on == m_depthWrite)
		return;
	m_depthWrite = on;

	glDepthMask(on ? GL_TRUE : GL_FALSE);
}

/**
 * @brief Sets the depth comparison function.
 * @param func The OpenGL depth function (e.g., GL_LESS, GL_LEQUAL).
 */
void Renderer::depthFunc(GLenum func)
{
	if (func == m_depthFunc)
		return;
	m_depthFunc = func;

	glDepthFunc(func);
}

/**
 * @brief Enables or disables face culling (GL_CULL_FACE).
 * @param on True to enable, false to disable.
 */
void Renderer::cullFace(bool on)
{
	if (on == m_cullFace)
		return;
	m_cullFace = on;

	on ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
}

/**
 * @brief Sets which faces to cull.
 * @param mode GL_FRONT, GL_BACK, or GL_FRONT_AND_BACK.
 */
void Renderer::cullMode(GLenum mode)
{
	if (mode == m_cullMode)
		return;
	m_cullMode = mode;

	glCullFace(mode);
}

/**
 * @brief Sets the front-facing polygon winding order.
 * @param winding GL_CW (clockwise) or GL_CCW (counter-clockwise).
 */
void Renderer::frontFace(GLenum winding)
{
	if (winding == m_frontFace)
		return;
	m_frontFace = winding;

	glFrontFace(winding);
}

/**
 * @brief Enables or disables blending (GL_BLEND).
 * @param on True to enable, false to disable.
 */
void Renderer::blend(bool on)
{
	if (on == m_blend)
		return;
	m_blend = on;

	on ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
}

/**
 * @brief Sets the blending function factors.
 * @param src Source blend factor (e.g., GL_SRC_ALPHA).
 * @param dst Destination blend factor (e.g., GL_ONE_MINUS_SRC_ALPHA).
 */
void Renderer::blendFunc(GLenum src, GLenum dst)
{
	if (src == m_blendSrc && dst == m_blendDst)
		return;
	m_blendSrc = src;
	m_blendDst = dst;

	glBlendFunc(src, dst);
}

/**
 * @brief Forces a resynchronization of the internal state cache with the OpenGL context.
 *
 * This is useful when external code (or another library) has modified the OpenGL state
 * directly, bypassing this wrapper. It inverts the cached states and then calls the
 * setter methods with the correct intended values, forcing the GL calls to execute.
 */
void Renderer::syncToDefaults()
{
	m_vpW = m_vpH	  = -1;
	m_clearColorKnown = false;
	m_depthTest		  = !m_depthTest;

	depthTest(!m_depthTest);
	m_cullFace = !m_cullFace;

	cullFace(!m_cullFace);
	m_blend = !m_blend;

	blend(!m_blend);
}
