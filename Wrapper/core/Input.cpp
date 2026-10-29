#include "core/Input.hpp"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace
{
bool isKnownKey(int key)
{
	return (key >= GLFW_KEY_SPACE && key <= GLFW_KEY_GRAVE_ACCENT) ||
		   (key >= GLFW_KEY_WORLD_1 && key <= GLFW_KEY_WORLD_2) ||
		   (key >= GLFW_KEY_ESCAPE && key <= GLFW_KEY_END) ||
		   (key >= GLFW_KEY_CAPS_LOCK && key <= GLFW_KEY_PAUSE) ||
		   (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) ||
		   (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL) ||
		   (key >= GLFW_KEY_LEFT_SHIFT && key <= GLFW_KEY_MENU);
}
} // namespace

Key::Code Key::fromGlfw(int key)
{
	if (key == GLFW_KEY_UNKNOWN || !isKnownKey(key))
		return Code::Unknown;
	return static_cast<Code>(key);
}

Mouse::Button Mouse::fromGlfw(int button)
{
	if (button < GLFW_MOUSE_BUTTON_1 || button > GLFW_MOUSE_BUTTON_LAST)
		return Button::Unknown;
	return static_cast<Button>(button);
}

void Input::beginFrame()
{
	m_keysPressed.clear();
	m_keysReleased.clear();
	m_buttonsPressed.clear();
	m_buttonsReleased.clear();
	m_deltaX	   = 0.0;
	m_deltaY	   = 0.0;
	m_scrollDeltaX = 0.0;
	m_scrollDeltaY = 0.0;
}

void Input::endFrame()
{
	beginFrame();
}

void Input::reset()
{
	m_keysDown.clear();
	m_keysPressed.clear();
	m_keysReleased.clear();
	m_buttonsDown.clear();
	m_buttonsPressed.clear();
	m_buttonsReleased.clear();
	m_mouseX		= 0.0;
	m_mouseY		= 0.0;
	m_lastMouseX	= 0.0;
	m_lastMouseY	= 0.0;
	m_deltaX		= 0.0;
	m_deltaY		= 0.0;
	m_scrollDeltaX	= 0.0;
	m_scrollDeltaY	= 0.0;
	m_firstMouseMove = true;
}

void Input::onKeyDown(Key::Code key)
{
	if (key == Key::Code::Unknown)
		return;
	if (m_keysDown.insert(key).second)
		m_keysPressed.insert(key);
}

void Input::onKeyUp(Key::Code key)
{
	if (key == Key::Code::Unknown)
		return;
	if (m_keysDown.erase(key))
		m_keysReleased.insert(key);
}

void Input::onMouseButtonDown(Mouse::Button button)
{
	if (button == Mouse::Button::Unknown)
		return;
	if (m_buttonsDown.insert(button).second)
		m_buttonsPressed.insert(button);
}

void Input::onMouseButtonUp(Mouse::Button button)
{
	if (button == Mouse::Button::Unknown)
		return;
	if (m_buttonsDown.erase(button))
		m_buttonsReleased.insert(button);
}

void Input::onMouseMove(double x, double y)
{
	if (m_firstMouseMove) {
		m_lastMouseX	 = x;
		m_lastMouseY	 = y;
		m_mouseX		 = x;
		m_mouseY		 = y;
		m_firstMouseMove = false;
		return;
	}

	m_deltaX += x - m_lastMouseX;
	m_deltaY += y - m_lastMouseY;
	m_mouseX = x;
	m_mouseY = y;
	m_lastMouseX = x;
	m_lastMouseY = y;
}

void Input::onScroll(double xoffset, double yoffset)
{
	m_scrollDeltaX += xoffset;
	m_scrollDeltaY += yoffset;
}

void Input::onFocus(bool focused)
{
	if (!focused) {
		reset();
		return;
	}
	m_firstMouseMove = true;
}

bool Input::isKeyDown(Key::Code key) const
{
	return m_keysDown.count(key) > 0;
}

bool Input::isKeyPressed(Key::Code key) const
{
	return m_keysPressed.count(key) > 0;
}

bool Input::isKeyReleased(Key::Code key) const
{
	return m_keysReleased.count(key) > 0;
}

bool Input::isMouseButtonDown(Mouse::Button button) const
{
	return m_buttonsDown.count(button) > 0;
}

bool Input::isMouseButtonPressed(Mouse::Button button) const
{
	return m_buttonsPressed.count(button) > 0;
}

bool Input::isMouseButtonReleased(Mouse::Button button) const
{
	return m_buttonsReleased.count(button) > 0;
}
