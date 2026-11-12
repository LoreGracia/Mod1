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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <memory>
#include <scene/Transform.hpp>
#include <vector>

int main()
{
	try {
		Environment env;

		WWindow::Config cfg;
		cfg.title		= "SPH Water — 1=rise 2=wave 3=rain  R=reset  Esc=close";
		cfg.width		= 1000;
		cfg.height		= 700;
		cfg.glMajor		= 4;
		cfg.glMinor		= 3;
		cfg.coreProfile = true;
		cfg.vsync		= true;
		env.addWindow(cfg);

		struct State
		{
			Camera							  cam;
			Renderer						  renderer;
			std::shared_ptr<Shader>			  shader;
			std::shared_ptr<Particles>		  water;
			std::shared_ptr<GPUSPH>			  gpuSPH;
			std::shared_ptr<GPUMarchingCubes> marchingCubes;
			Particles::Mode					  mode = Particles::Mode::UniformRise;
			float							  time = 0.f;
		} st;

		env.setInit([&](WWindow& w) {
			st.shader = std::make_shared<Shader>();
			// Puntos de lluvia: el vertex shader filtra por el flag rainDrop
			// (solo las gotas se dibujan; el resto pertenece a la malla).
			if (!st.shader->loadFromFile("src/water/shader/waterVertex.glsl",
										 "src/water/shader/waterFrag.frag")) {
				std::fprintf(stderr, "shader failed\n");
				return;
			}

			// Sistema de agua
			st.water = std::make_shared<Particles>();
			Particles::Config pcfg;
			// Caja un poco más pequeña (2x1.5x2 = 6 unidades cúbicas)
			pcfg.boundsMin = {-1.0f, -1.f, -1.0f};
			pcfg.boundsMax = {1.0f, 1.0f, 1.0f};

			pcfg.h			  = 0.25f;	// Radio más pequeño porque la caja es más pequeña
			pcfg.restDensity  = 150.f;
			pcfg.particleMass = 0.5f;  // Masa más pequeña para que no haya picos de presión
			pcfg.stiffness	  = 80.f;  // Fuerza de empuje moderada
			// Fase 5: viscosidad más baja => líquido, no gelatina. El
			// amortiguamiento suave (0.99/subpaso) asienta sin "pegajosidad".
			pcfg.viscosity = 0.12f;
			pcfg.gravity   = -9.8f;
			pcfg.damping   = 0.99f;
			pcfg.substeps  = 8;	 // Subimos a 8 para que la simulación sea más estable y suave

			pcfg.wallRestitution = 0.0f;

			pcfg.pointSize = 15.0f;	 // Puntos MUY grandes para que se solapen entre sí
			pcfg.alpha	   = 0.9f;
			// Fase 5: margen sobre la superficie REAL del charco (se calcula
			// dinámicamente en Particles::update a partir de las partículas
			// asentadas, no con una altura fija).
			pcfg.rainHideMargin = 0.06f;

			st.marchingCubes = std::make_shared<GPUMarchingCubes>();
			bool ok			 = st.marchingCubes->init(
				 pcfg.boundsMin, pcfg.boundsMax,
				 48,  // Fase 4: más resolución => superficie menos facetada
				 "src/water/shader/density_splat.comp", "src/water/shader/marching_cubes.comp",
				 "src/water/shader/meshVertex.glsl", "src/water/shader/meshFrag.frag");
			if (!ok) {
				std::fprintf(stderr, "Failed to init Marching Cubes\n");
			}
			// Fase 3.6/3.7: charco plano. El iso está en unidades de
			// "partículas efectivas": el pico de UNA partícula es 1.0, así
			// que con 1.10 ninguna partícula aislada crea superficie.
			st.marchingCubes->setIsoLevel(1.10f);
			st.marchingCubes->setDensityScale(2000.0f);
			st.marchingCubes->setParticleRadius(0.28f);
			st.marchingCubes->setFallSpeedCutoff(-1.0f);
			// Fase 3.7: solo agua acumulada en el fondo + anisotropía Y
			// (YScale > 1 comprime la distancia vertical y aplana la lámina).
			st.marchingCubes->setSplatHeight(0.15f);
			st.marchingCubes->setSplatMaxHeight(0.22f);
			st.marchingCubes->setSettleSpeed(0.5f);
			st.marchingCubes->setSplatYScale(2.0f);
			if (const char* m = std::getenv("WATER_MODE")) {
				switch (std::atoi(m)) {
				case 1:
					st.mode = Particles::Mode::UniformRise;
					break;
				case 2:
					st.mode = Particles::Mode::Wave;
					break;
				case 3:
					st.mode = Particles::Mode::Rain;
					break;
				default:
					break;
				}
			}
			st.water->init(1000, st.mode, pcfg);

			// Solver GPU: las partículas CPU solo siembran el estado inicial;
			// después de este upload, el estado GPU es autoritativo y la
			// física corre 100% en compute shaders.
			GPUSPH::Params gcfg;
			gcfg.h				 = pcfg.h;
			gcfg.restDensity	 = pcfg.restDensity;
			gcfg.stiffness		 = pcfg.stiffness;
			gcfg.viscosity		 = pcfg.viscosity;
			gcfg.gravity		 = pcfg.gravity;
			gcfg.damping		 = pcfg.damping;
			gcfg.substeps		 = pcfg.substeps;
			gcfg.particleMass	 = pcfg.particleMass;
			gcfg.wallRestitution = pcfg.wallRestitution;
			gcfg.boundsMin		 = pcfg.boundsMin;
			gcfg.boundsMax		 = pcfg.boundsMax;
			gcfg.rainMode		 = (st.mode == Particles::Mode::Rain);
			gcfg.rainSettleSpeed = pcfg.rainSettleSpeed;
			gcfg.rainAirBand	 = pcfg.rainAirBand;
			gcfg.rainPointSize	 = pcfg.rainPointSize;
			gcfg.rainAlpha		 = pcfg.rainAlpha;
			gcfg.rainColor		 = pcfg.rainColor;
			gcfg.rainSplashSpeed = pcfg.rainSplashSpeed;

			st.gpuSPH = std::make_shared<GPUSPH>();
			if (!st.gpuSPH->init(1000, gcfg, "src/water/shader/sph_clear_predict.comp",
								 "src/water/shader/sph_hash.comp", "src/water/shader/sph_density.comp",
								 "src/water/shader/sph_forces.comp",
								 "src/water/shader/sph_integrate.comp")) {
				std::fprintf(stderr, "Failed to init GPU SPH\n");
				return;
			}
			st.gpuSPH->uploadParticles(st.water->getParticles());

			// Handoff sin copia: el pase de splat consume el SSBO del solver
			// directamente (mismo layout GPUParticle); ya no hay upload por
			// frame de partículas.
			st.marchingCubes->setExternalParticleBuffer(
				st.gpuSPH->particleBuffer(), static_cast<GLuint>(st.gpuSPH->particleCount()));

			// (Opcional) colisión con terreno:
			// st.water->setTerrainHeight([](float x, float z) {
			//     return 0.2f * std::sin(x * 3.f) * std::cos(z * 3.f);
			// });
			st.cam = Camera({3.f, 3.f, 4.f}, {0.f, 1.f, 0.f});

			st.renderer.depthTest(true);
			st.renderer.cullFace(false);
			st.renderer.blend(false);  // el blending de partículas se activa en setRender

			glEnable(GL_PROGRAM_POINT_SIZE);

			(void)w;
		});

		env.setUpdate([&](WWindow& w, float dt) {
			Input& in = w.input();
			if (in.isKeyPressed(Key::Code::Escape))
				w.requestClose();

			// Cambiar modo: re-spawn en CPU (semilla) + re-seed del estado GPU.
			auto resetSim = [&](Particles::Mode m) {
				st.mode = m;
				st.water->reset(m);
				st.gpuSPH->setRainMode(m == Particles::Mode::Rain);
				st.gpuSPH->uploadParticles(st.water->getParticles());
			};
			if (in.isKeyPressed(Key::Code::Num1))
				resetSim(Particles::Mode::UniformRise);
			if (in.isKeyPressed(Key::Code::Num2))
				resetSim(Particles::Mode::Wave);
			if (in.isKeyPressed(Key::Code::Num3))
				resetSim(Particles::Mode::Rain);
			if (in.isKeyPressed(Key::Code::R))
				resetSim(st.mode);

			// Física (100% GPU): ya no hay update/upload CPU por frame.
			st.gpuSPH->update(dt);
			st.time += dt;

			// Cámara
			float	  speed = 3.f * dt;
			glm::vec3 fwd	= st.cam.forward();
			glm::vec3 right = st.cam.right();
			glm::vec3 up	= st.cam.up();
			glm::vec3 move{0.f};
			if (in.isKeyDown(Key::Code::W))
				move += fwd;
			if (in.isKeyDown(Key::Code::S))
				move -= fwd;
			if (in.isKeyDown(Key::Code::D))
				move += right;
			if (in.isKeyDown(Key::Code::A))
				move -= right;
			if (glm::length(move) > 1e-6f)
				st.cam.move(glm::normalize(move) * speed);

			if (in.isMouseButtonDown(Mouse::Button::Right)) {
				const float s = 0.02f;
				st.cam.move(right * (-static_cast<float>(in.mouseDeltaX()) * s) +
							up * (static_cast<float>(in.mouseDeltaY()) * s));
			}
			if (in.scrollDelta() != 0.0)
				st.cam.orbit(0.f, 0.f, -static_cast<float>(in.scrollDelta()) * 0.5f);
			if (in.isKeyPressed(Key::Code::Space)) {
				resetSim(st.mode);
				std::printf("[reset] Simulación reiniciada con la tecla Espacio\n");
			}
		});

		env.setRender([&](WWindow& w) {
			int rw, rh;
			if (w.consumeResize(rw, rh))
				st.renderer.setViewport(rw, rh);
			else
				st.renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

			st.renderer.setClearColor(0.05f, 0.05f, 0.10f, 1.f);
			st.renderer.clear(true, true, false);

			glm::mat4 vp = st.cam.projection(w.aspect()) * st.cam.view();

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// st.water->render(*st.shader, vp);

			st.marchingCubes->generateMesh();
			if (const char* v = std::getenv("MC_DEBUG_COUNT")) {
				static int dbgFrame = 0;
				int		   every	= std::atoi(v);
				if (every <= 0)
					every = 1;
				if (++dbgFrame % (every * 30) == 0)
					std::printf("[MC] debug vertexCount = %u\n",
								st.marchingCubes->debugReadDrawCount());
			}

			// Fase 5: las gotas se dibujan PRIMERO y escriben profundidad,
			// así el agua (transparente, sin escritura de depth) las ocluye
			// cuando están sumergidas. Antes se dibujaban encima de la
			// superficie y parecían flotar.
			glDepthMask(GL_TRUE);
			if (st.mode == Particles::Mode::Rain)
				st.gpuSPH->renderRainDrops(*st.shader, vp);

			glDepthMask(GL_FALSE);
			st.marchingCubes->render(vp, st.cam.position(), st.time);

			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
		});

		env.run();
	} catch (const std::exception& e) {
		std::fprintf(stderr, "exception: %s\n", e.what());
		return 1;
	}
	return 0;
}
