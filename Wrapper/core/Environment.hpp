#pragma once
#include "core/WWindow.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

/**
 * @file Environment.hpp
 * @brief Defines the Environment class, which orchestrates the application lifecycle,
 * window management, and GPU resource tracking.
 */

/**
 * @class Environment
 * @brief The main application context.
 *
 * Manages the creation of windows, the main loop (init, update, render),
 * and holds ownership of GPU resources to ensure proper cleanup.
 */
class Environment
{
public:
	/** @brief Callback executed once during the initialization phase. */
	using InitCallback = std::function<void(WWindow&)>;

	/** @brief Callback executed every frame to update the application state. */
	using UpdateCallback = std::function<void(WWindow&, float)>;

	/** @brief Callback executed every frame to render the application. */
	using RenderCallback = std::function<void(WWindow&)>;

	/** @brief Default constructor. */
	Environment() = default;

	/** @brief Destructor. Cleans up windows and tracked GPU resources. */
	~Environment();

	/** @brief Copy constructor is deleted. Environment manages unique resources. */
	Environment(const Environment&) = delete;

	/** @brief Copy assignment operator is deleted. Environment manages unique resources. */
	Environment& operator=(const Environment&) = delete;
	/**
	 * @brief Creates and adds a new window to the environment.
	 * @param cfg The configuration parameters for the new window.
	 * @return A reference to the newly created WWindow instance.
	 */
	WWindow& addWindow(WWindow::Config& cfg);

	/**
	 * @brief Registers the initialization callback.
	 * @param cb The function to execute during initialization.
	 */
	void setInit(InitCallback cb) { m_init = std::move(cb); }

	/**
	 * @brief Registers the update callback.
	 * @param cb The function to execute every frame for logic updates.
	 */
	void setUpdate(UpdateCallback cb) { m_update = std::move(cb); }

	/**
	 * @brief Registers the render callback.
	 * @param cb The function to execute every frame for rendering.
	 */
	void setRender(RenderCallback cb) { m_render = std::move(cb); }

	/**
	 * @brief Starts the main application loop.
	 * This function blocks until stop() is called or all windows are closed.
	 */
	void run();

	/** @brief Signals the main loop to stop gracefully. */
	void stop() { m_running.store(false); }

	/**
	 * @brief Tracks a shared GPU resource so it is automatically cleaned up
	 * when the Environment is destroyed.
	 * @tparam T The type of the resource (e.g., Shader, Mesh, Buffer).
	 * @param p A shared pointer to the GPU resource.
	 * @return The same shared pointer, allowing for method chaining.
	 */
	template <typename T> std::shared_ptr<T> track(std::shared_ptr<T> p)
	{
		m_gpuResources.push_back(p);
		return p;
	}

private:
	/** @brief Clears all tracked GPU resources. */
	void cleanupGpuResources();

	std::vector<std::unique_ptr<WWindow>> m_windows;  ///< List of created windows.
	std::vector<std::shared_ptr<void>>
					  m_gpuResources;	 ///< List of tracked GPU resources for automatic cleanup.
	std::atomic<bool> m_running{false};	 ///< Flag indicating if the main loop is currently running.
	bool			  m_glLoaded = false;  ///< Flag indicating if OpenGL/GLAD has been loaded.

	InitCallback   m_init;	  ///< The registered initialization callback.
	UpdateCallback m_update;  ///< The registered update callback.
	RenderCallback m_render;  ///< The registered render callback.
};
