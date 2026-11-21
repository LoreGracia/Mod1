#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <glad/gl.h>

#include <Camera.hpp>
#include <Entity.hpp>
#include <Environment.hpp>
#include <GPUMarchingCubes.hpp>
#include <Input.hpp>
#include <Mesh.hpp>
#include <Particles.hpp>
#include <Renderer.hpp>
#include <Shader.hpp>
#include <Terrain.hpp>
#include <WWindow.hpp>
#include <WaterRenderParams.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <memory>
#include <scene/Transform.hpp>
#include <vector>

namespace
{

/** @brief Maximum number of airborne CPU rain drops kept alive. */
constexpr size_t MAX_RAIN_DROPS = 600;

/** @brief Number of rain drops emitted per frame. */
constexpr size_t RAIN_BATCH_PER_FRAME = 2;

/** @brief Volume added to the tank when a single drop reaches the terrain. */
constexpr float DROP_VOLUME = 0.00005f;

/** @brief Downward acceleration applied to CPU rain drops. */
constexpr float DROP_GRAVITY = 9.8f;

/** @brief Initial downward speed of a spawned rain drop. */
constexpr float DROP_SPEED = 5.0f;

/** @brief Airborne rain drop simulated on the CPU. */
struct RainDrop
{
	glm::vec3 position; ///< World-space position of the drop.
	float speedY = 0.f; ///< Current vertical speed (negative while falling).
};

/** @brief Returns a pseudo-random float in [0, 1). */
float randomUnit()
{
	return static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1.0f);
}

}  // namespace

int main(int ac, char** av)
{
	try {
		if (ac < 2) {
			std::fprintf(stderr, "usage: %s <map.mod1>\n", av[0]);
			return 1;
		}
		const std::string mapPath = av[1];

		Environment env;

		WWindow::Config cfg;
		cfg.title		 = "SPH Water + Terrain";
		cfg.width		 = 1000;
		cfg.height		 = 700;
		cfg.glMajor		 = 4;
		cfg.glMinor		 = 3;
		cfg.coreProfile	 = true;
		cfg.vsync		 = true;
		env.addWindow(cfg);

		struct State
		{
			Camera							  cam;
			Renderer						  renderer;
			std::shared_ptr<Shader>			  shader;
			std::shared_ptr<GPUMarchingCubes> marchingCubes;
			std::shared_ptr<Terrain>		  terrain;
			/** @brief Point mesh used to render the airborne CPU rain drops. */
			Mesh dropMesh;
			/** @brief Active CPU rain drops. */
			std::vector<RainDrop> drops;
			std::vector<float> dropPositions;
			std::vector<float> dropDensities;
			std::vector<float> dropVelocities;
			Particles::Mode					  mode = Particles::Mode::Rain;
			float							  time = 0.f;
			float							  waterVolume = 0.f;
			float							  surfaceY = -1.f;
			glm::vec3 boundsMin;
			glm::vec3 boundsMax;
			WaterRenderParams waterParams;
		} st;

		env.setInit([&](WWindow& w) {
			st.terrain = std::make_shared<Terrain>(mapPath);
			st.terrain->init();

			/**
			 * @brief Water container aligned with the terrain footprint.
			 *
			 * The horizontal bounds match the terrain XZ extent. The bottom
			 * sits slightly below the lowest terrain elevation so the whole
			 * heightfield starts above the fluid domain.
			 */
			st.boundsMin = glm::vec3(st.terrain->footprintMin().x,
									 st.terrain->minHeight() - 0.2f,
									 st.terrain->footprintMin().y);
			st.boundsMax = glm::vec3(st.terrain->footprintMax().x,
									 st.terrain->maxHeight() + 1.0f,
									 st.terrain->footprintMax().y);

			st.shader = std::make_shared<Shader>();
			if (!st.shader->loadFromFile("src/water/shader/waterVertex.glsl",
										 "src/water/shader/waterFrag.frag")) {
				std::fprintf(stderr, "shader failed\n");
				return;
			}

			st.waterVolume = 0.0f;
			st.surfaceY	  = st.terrain->minHeight();
			st.waterParams.boundsMin = st.boundsMin;
			st.waterParams.boundsMax = st.boundsMax;
			st.waterParams.surfaceY = st.surfaceY;
			st.waterParams.deep		 = glm::vec3(0.01f, 0.05f, 0.15f);
			st.waterParams.shallow	 = glm::vec3(0.05f, 0.25f, 0.50f);

			st.marchingCubes = std::make_shared<GPUMarchingCubes>();
			bool ok = st.marchingCubes->init(
				 st.boundsMin, st.boundsMax, 48,
				 "src/water/shader/density_splat.comp", "src/water/shader/marching_cubes.comp",
				 "src/water/shader/meshVertex.glsl", "src/water/shader/meshFrag.frag");
			if (!ok) {
				std::fprintf(stderr, "Failed to init Marching Cubes\n");
			}

			st.marchingCubes->setIsoLevel(0.5f);
			st.marchingCubes->setDensityScale(2000.0f);
			st.marchingCubes->setParticleRadius(0.30f);
			st.marchingCubes->setFallSpeedCutoff(-1.0f);
			st.marchingCubes->setSettleSpeed(0.5f);
			st.marchingCubes->setSplatYScale(2.5f);
			st.marchingCubes->setSplatHeight(st.terrain->maxHeight() - st.boundsMin.y + 0.2f);
			st.marchingCubes->setWaterLevel(st.terrain->minHeight());
			st.marchingCubes->setTerrainHeightTexture(st.terrain->heightTexture(),
													   st.terrain->footprintMin(),
													   st.terrain->footprintMax());

			if (const char* m = std::getenv("WATER_MODE")) {
				switch (std::atoi(m)) {
				case 1: st.mode = Particles::Mode::UniformRise; break;
				case 2: st.mode = Particles::Mode::Wave; break;
				case 3: st.mode = Particles::Mode::Rain; break;
				default: break;
				}
			}

			st.drops.reserve(MAX_RAIN_DROPS);
			st.dropPositions.assign(MAX_RAIN_DROPS * 3, 0.0f);
			st.dropDensities.assign(MAX_RAIN_DROPS, 1.0f);
			st.dropVelocities.assign(MAX_RAIN_DROPS * 3, 0.0f);
			st.dropMesh.uploadPoints(st.dropPositions, st.dropDensities, st.dropVelocities);

			st.cam = Camera({3.5f, 4.5f, 5.5f}, {0.f, 1.0f, 0.f});

			st.renderer.depthTest(true);
			st.renderer.cullFace(false);
			st.renderer.blend(false);

			glEnable(GL_PROGRAM_POINT_SIZE);
			(void)w;
		});
		env.setUpdate([&](WWindow& w, float dt) {
			Input& in = w.input();
			if (in.isKeyPressed(Key::Code::Escape))
				w.requestClose();

			const float tankAreaXZ = glm::max(
				(st.boundsMax.x - st.boundsMin.x) * (st.boundsMax.z - st.boundsMin.z), 1e-4f);

			/**
			 * @brief Recomputes the visual water level from the accumulated volume.
			 *
			 * @note The level is now advanced incrementally by landed drops; this
			 * helper restores the exact terrain minimum on reset.
			 */
			auto resetSurfaceLevel = [&]() {
				st.surfaceY = st.terrain->minHeight();
				st.waterParams.surfaceY = st.surfaceY;
			};

			auto resetSim = [&](Particles::Mode m, bool resetVolume) {
				st.mode = m;
				st.drops.clear();

				if (resetVolume) {
					st.waterVolume = 0.0f;
					resetSurfaceLevel();
				}
			};

			const bool resetVolume = in.isKeyPressed(Key::Code::R);
			if (in.isKeyPressed(Key::Code::Num1)) resetSim(Particles::Mode::UniformRise, false);
			if (in.isKeyPressed(Key::Code::Num2)) resetSim(Particles::Mode::Wave, false);
			if (in.isKeyPressed(Key::Code::Num3)) resetSim(Particles::Mode::Rain, false);
			if (resetVolume)					  resetSim(st.mode, true);

			/**
			 * @brief Spawns a small batch of CPU drops at the top of the container.
			 */
			if (st.mode == Particles::Mode::Rain && st.drops.size() < MAX_RAIN_DROPS) {
				const size_t batch =
					std::min(RAIN_BATCH_PER_FRAME, MAX_RAIN_DROPS - st.drops.size());
				for (size_t i = 0; i < batch; ++i) {
					RainDrop drop;
					drop.position = glm::vec3(
						glm::mix(st.boundsMin.x + 0.05f, st.boundsMax.x - 0.05f, randomUnit()),
						st.boundsMax.y - 0.1f,
						glm::mix(st.boundsMin.z + 0.05f, st.boundsMax.z - 0.05f, randomUnit()));
					drop.speedY = -DROP_SPEED;
					st.drops.push_back(drop);
				}
			}

			/**
			 * @brief Integrates CPU drops and converts landed drops into tank volume.
			 *
			 * @note Landed drops are erased immediately so each impact contributes
			 * exactly one DROP_VOLUME to the accumulated water volume.
			 */
			for (auto it = st.drops.begin(); it != st.drops.end();) {
				it->speedY -= DROP_GRAVITY * dt;
				it->position.y += it->speedY * dt;

				const float ground =
					st.terrain->heightAt(glm::vec2(it->position.x, it->position.z));
				if (it->position.y <= ground) {
					st.waterVolume += DROP_VOLUME;
					st.surfaceY = glm::clamp(
						st.terrain->minHeight() + st.waterVolume / tankAreaXZ,
						st.terrain->minHeight(), st.boundsMax.y - 0.1f);
					st.waterParams.surfaceY = st.surfaceY;
					it = st.drops.erase(it);
				} else {
					++it;
				}
			}

			st.dropPositions.assign(st.drops.size() * 3, 0.0f);
			st.dropDensities.assign(st.drops.size(), 1.0f);
			st.dropVelocities.assign(st.drops.size() * 3, 0.0f);
			for (size_t i = 0; i < st.drops.size(); ++i) {
				st.dropPositions[i * 3 + 0] = st.drops[i].position.x;
				st.dropPositions[i * 3 + 1] = st.drops[i].position.y;
				st.dropPositions[i * 3 + 2] = st.drops[i].position.z;
				st.dropVelocities[i * 3 + 0] = 0.0f;
				st.dropVelocities[i * 3 + 1] = st.drops[i].speedY;
				st.dropVelocities[i * 3 + 2] = 0.0f;
			}
			if (!st.drops.empty())
				st.dropMesh.updatePoints(st.dropPositions, st.dropDensities, st.dropVelocities);

			st.marchingCubes->setWaterLevel(st.surfaceY);
			st.time += dt;

			float	  speed = 3.f * dt;
			glm::vec3 fwd	= st.cam.forward();
			glm::vec3 right	= st.cam.right();
			glm::vec3 up	= st.cam.up();
			glm::vec3 move{0.f};
			if (in.isKeyDown(Key::Code::W)) move += fwd;
			if (in.isKeyDown(Key::Code::S)) move -= fwd;
			if (in.isKeyDown(Key::Code::D)) move += right;
			if (in.isKeyDown(Key::Code::A)) move -= right;
			if (glm::length(move) > 1e-6f)
				st.cam.move(glm::normalize(move) * speed);

			if (in.isMouseButtonDown(Mouse::Button::Right)) {
				const float s = 0.02f;
				st.cam.move(right * (-static_cast<float>(in.mouseDeltaX()) * s) +
							 up * (static_cast<float>(in.mouseDeltaY()) * s));
			}
			// float rotX = 0;
			// float rotY = 0;
			// if (in.isMouseButtonDown(Mouse::Button::Left)) {
			// 	const float sens = 0.01f;
			// 	rotY += static_cast<float>(in.mouseDeltaX()) * sens;
			// 	rotX += static_cast<float>(in.mouseDeltaY()) * sens;

			// 	const float lim = glm::radians(89.f);
			// 	if (rotX > lim)
			// 		rotX = lim;
			// 	if (rotX < -lim)
			// 		rotX = -lim;
			// }
			if (in.scrollDelta() != 0.0)
				st.cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);

			if (in.isKeyPressed(Key::Code::Space)) {
				resetSim(st.mode, false);
			}
		});

		env.setRender([&](WWindow& w) {
			int rw, rh;
			if (w.consumeResize(rw, rh))
				st.renderer.setViewport(rw, rh);
			else
				st.renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

			st.renderer.setClearColor(0.15f, 0.18f, 0.22f, 1.f);
			st.renderer.clear(true, true, false);

			glm::mat4 vp = st.cam.projection(w.aspect()) * st.cam.view();

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			st.terrain->render(vp);

			glDepthMask(GL_TRUE);

			/**
			 * @brief Renders airborne CPU drops as opaque points above the surface.
			 */
			if (!st.drops.empty()) {
				st.shader->use();
				st.shader->setMat4("uViewProj", vp);
				st.shader->setFloat("uPointSize", 6.0f);
				st.shader->setVec3("uColorLow", glm::vec3(0.55f, 0.80f, 1.0f));
				st.shader->setVec3("uColorHigh", glm::vec3(0.55f, 0.80f, 1.0f));
				st.shader->setFloat("uAlpha", 1.0f);
				st.shader->setFloat("uDensityMin", 0.0f);
				st.shader->setFloat("uDensityMax", 2.0f);
				st.shader->setFloat("uHideBelowY", -1e30f);
				st.shader->setFloat("uSplashSpeed", 0.0f);
				st.shader->setFloat("uSurfaceY", st.surfaceY);
				glVertexAttrib1f(3, 1.0f);
				st.dropMesh.drawPoints(static_cast<GLsizei>(st.drops.size()));
				glVertexAttrib1f(3, 0.0f);
			}

			/**
			 * @brief Renders the artificial water surface once the tank has volume.
			 *
			 * The surface is generated from waterVolume, so an empty reservoir
			 * draws no water mesh at all.
			 */
			if (st.surfaceY > st.terrain->minHeight() + 0.01f) {
				st.marchingCubes->generateMesh();
				if (const char* v = std::getenv("MC_DEBUG_COUNT")) {
					static int dbgFrame = 0;
					int		   every	= std::atoi(v);
					if (every <= 0) every = 1;
					if (++dbgFrame % (every * 30) == 0)
						std::printf("[MC] debug vertexCount = %u\n", st.marchingCubes->debugReadDrawCount());
				}

				glDepthMask(GL_FALSE);
				st.marchingCubes->render(vp, st.cam.position(), st.time, st.waterParams);
				glDepthMask(GL_TRUE);
			}

			glDisable(GL_BLEND);
		});

		env.run();
	} catch (const std::exception& e) {
		std::fprintf(stderr, "exception: %s\n", e.what());
		return 1;
	}
	return 0;
}
