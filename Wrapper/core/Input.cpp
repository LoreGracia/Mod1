/**
 * @file Input.cpp
 * @brief Implementation of the Input system and GLFW conversions.
 */
#include <Input.hpp>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace
{
/**
 * @brief Helper function to validate if a GLFW key code is within known ranges.
 * @param key The GLFW key code to validate.
 * @return True if the key is recognized, false otherwise.
 */
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
}  // namespace

/**
 * @brief Converts a GLFW integer key code to the internal Key::Code enum.
 * @param key The GLFW key code.
 * @return The corresponding Key::Code, or Code::Unknown if invalid.
 */
Key::Code Key::fromGlfw(int key)
{
	if (key == GLFW_KEY_UNKNOWN || !isKnownKey(key))
		return Code::Unknown;
	return static_cast<Code>(key);
}

/**
 * @brief Converts a GLFW integer mouse button code to the internal Mouse::Button enum.
 * @param button The GLFW mouse button code.
 * @return The corresponding Mouse::Button, or Button::Unknown if invalid.
 */
Mouse::Button Mouse::fromGlfw(int button)
{
	if (button < GLFW_MOUSE_BUTTON_1 || button > GLFW_MOUSE_BUTTON_LAST)
		return Button::Unknown;
	return static_cast<Button>(button);
}

/**
 * @brief Prepares the input state for a new frame.
 * Clears transient states (pressed/released) and resets per-frame deltas.
 */
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

/**
 * @brief Finalizes the current frame's input state.
 * Currently an alias for beginFrame(), ensuring states are cleared for the next cycle.
 */
void Input::endFrame()
{
	beginFrame();
}

/**
 * @brief Resets all input states, clearing held keys, buttons, and mouse data.
 * Typically called when the window loses focus.
 */
void Input::reset()
{
	m_keysDown.clear();
	m_keysPressed.clear();
	m_keysReleased.clear();
	m_buttonsDown.clear();
	m_buttonsPressed.clear();
	m_buttonsReleased.clear();
	m_mouseX		 = 0.0;
	m_mouseY		 = 0.0;
	m_lastMouseX	 = 0.0;
	m_lastMouseY	 = 0.0;
	m_deltaX		 = 0.0;
	m_deltaY		 = 0.0;
	m_scrollDeltaX	 = 0.0;
	m_scrollDeltaY	 = 0.0;
	m_firstMouseMove = true;
}

/**
 * @brief Processes a key down event.
 * Only registers the 'pressed' state if the key wasn't already down,
 * preventing OS key-repeat from triggering multiple press events.
 * @param key The key that was pressed.
 */
void Input::onKeyDown(Key::Code key)
{
	if (key == Key::Code::Unknown)
		return;
	if (m_keysDown.insert(key).second)
		m_keysPressed.insert(key);
}

/**
 * @brief Processes a key up event.
 * @param key The key that was released.
 */
void Input::onKeyUp(Key::Code key)
{
	if (key == Key::Code::Unknown)
		return;
	if (m_keysDown.erase(key))
		m_keysReleased.insert(key);
}

/**
 * @brief Processes a mouse button down event.
 * @param button The button that was pressed.
 */
void Input::onMouseButtonDown(Mouse::Button button)
{
	if (button == Mouse::Button::Unknown)
		return;
	if (m_buttonsDown.insert(button).second)
		m_buttonsPressed.insert(button);
}

/**
 * @brief Processes a mouse button up event.
 * @param button The button that was released.
 */
void Input::onMouseButtonUp(Mouse::Button button)
{
	if (button == Mouse::Button::Unknown)
		return;
	if (m_buttonsDown.erase(button))
		m_buttonsReleased.insert(button);
}

/**
 * @brief Processes a mouse cursor movement event.
 * Handles the initial jump when the window gains focus, and accumulates
 * the movement delta for the current frame.
 * @param x The new X position of the cursor.
 * @param y The new Y position of the cursor.
 */
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
	m_mouseX	 = x;
	m_mouseY	 = y;
	m_lastMouseX = x;
	m_lastMouseY = y;
}

/**
 * @brief Processes a mouse scroll event, accumulating offsets for the frame.
 * @param xoffset The scroll offset on the X axis.
 * @param yoffset The scroll offset on the Y axis.
 */
void Input::onScroll(double xoffset, double yoffset)
{
	m_scrollDeltaX += xoffset;
	m_scrollDeltaY += yoffset;
}

/**
 * @brief Updates the window focus state.
 * Resets inputs on focus loss to prevent "stuck" keys, and flags
 * the next mouse move to prevent a sudden camera jump.
 * @param focused True if the window gained focus, false otherwise.
 */
void Input::onFocus(bool focused)
{
	if (!focused) {
		reset();
		return;
	}
	m_firstMouseMove = true;
}

/**
 * @brief Checks if a specific key is currently held down.
 * @param key The key to check.
 * @return True if held down.
 */
bool Input::isKeyDown(Key::Code key) const
{
	return m_keysDown.count(key) > 0;
}

/**
 * @brief Checks if a specific key was pressed during this frame.
 * @param key The key to check.
 * @return True if pressed this frame.
 */
bool Input::isKeyPressed(Key::Code key) const
{
	return m_keysPressed.count(key) > 0;
}

/**
 * @brief Checks if a specific key was released during this frame.
 * @param key The key to check.
 * @return True if released this frame.
 */
bool Input::isKeyReleased(Key::Code key) const
{
	return m_keysReleased.count(key) > 0;
}

/**
 * @brief Checks if a mouse button is currently held down.
 * @param button The mouse button to check.
 * @return True if held down.
 */
bool Input::isMouseButtonDown(Mouse::Button button) const
{
	return m_buttonsDown.count(button) > 0;
}

/**
 * @brief Checks if a mouse button was pressed during this frame.
 * @param button The mouse button to check.
 * @return True if pressed this frame.
 */
bool Input::isMouseButtonPressed(Mouse::Button button) const
{
	return m_buttonsPressed.count(button) > 0;
}

/**
 * @brief Checks if a mouse button was released during this frame.
 * @param button The mouse button to check.
 * @return True if released this frame.
 */
bool Input::isMouseButtonReleased(Mouse::Button button) const
{
	return m_buttonsReleased.count(button) > 0;
}
