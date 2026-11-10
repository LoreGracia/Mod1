#pragma once
#include <glad/gl.h>

/**
 * @file Renderer.hpp
 * @brief Defines the Renderer class, an optimized OpenGL state management wrapper.
 */

/**
 * @class Renderer
 * @brief Manages OpenGL render states (viewport, clear color, depth, culling, blending)
 * while caching state changes to minimize redundant GL driver calls.
 *
 * OpenGL state changes can be expensive. This class wraps common state
 * modifications (like glEnable/glDisable or glViewport) and only forwards
 * the call to the GPU if the requested state is different from the currently cached state.
 */
class Renderer
{
public:
	/**
	 * @brief Sets the viewport dimensions and caches them.
	 * @param w The width of the viewport in pixels.
	 * @param h The height of the viewport in pixels.
	 */
	void setViewport(int w, int h);

	/**
	 * @brief Sets the clear color and caches it.
	 * @param r Red component (0.0 to 1.0).
	 * @param g Green component (0.0 to 1.0).
	 * @param b Blue component (0.0 to 1.0).
	 * @param a Alpha component (0.0 to 1.0). Defaults to 1.0 (opaque).
	 */
	void setClearColor(float r, float g, float b, float a = 1.f);

	/**
	 * @brief Clears the specified buffers.
	 * @param color True to clear the color buffer. Defaults to true.
	 * @param depth True to clear the depth buffer. Defaults to true.
	 * @param stencil True to clear the stencil buffer. Defaults to false.
	 */
	void clear(bool color = true, bool depth = true, bool stencil = false);

	/**
	 * @brief Enables or disables depth testing (GL_DEPTH_TEST).
	 * @param on True to enable, false to disable.
	 */
	void depthTest(bool on);

	/**
	 * @brief Enables or disables writing to the depth buffer.
	 * @param on True to enable writing, false to make the depth buffer read-only.
	 */
	void depthWrite(bool on);

	/**
	 * @brief Sets the depth comparison function.
	 * @param func The OpenGL depth function (e.g., GL_LESS, GL_LEQUAL).
	 */
	void depthFunc(GLenum func);

	/**
	 * @brief Enables or disables face culling (GL_CULL_FACE).
	 * @param on True to enable, false to disable.
	 */
	void cullFace(bool on);

	/**
	 * @brief Sets which faces to cull.
	 * @param mode GL_FRONT, GL_BACK, or GL_FRONT_AND_BACK.
	 */
	void cullMode(GLenum mode);

	/**
	 * @brief Sets the front-facing polygon winding order.
	 * @param winding GL_CW (clockwise) or GL_CCW (counter-clockwise).
	 */
	void frontFace(GLenum winding);

	/**
	 * @brief Enables or disables blending (GL_BLEND).
	 * @param on True to enable, false to disable.
	 */
	void blend(bool on);

	/**
	 * @brief Sets the blending function factors.
	 * @param src Source blend factor (e.g., GL_SRC_ALPHA).
	 * @param dst Destination blend factor (e.g., GL_ONE_MINUS_SRC_ALPHA).
	 */
	void blendFunc(GLenum src, GLenum dst);

	/**
	 * @brief Forces a resynchronization of the internal state cache with the OpenGL context.
	 *
	 * Useful when external code modifies OpenGL state directly, bypassing this wrapper.
	 * It invalidates the cached states, forcing the next setter calls to execute.
	 */
	void syncToDefaults();

private:
	int	  m_vpW = -1, m_vpH = -1;  ///< Cached viewport dimensions.
	float m_clearR = 0.f, m_clearG = 0.f, m_clearB = 0.f, m_clearA = 1.f;  ///< Cached clear color.
	bool  m_clearColorKnown =
		false;	///< Flag indicating if the clear color has been set at least once.

	bool   m_depthTest	= false;	///< Cached depth test state.
	bool   m_depthWrite = true;		///< Cached depth write mask state.
	GLenum m_depthFunc	= GL_LESS;	///< Cached depth comparison function.

	bool   m_cullFace  = false;	   ///< Cached face culling state.
	GLenum m_cullMode  = GL_BACK;  ///< Cached culling mode.
	GLenum m_frontFace = GL_CCW;   ///< Cached front face winding order.

	bool   m_blend	  = false;	  ///< Cached blending state.
	GLenum m_blendSrc = GL_ONE;	  ///< Cached source blend factor.
	GLenum m_blendDst = GL_ZERO;  ///< Cached destination blend factor.
};
