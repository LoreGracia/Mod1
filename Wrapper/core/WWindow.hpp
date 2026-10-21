#pragma once
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "core/Input.hpp"
#include <atomic>
#include <string>

class WWindow
{
	private:
		Display	*m_display;
	public:
		struct Config {
			std::string title = "Default Name";
			int width = 800, heiight = 600;
			int x = 0, y = 0;
			GLXContext shareContext = nullptr;
			int  glMajor = 4, glMinor = 6;
			bool coreProfile = false;
		};

		WWindow(){};
		explicit WWindow(Config &conf);
		~WWindow(){};
};