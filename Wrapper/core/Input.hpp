#pragma once
#include <unordered_set>

/**
 * @file Input.hpp
 * @brief Defines the Input system, mapping GLFW keys/buttons and managing
 * per-frame input states (down, pressed, released).
 */

/**
 * @namespace Key
 * @brief Contains keyboard-related enums and conversion functions.
 */
namespace Key
{
/**
 * @enum Code
 * @brief Platform-independent key codes.
 * Values are mapped directly to GLFW key codes for easy conversion.
 */
enum class Code : int
{
	Unknown		   = -1,
	Space		   = 32,
	Apostrophe	   = 39,
	Comma		   = 44,
	Minus		   = 45,
	Period		   = 46,
	Slash		   = 47,
	Num0		   = 48,
	Num1		   = 49,
	Num2		   = 50,
	Num3		   = 51,
	Num4		   = 52,
	Num5		   = 53,
	Num6		   = 54,
	Num7		   = 55,
	Num8		   = 56,
	Num9		   = 57,
	Semicolon	   = 59,
	Equal		   = 61,
	A			   = 65,
	B			   = 66,
	C			   = 67,
	D			   = 68,
	E			   = 69,
	F			   = 70,
	G			   = 71,
	H			   = 72,
	I			   = 73,
	J			   = 74,
	K			   = 75,
	L			   = 76,
	M			   = 77,
	N			   = 78,
	O			   = 79,
	P			   = 80,
	Q			   = 81,
	R			   = 82,
	S			   = 83,
	T			   = 84,
	U			   = 85,
	V			   = 86,
	W			   = 87,
	X			   = 88,
	Y			   = 89,
	Z			   = 90,
	LeftBracket	   = 91,
	Backslash	   = 92,
	RightBracket   = 93,
	GraveAccent	   = 96,
	World1		   = 161,
	World2		   = 162,
	Escape		   = 256,
	Enter		   = 257,
	Tab			   = 258,
	Backspace	   = 259,
	Insert		   = 260,
	Delete		   = 261,
	Right		   = 262,
	Left		   = 263,
	Down		   = 264,
	Up			   = 265,
	PageUp		   = 266,
	PageDown	   = 267,
	Home		   = 268,
	End			   = 269,
	CapsLock	   = 280,
	ScrollLock	   = 281,
	NumLock		   = 282,
	PrintScreen	   = 283,
	Pause		   = 284,
	F1			   = 290,
	F2			   = 291,
	F3			   = 292,
	F4			   = 293,
	F5			   = 294,
	F6			   = 295,
	F7			   = 296,
	F8			   = 297,
	F9			   = 298,
	F10			   = 299,
	F11			   = 300,
	F12			   = 301,
	F13			   = 302,
	F14			   = 303,
	F15			   = 304,
	F16			   = 305,
	F17			   = 306,
	F18			   = 307,
	F19			   = 308,
	F20			   = 309,
	F21			   = 310,
	F22			   = 311,
	F23			   = 312,
	F24			   = 313,
	F25			   = 314,
	Keypad0		   = 320,
	Keypad1		   = 321,
	Keypad2		   = 322,
	Keypad3		   = 323,
	Keypad4		   = 324,
	Keypad5		   = 325,
	Keypad6		   = 326,
	Keypad7		   = 327,
	Keypad8		   = 328,
	Keypad9		   = 329,
	KeypadDecimal  = 330,
	KeypadDivide   = 331,
	KeypadMultiply = 332,
	KeypadSubtract = 333,
	KeypadAdd	   = 334,
	KeypadEnter	   = 335,
	KeypadEqual	   = 336,
	LeftShift	   = 340,
	LeftControl	   = 341,
	LeftAlt		   = 342,
	LeftSuper	   = 343,
	RightShift	   = 344,
	RightControl   = 345,
	RightAlt	   = 346,
	RightSuper	   = 347,
	Menu		   = 348,
};

/**
 * @brief Converts a GLFW integer key code to the internal Key::Code enum.
 * @param key The GLFW key code.
 * @return The corresponding Key::Code.
 */
Code fromGlfw(int key);
}  // namespace Key

/**
 * @namespace Mouse
 * @brief Contains mouse-related enums and conversion functions.
 */
namespace Mouse
{
/**
 * @enum Button
 * @brief Platform-independent mouse button codes.
 */
enum class Button : int
{
	Unknown = -1,
	Left	= 0,
	Right	= 1,
	Middle	= 2,
	X1		= 3,
	X2		= 4,
	Button6 = 5,
	Button7 = 6,
	Button8 = 7,
};

/**
 * @brief Converts a GLFW integer mouse button code to the internal Mouse::Button enum.
 * @param button The GLFW mouse button code.
 * @return The corresponding Mouse::Button.
 */
Button fromGlfw(int button);
}  // namespace Mouse

/**
 * @class Input
 * @brief Manages the input state for a single frame.
 *
 * Tracks keyboard and mouse states, including whether a key/button is currently
 * held down, was just pressed, or was just released in the current frame.
 */
class Input
{
public:
	/** @brief Default constructor. */
	Input() = default;

	/**
	 * @brief Called at the start of a frame to prepare input states.
	 * Clears transient states (pressed/released) from the previous frame.
	 */
	void beginFrame();

	/**
	 * @brief Called at the end of a frame.
	 */
	void endFrame();

	/** @brief Resets all input states and clears tracked keys/buttons. */
	void reset();

	/**
	 * @brief Processes a key down event.
	 * @param key The key that was pressed.
	 */
	void onKeyDown(Key::Code key);

	/**
	 * @brief Processes a key up event.
	 * @param key The key that was released.
	 */
	void onKeyUp(Key::Code key);

	/**
	 * @brief Processes a mouse button down event.
	 * @param button The button that was pressed.
	 */
	void onMouseButtonDown(Mouse::Button button);

	/**
	 * @brief Processes a mouse button up event.
	 * @param button The button that was released.
	 */
	void onMouseButtonUp(Mouse::Button button);

	/**
	 * @brief Processes a mouse cursor movement event.
	 * @param x The new X position of the cursor.
	 * @param y The new Y position of the cursor.
	 */
	void onMouseMove(double x, double y);

	/**
	 * @brief Processes a mouse scroll event.
	 * @param xoffset The scroll offset on the X axis.
	 * @param yoffset The scroll offset on the Y axis.
	 */
	void onScroll(double xoffset, double yoffset);

	/**
	 * @brief Updates the window focus state.
	 * @param focused True if the window gained focus, false otherwise.
	 */
	void onFocus(bool focused);

	/**
	 * @brief Checks if a specific key is currently held down.
	 * @param key The key to check.
	 * @return True if held down.
	 */
	bool isKeyDown(Key::Code key) const;

	/**
	 * @brief Checks if a specific key was pressed during this frame.
	 * @param key The key to check.
	 * @return True if pressed this frame.
	 */
	bool isKeyPressed(Key::Code key) const;

	/**
	 * @brief Checks if a specific key was released during this frame.
	 * @param key The key to check.
	 * @return True if released this frame.
	 */
	bool isKeyReleased(Key::Code key) const;

	/**
	 * @brief Checks if a mouse button is currently held down.
	 * @param button The mouse button to check.
	 * @return True if held down.
	 */
	bool isMouseButtonDown(Mouse::Button button) const;

	/**
	 * @brief Checks if a mouse button was pressed during this frame.
	 * @param button The mouse button to check.
	 * @return True if pressed this frame.
	 */
	bool isMouseButtonPressed(Mouse::Button button) const;

	/**
	 * @brief Checks if a mouse button was released during this frame.
	 * @param button The mouse button to check.
	 * @return True if released this frame.
	 */
	bool isMouseButtonReleased(Mouse::Button button) const;

	/** @brief Gets the current X position of the mouse cursor. */
	double mouseX() const { return m_mouseX; }

	/** @brief Gets the current Y position of the mouse cursor. */
	double mouseY() const { return m_mouseY; }

	/** @brief Gets the mouse movement delta on the X axis since the last frame. */
	double mouseDeltaX() const { return m_deltaX; }

	/** @brief Gets the mouse movement delta on the Y axis since the last frame. */
	double mouseDeltaY() const { return m_deltaY; }

	/** @brief Gets the total scroll delta for the current frame. */
	double scrollDelta() const { return m_scrollDeltaY; }

	/** @brief Gets the scroll delta on the X axis. */
	double scrollDeltaX() const { return m_scrollDeltaX; }

	/** @brief Gets the scroll delta on the Y axis. */
	double scrollDeltaY() const { return m_scrollDeltaY; }

private:
	std::unordered_set<Key::Code> m_keysDown;	   ///< Keys currently held down.
	std::unordered_set<Key::Code> m_keysPressed;   ///< Keys pressed in the current frame.
	std::unordered_set<Key::Code> m_keysReleased;  ///< Keys released in the current frame.

	std::unordered_set<Mouse::Button> m_buttonsDown;  ///< Mouse buttons currently held down.
	std::unordered_set<Mouse::Button>
		m_buttonsPressed;  ///< Mouse buttons pressed in the current frame.
	std::unordered_set<Mouse::Button>
		m_buttonsReleased;	///< Mouse buttons released in the current frame

	double m_mouseX		  = 0.0;   ///< Current mouse X position.
	double m_mouseY		  = 0.0;   ///< Current mouse Y position.
	double m_lastMouseX	  = 0.0;   ///< Mouse X position in the previous frame.
	double m_lastMouseY	  = 0.0;   ///< Mouse Y position in the previous frame.
	double m_deltaX		  = 0.0;   ///< Mouse delta X for the current frame.
	double m_deltaY		  = 0.0;   ///< Mouse delta Y for the current frame.
	double m_scrollDeltaX = 0.0;   ///< Scroll delta X for the current frame.
	double m_scrollDeltaY = 0.0;   ///< Scroll delta Y for the current frame.
	bool m_firstMouseMove = true;  ///< Flag to ignore the initial mouse jump when the window opens.
};
