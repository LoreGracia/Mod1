#pragma once
#include <unordered_set>

namespace Key
{
enum class Code : int
{
	Unknown		 = -1,
	Space		 = 32,
	Apostrophe	 = 39,
	Comma		 = 44,
	Minus		 = 45,
	Period		 = 46,
	Slash		 = 47,
	Num0		 = 48,
	Num1		 = 49,
	Num2		 = 50,
	Num3		 = 51,
	Num4		 = 52,
	Num5		 = 53,
	Num6		 = 54,
	Num7		 = 55,
	Num8		 = 56,
	Num9		 = 57,
	Semicolon	 = 59,
	Equal		 = 61,
	A			 = 65,
	B			 = 66,
	C			 = 67,
	D			 = 68,
	E			 = 69,
	F			 = 70,
	G			 = 71,
	H			 = 72,
	I			 = 73,
	J			 = 74,
	K			 = 75,
	L			 = 76,
	M			 = 77,
	N			 = 78,
	O			 = 79,
	P			 = 80,
	Q			 = 81,
	R			 = 82,
	S			 = 83,
	T			 = 84,
	U			 = 85,
	V			 = 86,
	W			 = 87,
	X			 = 88,
	Y			 = 89,
	Z			 = 90,
	LeftBracket	 = 91,
	Backslash	 = 92,
	RightBracket = 93,
	GraveAccent	 = 96,
	World1		 = 161,
	World2		 = 162,
	Escape		 = 256,
	Enter		 = 257,
	Tab			 = 258,
	Backspace	 = 259,
	Insert		 = 260,
	Delete		 = 261,
	Right		 = 262,
	Left		 = 263,
	Down		 = 264,
	Up			 = 265,
	PageUp		 = 266,
	PageDown	 = 267,
	Home		 = 268,
	End			 = 269,
	CapsLock	 = 280,
	ScrollLock	 = 281,
	NumLock		 = 282,
	PrintScreen	 = 283,
	Pause		 = 284,
	F1			 = 290,
	F2			 = 291,
	F3			 = 292,
	F4			 = 293,
	F5			 = 294,
	F6			 = 295,
	F7			 = 296,
	F8			 = 297,
	F9			 = 298,
	F10			 = 299,
	F11			 = 300,
	F12			 = 301,
	F13			 = 302,
	F14			 = 303,
	F15			 = 304,
	F16			 = 305,
	F17			 = 306,
	F18			 = 307,
	F19			 = 308,
	F20			 = 309,
	F21			 = 310,
	F22			 = 311,
	F23			 = 312,
	F24			 = 313,
	F25			 = 314,
	Keypad0		 = 320,
	Keypad1		 = 321,
	Keypad2		 = 322,
	Keypad3		 = 323,
	Keypad4		 = 324,
	Keypad5		 = 325,
	Keypad6		 = 326,
	Keypad7		 = 327,
	Keypad8		 = 328,
	Keypad9		 = 329,
	KeypadDecimal = 330,
	KeypadDivide  = 331,
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

Code fromGlfw(int key);
} // namespace Key

namespace Mouse
{
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

Button fromGlfw(int button);
} // namespace Mouse

class Input
{
public:
	Input() = default;

	void beginFrame();
	void endFrame();

	void reset();

	void onKeyDown(Key::Code key);
	void onKeyUp(Key::Code key);

	void onMouseButtonDown(Mouse::Button button);
	void onMouseButtonUp(Mouse::Button button);

	void onMouseMove(double x, double y);
	void onScroll(double xoffset, double yoffset);
	void onFocus(bool focused);

	bool isKeyDown(Key::Code key) const;
	bool isKeyPressed(Key::Code key) const;
	bool isKeyReleased(Key::Code key) const;

	bool isMouseButtonDown(Mouse::Button button) const;
	bool isMouseButtonPressed(Mouse::Button button) const;
	bool isMouseButtonReleased(Mouse::Button button) const;

	double mouseX() const { return m_mouseX; }
	double mouseY() const { return m_mouseY; }
	double mouseDeltaX() const { return m_deltaX; }
	double mouseDeltaY() const { return m_deltaY; }
	double scrollDelta() const { return m_scrollDeltaY; }
	double scrollDeltaX() const { return m_scrollDeltaX; }
	double scrollDeltaY() const { return m_scrollDeltaY; }

private:
	std::unordered_set<Key::Code> m_keysDown;
	std::unordered_set<Key::Code> m_keysPressed;
	std::unordered_set<Key::Code> m_keysReleased;

	std::unordered_set<Mouse::Button> m_buttonsDown;
	std::unordered_set<Mouse::Button> m_buttonsPressed;
	std::unordered_set<Mouse::Button> m_buttonsReleased;

	double m_mouseX		   = 0.0;
	double m_mouseY		   = 0.0;
	double m_lastMouseX	   = 0.0;
	double m_lastMouseY	   = 0.0;
	double m_deltaX		   = 0.0;
	double m_deltaY		   = 0.0;
	double m_scrollDeltaX  = 0.0;
	double m_scrollDeltaY  = 0.0;
	bool   m_firstMouseMove = true;
};
