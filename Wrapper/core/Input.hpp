#pragma once
#include <X11/Xlib.h>
#include <unordered_set>

class Input {
public:
	Input() = default;

	// WWindow::pollEvents llama beginFrame() UNA vez, antes de drenar la cola.
	void beginFrame();

	// WWindow::pollEvents llama handleEvent(ev) por cada XEvent.
	void handleEvent(const XEvent& ev);

	// Limpia TODO (útil al perder el foco: las teclas "pulsadas" dejan de estarlo).
	void reset();

	// Teclado (KeySym: XK_w, XK_Left, XK_Escape, ...)
	bool isKeyDown    (KeySym k) const;
	bool isKeyPressed (KeySym k) const;   // flanco: se pulsó este frame
	bool isKeyReleased(KeySym k) const;   // flanco: se soltó este frame

	// Ratón
	bool isMouseButtonDown    (int b) const;
	bool isMouseButtonPressed (int b) const;
	bool isMouseButtonReleased(int b) const;

	int   mouseX()      const { return m_mouseX; }
	int   mouseY()      const { return m_mouseY; }
	int   mouseDeltaX() const { return m_deltaX; }
	int   mouseDeltaY() const { return m_deltaY; }
	float scrollDelta() const { return m_scrollDelta; }

private:
	std::unordered_set<KeySym> m_keysDown;
	std::unordered_set<KeySym> m_keysPressed;
	std::unordered_set<KeySym> m_keysReleased;

	std::unordered_set<int> m_buttonsDown;
	std::unordered_set<int> m_buttonsPressed;
	std::unordered_set<int> m_buttonsReleased;

	int   m_mouseX = 0, m_mouseY = 0;
	int   m_deltaX = 0, m_deltaY = 0;
	float m_scrollDelta = 0.f;
};
