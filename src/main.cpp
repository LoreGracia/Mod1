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
#include <GPUSPH.hpp>
#include <Input.hpp>
#include <Mesh.hpp>
#include <Particle.hpp>
#include <Particles.hpp>
#include <Renderer.hpp>
#include <Shader.hpp>
#include <WWindow.hpp>
#include <WaterRenderParams.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp> // Necesario para translate/scale
#include <gtc/type_ptr.hpp>
#include <memory>
#include <scene/Transform.hpp>
#include <vector>

// int main()
// {
// 	try {
// 		Environment env;

// 		WWindow::Config cfg;
// 		cfg.title		= "SPH Water — 1=rise 2=wave 3=rain R=reset";
// 		cfg.width		= 1000;
// 		cfg.height		= 700;
// 		cfg.glMajor		= 4;
// 		cfg.glMinor		= 3;
// 		cfg.coreProfile = true;
// 		cfg.vsync		= true;
// 		env.addWindow(cfg);

// 		struct State
// 		{
// 			Camera							  cam;
// 			Renderer						  renderer;
// 			std::shared_ptr<Shader>			  shader;
// 			std::shared_ptr<Particles>		  water;
// 			std::shared_ptr<GPUSPH>			  gpuSPH;
// 			std::shared_ptr<GPUMarchingCubes> marchingCubes;
// 			Particles::Mode					  mode = Particles::Mode::UniformRise;
// 			float							  time = 0.f;
// 			float							  waterVolume = 0.f;
// 			float							  surfaceY = -1.f;
// 			float							  rainRate = 0.10f;
// 			float							  particleVolume = 1.f;
// 			float							  emitAccumulator = 0.f;
			
// 			// Variables para el Fake Volume
// 			std::shared_ptr<Shader> volumeShader;
// 			std::shared_ptr<Mesh>   volumeMesh;
// 			glm::vec3 boundsMin{-1.f, -1.f, -1.f};
// 			glm::vec3 boundsMax{ 1.f,  3.f,  1.f};
// 			WaterRenderParams waterParams;
// 		} st;

// 		env.setInit([&](WWindow& w) {
// 			st.shader = std::make_shared<Shader>();
// 			if (!st.shader->loadFromFile("src/water/shader/waterVertex.glsl",
// 										 "src/water/shader/waterFrag.frag")) {
// 				std::fprintf(stderr, "shader failed\n");
// 				return;
// 			}

// 			st.water = std::make_shared<Particles>();
// 			Particles::Config pcfg;
// 			pcfg.boundsMin = {-1.0f, -1.f, -1.0f};
// 			pcfg.boundsMax = {1.0f,  3.0f, 1.0f};

// 			// Guardamos los bounds en el State
// 			st.boundsMin = pcfg.boundsMin;
// 			st.boundsMax = pcfg.boundsMax;
// 			st.surfaceY = pcfg.boundsMin.y;
// 			st.waterParams.boundsMin = pcfg.boundsMin;
// 			st.waterParams.boundsMax = pcfg.boundsMax;
// 			st.waterParams.surfaceY = st.surfaceY;
// 			st.waterParams.deep		 = glm::vec3(0.01f, 0.05f, 0.15f);
// 			st.waterParams.shallow	 = glm::vec3(0.05f, 0.25f, 0.50f);

// 			pcfg.h			  = 0.25f;
// 			pcfg.restDensity  = 150.f;
// 			pcfg.particleMass = 0.5f;
// 			st.particleVolume = pcfg.particleMass / pcfg.restDensity;
// 			pcfg.stiffness	  = 120.f;
// 			pcfg.viscosity    = 0.12f;
// 			pcfg.gravity      = -9.8f;
// 			pcfg.damping      = 0.99f;
// 			pcfg.substeps     = 8;
// 			pcfg.wallRestitution = 0.0f;
// 			pcfg.pointSize = 15.0f;
// 			pcfg.alpha	   = 0.9f;
// 			pcfg.rainHideMargin = 0.06f;

// 			st.marchingCubes = std::make_shared<GPUMarchingCubes>();
// 			bool ok = st.marchingCubes->init(
// 				 pcfg.boundsMin, pcfg.boundsMax, 48,
// 				 "src/water/shader/density_splat.comp", "src/water/shader/marching_cubes.comp",
// 				 "src/water/shader/meshVertex.glsl", "src/water/shader/meshFrag.frag");
// 			if (!ok) {
// 				std::fprintf(stderr, "Failed to init Marching Cubes\n");
// 			}
			
// 			st.marchingCubes->setIsoLevel(1.05f);
// 			st.marchingCubes->setDensityScale(2000.0f);
// 			st.marchingCubes->setParticleRadius(0.30f);
// 			st.marchingCubes->setFallSpeedCutoff(-1.0f);
// 			st.marchingCubes->setSettleSpeed(0.5f);
// 			st.marchingCubes->setSplatYScale(2.5f);
// 			st.marchingCubes->setWaterLevel(pcfg.boundsMin.y);
			
// 			if (const char* m = std::getenv("WATER_MODE")) {
// 				switch (std::atoi(m)) {
// 				case 1: st.mode = Particles::Mode::UniformRise; break;
// 				case 2: st.mode = Particles::Mode::Wave; break;
// 				case 3: st.mode = Particles::Mode::Rain; break;
// 				default: break;
// 				}
// 			}
// 			st.water->init(1000, st.mode, pcfg);

// 			GPUSPH::Params gcfg;
// 			gcfg.h				 = pcfg.h;
// 			gcfg.restDensity	 = pcfg.restDensity;
// 			gcfg.stiffness		 = pcfg.stiffness;
// 			gcfg.viscosity		 = pcfg.viscosity;
// 			gcfg.gravity		 = pcfg.gravity;
// 			gcfg.damping		 = pcfg.damping;
// 			gcfg.substeps		 = pcfg.substeps;
// 			gcfg.particleMass	 = pcfg.particleMass;
// 			gcfg.wallRestitution = pcfg.wallRestitution;
// 			gcfg.boundsMin		 = pcfg.boundsMin;
// 			gcfg.boundsMax		 = pcfg.boundsMax;
// 			gcfg.rainMode		 = (st.mode == Particles::Mode::Rain);
// 			gcfg.rainSettleSpeed = pcfg.rainSettleSpeed;
// 			gcfg.rainAirBand	 = pcfg.rainAirBand;
// 			gcfg.rainPointSize	 = pcfg.rainPointSize;
// 			gcfg.rainAlpha		 = pcfg.rainAlpha;
// 			gcfg.rainColor		 = pcfg.rainColor;
// 			gcfg.rainSplashSpeed = pcfg.rainSplashSpeed;

// 			st.gpuSPH = std::make_shared<GPUSPH>();
// 			// IMPORTANTE: Reservamos capacidad para 50,000 aunque empecemos con 1000
// 			if (!st.gpuSPH->init(50000, gcfg, "src/water/shader/sph_clear_predict.comp",
// 								 "src/water/shader/sph_hash.comp", "src/water/shader/sph_density.comp",
// 								 "src/water/shader/sph_forces.comp",
// 								 "src/water/shader/sph_integrate.comp")) {
// 				std::fprintf(stderr, "Failed to init GPU SPH\n");
// 				return;
// 			}
// 			if (st.mode == Particles::Mode::Rain)
// 				st.gpuSPH->uploadParticles(std::vector<Particle>{});
// 			else
// 				st.gpuSPH->uploadParticles(st.water->getParticles());

// 			st.marchingCubes->setExternalParticleBuffer(
// 				st.gpuSPH->particleBuffer(), static_cast<GLuint>(st.gpuSPH->particleCount()));

// 			// ── Inicializar Fake Volume ──
// 			st.volumeShader = std::make_shared<Shader>();
// 			if (!st.volumeShader->loadFromFile("src/water/shader/volumeVertex.glsl",
// 											   "src/water/shader/volumeFrag.frag")) {
// 				std::fprintf(stderr, "Volume shader failed to load\n");
// 			}
// 			st.volumeMesh = std::make_shared<Mesh>();
// 			*st.volumeMesh = Mesh::triangle(); // En este proyecto, triangle() genera un cubo

// 			st.cam = Camera({3.5f, 4.5f, 5.5f}, {0.f, 1.0f, 0.f});

// 			st.renderer.depthTest(true);
// 			st.renderer.cullFace(false);
// 			st.renderer.blend(false);

// 			glEnable(GL_PROGRAM_POINT_SIZE);
// 			(void)w;
// 		});

// 		env.setUpdate([&](WWindow& w, float dt) {
// 			Input& in = w.input();
// 			if (in.isKeyPressed(Key::Code::Escape))
// 				w.requestClose();

// 			auto resetSim = [&](Particles::Mode m, bool resetVolume) {
// 				const bool wasRain = (st.mode == Particles::Mode::Rain);
// 				st.mode = m;
// 				st.water->reset(m);
// 				st.gpuSPH->setRainMode(m == Particles::Mode::Rain);

// 				if (m == Particles::Mode::Rain) {
// 					// Repeated rain restarts keep the GPU state untouched so
// 					// the accumulated drops do not vanish. Entering rain from
// 					// another mode reseeds the preserved logical volume.
// 					if (!wasRain || resetVolume) {
// 						st.gpuSPH->uploadParticles(std::vector<Particle>{});
// 						if (!resetVolume) {
// 							const float safeParticleVolume =
// 								st.particleVolume > 1e-6f ? st.particleVolume : 1e-6f;
// 							const size_t seedCount =
// 								static_cast<size_t>(st.waterVolume / safeParticleVolume);
// 							st.gpuSPH->emitRainParticles(seedCount);
// 						}
// 					}
// 				} else {
// 					st.gpuSPH->uploadParticles(st.water->getParticles());
// 				}

// 				if (resetVolume) {
// 					st.waterVolume    = 0.0f;
// 					st.surfaceY       = st.boundsMin.y;
// 					st.emitAccumulator = 0.0f;
// 				}
// 			};

// 			const bool resetVolume = in.isKeyPressed(Key::Code::R);
// 			if (in.isKeyPressed(Key::Code::Num1)) resetSim(Particles::Mode::UniformRise, false);
// 			if (in.isKeyPressed(Key::Code::Num2)) resetSim(Particles::Mode::Wave, false);
// 			if (in.isKeyPressed(Key::Code::Num3)) resetSim(Particles::Mode::Rain, false);
// 			if (resetVolume)					  resetSim(st.mode, true);

// 			// ── LLENADO AUTOMÁTICO ──
// 			// El volumen solo crece mientras llueve. Al salir de Rain queda
// 			// congelado (no se reduce) y la superficie se deriva de él.
// 			float areaXZ = (st.boundsMax.x - st.boundsMin.x) *
// 						   (st.boundsMax.z - st.boundsMin.z);
// 			if (areaXZ < 1e-4f)
// 				areaXZ = 1e-4f;

// 			if (st.mode == Particles::Mode::Rain) {
// 				const float safeParticleVolume =
// 					st.particleVolume > 1e-6f ? st.particleVolume : 1e-6f;
// 				const float maxVolume =
// 					(st.boundsMax.y - st.boundsMin.y - 0.1f) * areaXZ;

// 				// Stop both the level and the emitter once the container is
// 				// full; the already airborne drops fall and hide below the
// 				// surface instead of spawning new ones.
// 				if (st.waterVolume < maxVolume) {
// 					st.waterVolume += dt * st.rainRate;
// 					if (st.waterVolume > maxVolume)
// 						st.waterVolume = maxVolume;

// 					// Emit one drop per particle-volume so the physical mass
// 					// and the logical volume advance at the same rate.
// 					const float emitRate = st.rainRate / safeParticleVolume;
// 					st.emitAccumulator += dt * emitRate;
// 					const size_t toEmit = static_cast<size_t>(st.emitAccumulator);
// 					if (toEmit > 0) {
// 						st.emitAccumulator -= static_cast<float>(toEmit);
// 						st.gpuSPH->emitRainParticles(toEmit);
// 					}
// 				}
// 			}

// 			st.marchingCubes->setExternalParticleBuffer(
// 				st.gpuSPH->particleBuffer(), static_cast<GLuint>(st.gpuSPH->particleCount()));

// 			st.surfaceY = glm::clamp(st.boundsMin.y + st.waterVolume / areaXZ,
// 									 st.boundsMin.y, st.boundsMax.y - 0.1f);
// 			st.waterParams.surfaceY = st.surfaceY;

// 			float heightRatio = (st.surfaceY - st.boundsMin.y) /
// 								(st.boundsMax.y - st.boundsMin.y);

// 			st.marchingCubes->setWaterLevel(st.waterParams.surfaceY);
// 			st.marchingCubes->setFillScale(1.0f + heightRatio * 5.0f);
// 			st.marchingCubes->setSplatMaxHeight(st.surfaceY - st.boundsMin.y + 0.3f);
// 			st.marchingCubes->setSplatHeight(st.surfaceY - st.boundsMin.y + 0.1f);

// 			st.gpuSPH->update(dt);
// 			st.marchingCubes->setWaterEnergy(st.gpuSPH->waterEnergy());
// 			st.time += dt;

// 			// Cámara
// 			float	  speed = 3.f * dt;
// 			glm::vec3 fwd	= st.cam.forward();
// 			glm::vec3 right = st.cam.right();
// 			glm::vec3 up	= st.cam.up();
// 			glm::vec3 move{0.f};
// 			if (in.isKeyDown(Key::Code::W)) move += fwd;
// 			if (in.isKeyDown(Key::Code::S)) move -= fwd;
// 			if (in.isKeyDown(Key::Code::D)) move += right;
// 			if (in.isKeyDown(Key::Code::A)) move -= right;
// 			if (glm::length(move) > 1e-6f)
// 				st.cam.move(glm::normalize(move) * speed);

// 			if (in.isMouseButtonDown(Mouse::Button::Right)) {
// 				const float s = 0.02f;
// 				st.cam.move(right * (-static_cast<float>(in.mouseDeltaX()) * s) +
// 							up * (static_cast<float>(in.mouseDeltaY()) * s));
// 			}
// 			if (in.scrollDelta() != 0.0)
// 				st.cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);
				
// 			if (in.isKeyPressed(Key::Code::Space)) {
// 				resetSim(st.mode, false);
// 			}
// 		});

// 		env.setRender([&](WWindow& w) {
// 			int rw, rh;
// 			if (w.consumeResize(rw, rh))
// 				st.renderer.setViewport(rw, rh);
// 			else
// 				st.renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

// 			st.renderer.setClearColor(0.15f, 0.18f, 0.22f, 1.f); // Fondo gris-azulado
// 			st.renderer.clear(true, true, false);

// 			glm::mat4 vp = st.cam.projection(w.aspect()) * st.cam.view();

// 			glEnable(GL_BLEND);
// 			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

// 			// 1. Dibujar las gotas de lluvia (opacas, escriben profundidad)
// 			glDepthMask(GL_TRUE);
// 			if (st.mode == Particles::Mode::Rain) {
// 				st.shader->use();
// 				st.shader->setFloat("uSurfaceY", st.waterParams.surfaceY);
// 				st.gpuSPH->renderRainDrops(*st.shader, vp);
// 			}

// 			// 2. Dibujar el Fake Volume (el cuerpo del agua)
// 			// La altura visual sale de la misma superficie que Marching Cubes.
// 			float waterHeight = st.waterParams.surfaceY - st.boundsMin.y;
			
// 			if (waterHeight > 0.01f && st.volumeMesh && st.volumeShader) {
// 				glm::mat4 model = glm::mat4(1.0f);
// 				float centerY = st.boundsMin.y + waterHeight * 0.5f;
// 				model = glm::translate(model, glm::vec3(0.0f, centerY, 0.0f));
				
// 				float sizeX = st.boundsMax.x - st.boundsMin.x;
// 				float sizeZ = st.boundsMax.z - st.boundsMin.z;
// 				model = glm::scale(model, glm::vec3(sizeX, waterHeight, sizeZ));

// 				st.volumeShader->use();
// 				applyWaterUniforms(*st.volumeShader, st.waterParams);
// 				st.volumeShader->setMat4("uModel", model);
// 				st.volumeShader->setMat4("uViewProj", vp);
// 				st.volumeShader->setVec3("uCameraPos", st.cam.position());
				
// 				glDepthMask(GL_FALSE); // No escribe prof para que la superficie brille encima
// 				st.volumeMesh->draw();
// 				glDepthMask(GL_TRUE);
// 			}

// 			// 3. Generar y dibujar la superficie de Marching Cubes
// 			st.marchingCubes->generateMesh();
// 			if (const char* v = std::getenv("MC_DEBUG_COUNT")) {
// 				static int dbgFrame = 0;
// 				int		   every	= std::atoi(v);
// 				if (every <= 0) every = 1;
// 				if (++dbgFrame % (every * 30) == 0)
// 					std::printf("[MC] debug vertexCount = %u\n", st.marchingCubes->debugReadDrawCount());
// 			}

// 			glDepthMask(GL_FALSE);
// 			st.marchingCubes->render(vp, st.cam.position(), st.time, st.waterParams);

// 			glDepthMask(GL_TRUE);
// 			glDisable(GL_BLEND);
// 		});

// 		env.run();
// 	} catch (const std::exception& e) {
// 		std::fprintf(stderr, "exception: %s\n", e.what());
// 		return 1;
// 	}
// 	return 0;
// }

#include <Terrain.hpp>
#include <cstdio>
#include <memory>

int main(int ac, char** av)
{
	try {
		(void)ac;
		Terrain te(av[1]);
		te.render();
	} catch (const std::exception& e) {
		std::fprintf(stderr, "exception: %s\n", e.what());
		return 1;
	}
	return 0;
}