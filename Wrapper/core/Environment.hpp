#pragma once
#include "core/WWindow.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

class Environment {
	public:
		using InitCallback   = std::function<void(WWindow&)>;
		using UpdateCallback = std::function<void(WWindow&, float)>;
		using RenderCallback = std::function<void(WWindow&)>;

		Environment() = default;
		~Environment();

		Environment(const Environment &)				= delete;
		Environment &operator=(const Environment &)	= delete;

		WWindow& addWindow(WWindow::Config &cfg);

		void setInit  (InitCallback   cb) { m_init   = std::move(cb); }
		void setUpdate(UpdateCallback cb) { m_update = std::move(cb); }
		void setRender(RenderCallback cb) { m_render = std::move(cb); }

		void run();

		void stop() { m_running.store(false); }

		template<typename T>
		std::shared_ptr<T> track(std::shared_ptr<T> p) {
			m_gpuResources.push_back(p);
			return p;
		}

	private:
		void windowThread(WWindow *win);
		void cleanupGpuResources();

		std::vector<std::unique_ptr<WWindow>> m_windows;
		std::vector<std::shared_ptr<void>>    m_gpuResources;
		std::vector<std::thread>              m_threads;
		std::atomic<bool>                     m_running{false};

		InitCallback   m_init;
		UpdateCallback m_update;
		RenderCallback m_render;
};
