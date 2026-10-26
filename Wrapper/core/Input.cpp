#include "core/Input.hpp"
#include <X11/keysym.h>

void Input::beginFrame()
{
	m_keysPressed.clear();
	m_keysReleased.clear();
	m_buttonsPressed.clear();
	m_buttonsReleased.clear();
	m_deltaX = 0;
	m_deltaY = 0;
	m_scrollDelta = 0.f;
}

void Input::reset() {
	m_keysDown.clear();
	m_keysPressed.clear();
	m_keysReleased.clear();
	m_buttonsDown.clear();
	m_buttonsPressed.clear();
	m_buttonsReleased.clear();
	m_mouseX = m_mouseY = 0;
	m_deltaX = m_deltaY = 0;
	m_scrollDelta = 0.f;
}

void Input::handleEvent(const XEvent& ev)
{
	switch (ev.type) {
		case KeyPress: {
			KeySym sym = XLookupKeysym(const_cast<XKeyEvent*>(&ev.xkey), 0);

			if (m_keysDown.insert(sym).second)
				m_keysPressed.insert(sym);
			break;
		}
		case KeyRelease: {
			KeySym sym = XLookupKeysym(const_cast<XKeyEvent*>(&ev.xkey), 0);

			if (m_keysDown.erase(sym))
				m_keysReleased.insert(sym);
			break;
		}

		case ButtonPress: {
			if (ev.xbutton.button == Button4) {
				m_scrollDelta += 1.f;
				break;
			}
			if (ev.xbutton.button == Button5) { 
				m_scrollDelta -= 1.f; 
				break;
			}

			int b = ev.xbutton.button;

			if (m_buttonsDown.insert(b).second)
				m_buttonsPressed.insert(b);
			break;
		}
		case ButtonRelease: {
			if (ev.xbutton.button == Button4 || ev.xbutton.button == Button5)
				break;
			int b = ev.xbutton.button;
			if (m_buttonsDown.erase(b))
				m_buttonsReleased.insert(b);
			break;
		}

		case MotionNotify: {
			int nx = ev.xmotion.x;
			int ny = ev.xmotion.y;
			m_deltaX += nx - m_mouseX;
			m_deltaY += ny - m_mouseY;
			m_mouseX = nx;
			m_mouseY = ny;
			break;
		}

		case FocusOut:
			reset();
			break;

		default:
			break;
	}
}

bool Input::isKeyDown    (KeySym k) const { return m_keysDown.count(k)     > 0; }
bool Input::isKeyPressed (KeySym k) const { return m_keysPressed.count(k)  > 0; }
bool Input::isKeyReleased(KeySym k) const { return m_keysReleased.count(k) > 0; }

bool Input::isMouseButtonDown    (int b) const { return m_buttonsDown.count(b)     > 0; }
bool Input::isMouseButtonPressed (int b) const { return m_buttonsPressed.count(b)  > 0; }
bool Input::isMouseButtonReleased(int b) const { return m_buttonsReleased.count(b) > 0; }
