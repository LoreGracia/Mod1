# Informe técnico: interacción Agua–Terreno en `Mod1`

**Autor:** Ingeniería Gráfica (C++17 / OpenGL 4.3 / GPGPU)  
**Fecha:** 2026-11-21  
**Estado:** diagnóstico y propuesta de arquitectura. **No se ha modificado código fuente.**

---

## 0. Resumen ejecutivo

Tras el merge `f3f4888 merge water to terrain`, el repositorio contiene **tres subsistemas completos pero desacoplados**:

1. `Terrain`: heightfield generado en CPU y subido como malla estática.
2. `GPUSPH`: solver SPH 100 % GPU con colisión contra un **plano horizontal**.
3. `GPUMarchingCubes` + Fake Volume: dos representaciones de agua que ignoran la topografía.

El problema no es un único bug, sino la **ausencia de una representación compartida del terreno**. El heightfield existe solo en la CPU (`Terrain::vertices`) y en un VBO; ningún shader de agua puede muestrearlo. Además, hay un hallazgo de integración crítico: en el `main()` activo el código de agua está **comentado en su totalidad** (`src/main.cpp:33-383`) y el executable solo dibuja el terreno (`src/main.cpp:389-399`). Por tanto, hoy mismo no existe un bucle común donde pueda ocurrir la interacción.

**Propuesta núcleo:** exportar el heightfield como una **textura 2D R32F** (`terrain height texture`) generada por `Terrain` a partir de la misma función `sampleHeight()` que construye la malla, y enlazar esa única textura en (a) `sph_integrate.comp` para colisión con normal y fricción, (b) `marching_cubes.comp` como CSG `min(densidad − iso, y − altura_terreno)`, y (c) `volumeFrag.frag` / `meshFrag.frag` para recorte y espuma de costa.

---

## 1. Hallazgo de integración previo al diagnóstico físico

| Elemento | Ubicación | Observación |
|---|---|---|
| `main()` activo | `src/main.cpp:389-399` | `Terrain te(av[1]); te.render();` — instancia **solo** terreno. |
| Pipeline de agua | `src/main.cpp:33-383` | Bloque `main()` alternativo **comentado**, con `GPUSPH`, `GPUMarchingCubes`, Fake Volume y lluvia. |
| Ventana | `Terrain::render()` (`Terrain.cpp:475-513`) | `Terrain` posee su propio `Environment`, `WWindow::Config`, `Camera` y `Renderer`, y llama a `env.run()`. El agua comentada poseía los suyos propios. |

**Consecuencia:** incluso si se arregla la física, el agua no aparecerá hasta que exista un único `Environment`/`Camera`/`Renderer` que posea los tres subsistemas. La arquitectura objetivo debe eliminar la propiedad de ventana de `Terrain` (o convertirla en un `TerrainRenderer` sin `env.run()`), y mover la orquestación a `main.cpp`.

Orden de dibujo objetivo (una sola ventana, un solo `vp`):

```text
init:
  Terrain::parse + generateTerrain  ->  heightTexture (R32F) + malla
  GPUSPH::init(bounds derivadas del footprint del terreno)
  GPUMarchingCubes::init(mismo footprint)

frame:
  1. gpuSPH->update(dt)                       // colISIONA contra heightTexture
  2. marchingCubes->setExternalParticleBuffer(gpuSPH->particleBuffer(), ...)
  3. marchingCubes->generateMesh()            // recorta contra heightTexture
  4. renderer.clear()
  5. terrain.render(vp)                       // escribe profundidad
  6. volume.render(vp)                        // depthMask(false), recorte por altura
  7. marchingCubes.render(vp)                 // depthMask(false), superficie CSG
```

---

## 2. Análisis del Terreno

### 2.1 Formato de entrada y normalización

`Terrain::parse()` (`Terrain.cpp:57-79`) lee ficheros `.mod1` (ej. `resources/resources/demo1.mod1` contiene `(10000,10000,6000)`):

- El regex `R"(\d+(?:\.\d+)?)"` extrae ternas `(x, z, h)` en el espacio original `0..20000`.
- `extractNumbers()` (`Terrain.cpp:39-54`) aplica:
  - `n = size / (20000 / valor)` → escalado lineal.
  - `n -= size / 2` para las dos primeras componentes (X y Z), **no para la altura**.
- Con `size = 2` (`Terrain.hpp:34`), el dominio world es **X,Z ∈ [−1, 1]** y la altura escala como `2 · h/20000` (p. ej. `6000 → 0.6`).
- `points.emplace_back(numbers[i], numbers[i+2], numbers[i+1])` reordena a `(x, y=altura, z)`.
- `originalPeak` guarda el pico en escala sin centrar; se usa solo para teñir el shader (`uMapHeight`).

### 2.2 Generación: heightfieldCPU, no malla arbitraria

`Terrain::generateTerrain()` (`Terrain.cpp:144-357`) construye una **rejilla regular de 128 × 128** (`resolution`) sobre `[−1,1]²`:

- `sampleHeight(x,z)` (`Terrain.cpp:222-282`):
  - warp de dominio con `fbm`,
  - influencia gaussiana anisotropa por punto: `exp(−0.5 · (d²/r²)^exponente)`,
  - radios de influencia con jitter y expansión por diferencia de altura,
  - `smoothMax` para fusionar lomas,
  - `borderFade` para anclar los bordes a 0,
  - micro-detalle `fbm · 0.02`.
- Normales **analíticas** por diferencias centrales con `eps = gridSpacing/2` (`Terrain.cpp:299-316`), guardadas en `normals`; el método `getNormals()` (`Terrain.cpp:102-142`) existe pero **no se invoca** en `generateTerrain()`.
- Se calculan `minHeight` / `maxHeight` (`Terrain.cpp:339-345`) y se sube todo con `plane->upload(positions, normals, indices, GL_TRIANGLES)` (`Terrain.cpp:356`).

**Punto clave para la propuesta:** la altura es una función univaluada `y = f(x,z)`. No hay vuelcos, cuevas ni overhangs. Eso hace que una **heightmap 2D** sea exacta y óptima; un SDF 3D sería desperdicio de memoria y ancho de banda.

### 2.3 Representación GPU y shader

- `plane` es un `Mesh` (VAO/VBO/EBO) de `Wrapper/render/Mesh.hpp`.
- `Terrain::render()` aplica un `uModel` con rotaciones `rotY`/`rotX` controladas por ratón (`Terrain.cpp:488-493`). El water solver y Marching Cubes trabajan en coordenadas world con `uGridMin/uBoundsMin` fijos. **El modelo rotado rompe la correspondencia world ↔ heightfield.**
- `terrain/shader/meshVertex.glsl` calcula `vWorldPos = uModel * aPos`, `vHeight = aPos.y` (altura **local**, no world) y normal con `transpose(inverse(uModel))`.
- `terrain/shader/meshFrag.frag` colorea por `h = (vHeight−uMinHeight)/(uMaxHeight−uMinHeight)`, pendiente `slope = 1−N.y` y un factor `sierra = smoothstep(0.28,0.42,uMapHeight)`.

### 2.4 Propiedades relevantes para la interacción

| Propiedad | Valor / estado | Implicación |
|---|---|---|
| Dominio XZ | `[−1,1]²` | Coincide con los `boundsMin/Max.x/z` de la agua comentada. |
| Rango Y | `≈ [0, 0.6]` (depende del `.mod1`) | Los `boundsMin.y = −1` del agua dejan 1 unidad de agua **bajo** toda la montaña. |
| Textura de altura | **No existe** | Debe crearse; es el núcleo de la solución. |
| Accesores públicos | **Ninguno** (`Terrain.hpp:23-64` todo `private`) | Hay que exponer altura, footprint, min/max y textura. |
| Transform | `uModel` rotatable | Debe fijarse a identidad o integrarse en el muestreo. |
| Resolución | 128 × 128, `gridSpacing = 2/127 ≈ 0.01575` | La textura debe alinearse a esta rejilla para evitar grietas. |

---

## 3. Diagnóstico del SPH

### 3.1 Cadena de pases

`GPUSPH::update()` (`GPUSPH.cpp:507-540`) ejecuta por substep:

`Clear & Predict` → `Spatial Hash` (6 fases) → `Density & Pressure` → `Forces & XSPH` → `Integrate`.

Todos los pases reciben `uBoundsMin/uBoundsMax` (`GPUSPH.cpp:486-487`, `buildSpatialHash`, `computeDensityPressure`, `computeForces`). **Ninguno recibe datos del terreno.**

### 3.2 La causa raíz, en una línea

`sph_integrate.comp:141-150`:

```glsl
const float e      = uWallRestitution;
const float ground = uBoundsMin.y;
if (pos.y < ground) {
    pos.y = ground;
    if (vel.y < 0.0)
        vel.y = -vel.y * e;
    vel.x *= 0.98;
    vel.z *= 0.98;
}
```

`ground` es una **constante escalar** igual a `boundsMin.y` (−1 en la configuración comentada). No hay `texture()`, ni `uHeightMap`, ni término `f(x,z)`. El propio Doxygen del shader lo admite en `sph_integrate.comp:32-34`: el terreno opcional del solver CPU "no está migrado (la aplicación nunca lo asigna)".

### 3.3 Por qué el agua "atraviesa" o se ignora

1. **Piso global demasiado bajo**: con terreno entre `0` y `0.6`, las partículas caen hasta `y = −1`, es decir, **1 unidad por debajo de la superficie del terreno**. El agua se acumula en un sótano inexistente.
2. **Sin respuesta a pendiente**: incluso si el piso se subiera a `minHeight`, un plano no puede generar flujo ladera abajo: la restitución solo invierte `vel.y` y la fricción `0.98` actúa sobre `vel.xz`. La componente tangencial gravitacional nunca aparece.
3. **Lluvia mal resuelta**: el aterrizaje usa `pos.y <= ground + 0.05` (`sph_integrate.comp:175`) y el respawn usa `uBoundsMin/Max` (`GPUSPH.cpp:304-309`, `sph_integrate.comp:185-188`). Las gotas "aterrizan" en el vacío bajo la montaña.
4. **Hash espacial coherente con el plano**: `sph_clear_predict.comp:92` predice posiciones sin contacto; el grid se construye sobre la caja. Esto está bien para el solver, pero refuerza que el dominio físico es la caja y no el terreno.
5. **El callback CPU sí existía**: `Particles::setTerrainHeight(HeightFn)` (`Particles.hpp:131-135`) y `Particles::collide()` (`Particles.cpp:240-243`) usan `m_heightFn ? m_heightFn(x,z) : lo.y`. La migración GPU no transportó esa interfaz a `GPUSPH::Params` (`GPUSPH.hpp:42-68`). Es el eslabón perdido exacto.

### 3.4 Efecto en el renderizado

`density_splat.comp:90-99` restringe el splat a una banda baja (`relY <= uSplatHeight` o `|vy| <= uSettleSpeed`) y a un techo absoluto `uSplatMaxHeight`. Si las partículas se asientan en `y = −1`, el agua se genera como una **lámina plana** en el fondo de la caja, sin relación con la topografía. `meshVertex.glsl:49` remata la faena forzando `outPos.y = uWaterLevel + microWave` para **todos** los vértices.

---

## 4. Propuesta de colisión SPH

### 4.1 Evaluación de alternativas

| Opción | Coste | Precisión | Adecuación al código actual |
|---|---|---|---|
| **A. Textura 2D de altura (R32F)** | 1 texel por XZ (128² ≈ 64 KB; 256² ≈ 256 KB), 1 `texture()` por partícula/substep | Exacta para heightfields; permite normal analítica | **Recomendada.** Reutiliza `sampleHeight()`, encaja con GL 4.3 y con Marching Cubes/Volume. |
| B. SDF 3D (`R16F`/`R32F`, 128³) | 8–32 MB + triple interpolación | Soporta overhangs, cuevas, paredes verticales | Desproporcionado: el terreno es `y=f(x,z)`. Reservar si se adopta geometría CSG arbitraria. |
| C. Raycast contra malla (BVH / SSBO de triángulos) | Build de BVH + 1 rayo por partícula | Genérica | Descartada ahora: 128² ≈ 32 K triángulos y no aporta nada sobre una altura univaluada. Útil solo si el terreno deja de ser heightfield. |

**Decisión:** opción A como fuente de verdad, con normal derivada del gradiente de la textura (o reutilizando `normals` de `Terrain` en una textura RG16F opcional). La opción B queda documentada como vía de extensión.

### 4.2 Generación de la height texture

La textura debe construirse **dentro de `generateTerrain()`**, usando el mismo `sampleHeight` lambda, antes de borrar los datos locales. Así se garantiza que malla y física usen exactamente la misma función.

```cpp
/**
 * @brief Builds the shared single-channel terrain height texture.
 *
 * Texel (i, j) stores the world-space elevation of terrain vertex (i, j)
 * of the 128x128 heightfield. Texel centers are aligned with mesh vertices,
 * so the collision field and the rendered surface agree to floating-point
 * precision. The texture is sampled with GL_LINEAR and GL_CLAMP_TO_EDGE.
 *
 * @return OpenGL handle of the R32F height texture, or zero on failure.
 */
GLuint buildHeightTexture() const;

/**
 * @brief Returns the XZ footprint covered by the height texture.
 * @return Minimum and maximum world-space XZ corners of the terrain.
 */
std::pair<glm::vec2, glm::vec2> footprint() const;

/**
 * @brief Samples the terrain height on the CPU.
 *
 * Used by the rain emitter and by offline validation. It performs the same
 * bilinear filtering as the GPU sampler.
 *
 * @param xz World-space horizontal coordinates.
 * @return Terrain elevation in world units.
 */
float heightAt(const glm::vec2& xz) const;
```

Muestreo en GLSL (idéntico en los tres shaders de agua):

```glsl
/**
 * @brief Converts a world-space XZ position into height-texture coordinates.
 *
 * The half-texel offset maps mesh vertex (i, j) to texel center (i + 0.5, j + 0.5).
 *
 * @param xz World-space horizontal coordinates.
 * @return Texture coordinates in [0, 1].
 */
vec2 terrainUV(vec2 xz) {
    vec2 t = (xz - uTerrainMinXZ) / (uTerrainMaxXZ - uTerrainMinXZ);
    return (t * (uTerrainTexSize - 1.0) + 0.5) / uTerrainTexSize;
}

/**
 * @brief Samples the terrain elevation at a world-space XZ position.
 * @param xz World-space horizontal coordinates.
 * @return Terrain elevation in world units.
 */
float terrainHeight(vec2 xz) {
    return texture(uTerrainHeight, clamp(terrainUV(xz), 0.0, 1.0)).r;
}

/**
 * @brief Computes the outward terrain normal from the height texture.
 * @param xz World-space horizontal coordinates.
 * @return Normalized terrain normal pointing away from the ground.
 */
vec3 terrainNormal(vec2 xz) {
    float e = uTerrainTexelWorld.x;
    float hL = terrainHeight(xz - vec2(e, 0.0));
    float hR = terrainHeight(xz + vec2(e, 0.0));
    float hD = terrainHeight(xz - vec2(0.0, e));
    float hU = terrainHeight(xz + vec2(0.0, e));
    return normalize(vec3(hL - hR, 2.0 * e, hD - hU));
}
```

### 4.3 Resolución de contacto en `sph_integrate.comp`

Sustituir el bloque de plano por una resolución **contra el plano tangente** del heightfield:

```glsl
/**
 * @brief Resolves a particle against the heightfield surface.
 *
 * The penetration is removed along the terrain normal, the inbound normal
 * velocity is reflected with restitution and the tangential velocity is
 * damped. The vertical clamp is kept as a safety net for numerically deep
 * penetrations.
 *
 * @param pos Particle position, corrected in place.
 * @param vel Particle velocity, corrected in place.
 * @param restitution Normal reflection coefficient.
 * @param friction Tangential damping coefficient.
 */
void collideTerrain(inout vec3 pos, inout vec3 vel,
                    float restitution, float friction) {
    float h = terrainHeight(pos.xz) + uContactOffset;
    if (pos.y >= h)
        return;

    vec3  n  = terrainNormal(pos.xz);
    float vn = dot(vel, n);

    pos += n * (h - pos.y) / max(n.y, 0.2);

    if (vn < 0.0)
        vel -= n * (vn * (1.0 + restitution));
    vel *= friction;

    pos.y = max(pos.y, h);
}
```

Consideraciones numéricas:

- `uContactOffset` ≈ `0.4 · h_sph` (radio de soporte del kernel) evita que el centro de la partícula quede a media altura del kernel dentro del suelo y reduce el error de densidad en la frontera.
- `max(n.y, 0.2)` evita la división por cero en paredes muy inclinadas; para pendientes extremas puede sustituirse por un clamp de desplazamiento.
- Con `substeps = 8` y `maxSpeed = 10`, el desplazamiento por substep es ≈ 0.04 unidades, comparable al `gridSpacing = 0.0157`. Conviene limitar `uMaxSpeed` o incrementar substeps para evitar túneles a través de lomas estrechas.
- El gradiente de la textura debe calcularse con el mismo `eps` que se usó para las normales de la malla, de modo que la respuesta física y el sombreado coincidan.
- La lluvia debe usar `terrainHeight(pos.xz)` tanto para el aterrizaje como para `respawn` (suelo local + banda de aire), no `uBoundsMin.y`.

### 4.4 Cambios en C++

```cpp
/**
 * @brief Binds the terrain height field used by the SPH integration pass.
 *
 * The texture stays bound for the whole solver lifetime. The footprint is
 * used to build the inverse transform that maps world XZ into texture UV.
 *
 * @param texture OpenGL handle of the R32F height texture.
 * @param footprintMin Minimum world-space XZ corner of the terrain.
 * @param footprintMax Maximum world-space XZ corner of the terrain.
 * @param contactOffset Vertical clearance added to the sampled elevation.
 */
void setTerrainHeight(GLuint texture, const glm::vec2& footprintMin,
                      const glm::vec2& footprintMax, float contactOffset);
```

En `GPUSPH::integrateParticles()` se enlazaría la unidad de textura y se subirían `uTerrainHeight`, `uTerrainMinXZ`, `uTerrainMaxXZ`, `uTerrainTexSize`, `uTerrainTexelWorld` y `uContactOffset`. `GPUSPH::Params` debe incorporar el footprint y el offset; `setParams()` ya reclama buffers cuando cambian los bounds, así que el cambio de dominio queda cubierto.

**Ajuste de dominio recomendado:** `boundsMin.x/z = footprintMin`, `boundsMax.x/z = footprintMax`, `boundsMin.y = minHeight − margen`, `boundsMax.y ≥ maxHeight + margen`. El piso lógico pasa a ser el terreno; `boundsMin.y` queda solo como suelo de seguridad y para el palette de color.

**Ajuste de transform:** fijar `uModel = identity` en `Terrain::render()` (o eliminar `rotY/rotX`). Si se conserva la rotación interactiva, la height texture debe generarse en el espacio rotado y el agua debe recibir la misma matriz; se desaconseja por coste y por riesgo de desincronización.

---

## 5. Propuesta de renderizado

### 5.1 Fake Volume: recorte per-fragmento

El volumen es un cubo (`Mesh::triangle()` en este proyecto) escalado a `(sizeX, waterHeight, sizeZ)` y centrado en `boundsMin.y + waterHeight·0.5` (`src/main.cpp:338-357`). `volumeFrag.frag:40-43` solo descarta la tapa superior (`Ng.y > 0.9`); la tapa inferior y las cuatro paredes se pintan con `alpha ≈ 0.85..1.0`, `glDepthMask(GL_FALSE)`.

**Fallos:**
- La caja existe incluso en columnas donde la montaña supera `surfaceY`: se dibuja agua **dentro** de la roca.
- Las paredes laterales de la caja pueden quedar por delante de la ladera en vistas rasantes y "cortar" la montaña con un plano azul.
- La tapa inferior en `y = −1` es un suelo falso bajo todo el terreno.
- `waterBaseColor()` usa `uBoundsMin.y` como referencia, no la altura local del terreno.

**Corrección:** muestrear la misma height texture en el fragmento y descartar todo lo que quede por debajo:

```glsl
/**
 * @brief Discards volume fragments embedded inside the terrain.
 *
 * Terrain is rendered first and writes depth, so this test only removes the
 * remaining cases where a cube face lies in front of the mountain. The color
 * ramp is evaluated from the local ground instead of the global container.
 *
 * @param worldPos World-space position of the current fragment.
 */
void clipVolumeAgainstTerrain(vec3 worldPos) {
    float ground = terrainHeight(worldPos.xz);
    if (worldPos.y < ground - uClipEpsilon)
        discard;
}
```

Además:

- Sustituir el `waterMax` de `volumeFrag.frag:53` por `vec3(xz, max(uSurfaceY, ground + ε), zz)` para que la paleta use la columna de agua real.
- Añadir atenuación de alpha en una banda delgada sobre `ground` para suavizar la intersección (o muestrear la depth texture del terreno para una transición blanda).
- Mantener `depthTest = true` y `depthMask = false` para el volumen; el terreno ya escribió profundidad.

### 5.2 Marching Cubes: recorte CSG contra el terreno

Hoy `marching_cubes.comp` extrae `d[i] >= uIsoLevel` usando solo densidad de partículas. El terreno no aparece en ninguna ecuación.

**Propuesta:** definir un campo implícito compuesto, con la convención "inside = positivo":

```
w(p) = waterDensity(p) − iso        // dentro del agua
t(p) = p.y − terrainHeight(p.xz)    // sobre el terreno
phi(p) = min(w(p), t(p))            // agua real = w>0 AND t>0
```

La superficie es `phi = 0`. Esto produce:
- la superficie libre del agua cuando `w < t`,
- la línea de costa / intersección exacta con la ladera cuando `t < w`,
- continuidad topológica (no hay que "pegar" dos mallas).

```glsl
/**
 * @brief Builds the clipped implicit field used by Marching Cubes.
 *
 * The water density is converted from its fixed-point representation and
 * shifted so that positive values are inside the fluid. The terrain height
 * subtracts the solid region below the heightfield. Marching Cubes then
 * extracts the zero crossing with the same tables as before.
 *
 * @param densityRaw Fixed-point density stored in the 3D texture.
 * @param worldPos World-space position of the voxel corner.
 * @return Signed field value; positive means water.
 */
float clippedWaterField(uint densityRaw, vec3 worldPos) {
    float water = float(densityRaw) / uDensityScale - uIsoLevel;
    float aboveGround = worldPos.y - terrainHeight(worldPos.xz);
    return min(water, aboveGround);
}
```

En `marching_cubes.comp` los corner values `d[8]` pasarían a ser `clippedWaterField(...)` y el `cubeIndex` se formaría con `d[i] >= 0.0`. El interpolador de aristas trabaja igual; el gradiente de `phi` es discontinuo en la curva de intersección, por lo que se recomienda:

- usar `surfaceNormal()` (gradiente de densidad) cuando `water <= aboveGround + ε`,
- usar `terrainNormal()` en caso contrario,
- mezclar ambos en una banda estrecha para evitar un pliegue de normales.

**Cambios asociados imprescindibles:**
- `density_splat.comp`: relajar `uSplatMaxHeight` (o hacerlo relativo al terreno local). Si no, el agua que sube por una ladera por encima de `boundsMin.y + 0.22` nunca genera densidad y no aparecerá superficie aunque las partículas estén ahí.
- `meshVertex.glsl:45-49`: **eliminar** `outPos.y = uWaterLevel + microWave`. Mientras exista ese aplastamiento global, todo el trabajo de recorte es invisible. Si se quiere conservar el aspecto de lámina plana en lagos, aplicar el nivel solo donde `terrainHeight(xz) < uWaterLevel − ε`, con micro-ondas atenuadas por `uWaterEnergy`.

### 5.3 Espuma de costa y estado GL

- En `meshFrag.frag`, la señal de espuma ya tiene curvatura, flujo y ruido. Añadir un término de costa: `shore = 1 − smoothstep(0.0, δ, vWorldPos.y − terrainHeight(vWorldPos.xz))`, multiplicado por `uWaterEnergy` o por la velocidad del fluido. Esto hace visible la interacción agua–terreno.
- Orden de estado: terreno (`depthMask(true)`, `GL_FILL`) → gotas de lluvia (opacas) → Fake Volume (`depthMask(false)`) → superficie MC (`depthMask(false)`) → restaurar `depthMask(true)`, desactivar `GL_BLEND`.
- Ojo con `glPolygonMode(GL_FRONT_AND_BACK, ...)`: `Terrain::render()` lo activa/desactiva para wireframe; si se unifica el bucle, debe restaurarse a `GL_FILL` antes de dibujar agua.

---

## 6. Arquitectura objetivo

```text
                     +---------------------------+
   .mod1 ----------> | Terrain                   |
                     |  points -> sampleHeight() |
                     +-------------+-------------+
                                   |  (misma funcion)
                   +---------------+----------------+
                   |                                |
          malla 128x128 (VBO)             height texture R32F 128/256
                   |                                |
                   |                 +--------------+---------------+
                   |                 |              |               |
              render depth      sph_integrate   marching_cubes   volume/mesh frag
                   |              collision        CSG clip        discard/foam
                   +----------------+----------------+---------------+
                                    |
                          Unico Environment / Camera / Renderer
                                    |
                                 main.cpp
```

**Principio rector:** una sola fuente de verdad (`sampleHeight` → height texture) consumida por física y render. No se debe reconstruir la altura en el solver con otra fórmula ni muestrear la malla en CPU por frame.

---

## 7. Archivos a modificar

| Archivo | Cambio previsto | Prioridad |
|---|---|---|
| `src/main.cpp` | Reemplazar el `main()` activo por el bucle unificado (terreno + SPH + MC + volumen); eliminar el bloque comentado; unificar cámara/renderer; derivar bounds del footprint. | **Crítica** |
| `src/terrain/Terrain.hpp` | Exponer `heightTexture()`, `heightAt()`, `footprint()`, `minHeight()`, `maxHeight()`, `mesh()`; quitar propiedad de `Environment`/`WWindow`/`Camera`. | **Crítica** |
| `src/terrain/Terrain.cpp` | Generar la height texture dentro de `generateTerrain()`; fijar `uModel = identity` (o eliminar rotación); publicar bounds. | **Crítica** |
| `src/water/shader/sph_integrate.comp` | Sustituir `ground = uBoundsMin.y` por `terrainHeight(pos.xz)`; colisión con normal/fricción; lluvia y respawn relativos al suelo local. | **Crítica** |
| `src/water/GPUSPH.hpp` | `Params`: footprint, contact offset, handle de textura; nuevo `setTerrainHeight()`. | **Crítica** |
| `src/water/GPUSPH.cpp` | Enlazar/subir uniforms de la height texture en `integrateParticles()`; opcionalmente en `clearPredict`; `emitRainParticles()` usando `heightAt()`. | **Crítica** |
| `src/water/shader/marching_cubes.comp` | Campo `phi = min(densidad−iso, y−terreno)`; normal híbrida; iso 0. | **Crítica** |
| `src/water/GPUMarchingCubes.hpp/.cpp` | Handle y uniforms de la height texture; setters; pasar `uIsoLevel = 0` al campo compuesto. | **Crítica** |
| `src/water/shader/meshVertex.glsl` | Eliminar el aplastamiento global a `uWaterLevel`; conservar micro-ondas solo sobre agua en reposo. | **Crítica** |
| `src/water/shader/volumeFrag.frag` | `discard` bajo el terreno; paleta desde la altura local; alpha de transición en la costa. | Alta |
| `src/water/shader/density_splat.comp` | Techo de splat dependiente del terreno; permitir densidad en laderas altas. | Alta |
| `src/water/WaterRenderParams.hpp` | Añadir handle de height texture, footprint, offset y subirlos en `applyWaterUniforms()`. | Alta |
| `src/water/shader/meshFrag.frag` | Espuma de costa y atenuación de profundidad respecto al terreno. | Media |
| `src/water/shader/sph_clear_predict.comp` | (Opcional) predicción con awareness de contacto para reducir penetraciones a alta velocidad. | Media |
| `src/terrain/shader/meshFrag.frag` | (Opcional) humedecimiento/oscurecimiento de la orilla con `uSurfaceY`. | Media |
| `Wrapper/render/Renderer.hpp/.cpp` | (Opcional) depth texture del prepass del terreno para intersección blanda. | Baja |
| `Makefile` | Solo si se crea un nuevo `src/TerrainHeight.hpp` o un módulo de test; el proyecto compila con `-std=c++17 -Wall -Wextra -Werror -pedantic`. | Condicional |

---

## 8. Verificación recomendada

1. **Consistencia de altura:** muestrear 10 000 XZ aleatorios y comparar `heightAt()` (bilinear CPU) contra `sampleHeight()`; error máximo objetivo `< 1e-4`.
2. **No penetración:** tras 600 frames, `min(pos.y − terrainHeight(pos.xz)) ≥ −ε` para todas las partículas asentadas (readback solo en test, nunca en el bucle).
3. **Drenaje:** verter agua sobre un pico y comprobar que el centro de masa se desplaza ladera abajo; con el plano actual el agua permanece quieta en `y = −1`, es el test que demuestra la mejora.
4. **Lluvia:** las gotas deben convertirse en agua asentada al tocar la ladera, no al tocar `boundsMin.y`.
5. **Visual:** sin franjas azules dentro de la montaña; la superficie MC debe seguir la costa; sin `z-fighting` entre malla de terreno y campo CSG.
6. **Rendimiento:** la colisión añade 1–3 `texture()` por partícula y substep. Con 50 000 partículas × 8 substeps ≈ 400 000–1.2 M muestreos/frame; negligible frente al coste del vecindario SPH, pero medir con `MC_DEBUG_COUNT` y temporizadores GPU.

---

## 9. Archivos del terreno que necesito para el siguiente paso

Ya están presentes en el repositorio y los he analizado, pero para escribir el código de colisión y renderizado con garantías necesito que me **pases o confirmes** explícitamente:

1. `src/terrain/Terrain.hpp` — interfaz actual y qué miembros aceptas exponer.
2. `src/terrain/Terrain.cpp` — especialmente `generateTerrain()` y `sampleHeight()` para insertar la generación de la textura sin duplicar la función.
3. `src/terrain/shader/meshVertex.glsl` y `src/terrain/shader/meshFrag.frag` — para coordinar el espacio de coordenadas (`vWorldPos` vs `vHeight`) y la posible humedad de orilla.
4. Un `.mod1` representativo (p. ej. `resources/resources/demo3.mod1`) para validar rangos de altura reales.
5. Confirmaciones de diseño:
   - ¿Se elimina la rotación por ratón del terreno (`rotX`/`rotY`) o debe conservarse?
   - ¿El terreno será siempre un heightfield `y=f(x,z)` o se prevén cuevas/overhangs (en ese caso pasar a SDF 3D)?
   - ¿Quieres que `Terrain` deje de crear su propia ventana y pase a ser un componente dibujado por el `main()` unificado? (Recomendado.)
   - ¿Resolución de height texture: 128 (alineada con la malla) o 256 (más suave, con riesgo de desajuste de un texel)?

> **Resolución:** los archivos se han proporcionado y el Paso 1 está implementado; ver sección 10.

---

## 10. Paso 1 implementado: unificación y textura de altura

### 10.1 Cambios en `src/terrain/Terrain.hpp` / `Terrain.cpp`

- Nuevo miembro privado `GLuint m_heightTexture = 0` con acceso público `heightTexture() const`.
- Nuevos accesores públicos `footprintMin()`, `footprintMax()`, `minHeight()` y `maxHeight()`. Los antiguos miembros `minHeight`/`maxHeight` se renombran a `m_minHeight`/`m_maxHeight` para permitir los getters.
- `generateTerrain()` publica el heightfield como textura **`GL_R32F` de 128 × 128** mediante `glTexImage2D(..., GL_RED, GL_FLOAT, ...)`, rellenándola con `vertices[i].y` (la misma altura de los vértices). Filtros: `GL_LINEAR` y `GL_CLAMP_TO_EDGE`.
- `Terrain` deja de poseer `Environment`, `WWindow::Config`, `Camera`, `Renderer`, `Input` y los hooks de ratón/teclado. Desaparecen `rotX`/`rotY`, `addWindow()`, `initHooks()` y `initEnv()`.
- El constructor solo parsea el `.mod1`; la creación de recursos GPU se traslada a `Terrain::init()`, invocada ya con contexto OpenGL activo desde `env.setInit()`.
- `Terrain::render(const glm::mat4& viewProj)` fija `uModel = glm::mat4(1.0f)`, garantizando un único espacio de coordenadas global.
- Se añaden `release()` y un destructor que libera la textura; `operator=(Terrain&&)` transfiere el handle.

### 10.2 Cambios en `src/main.cpp`

- Se elimina el `main()` que solo dibujaba terreno y se descomenta el pipeline completo de agua (SPH + Marching Cubes + Fake Volume + lluvia), conservando su documentación Doxygen.
- Un único `Environment`, `Camera` y `Renderer` poseen ahora terreno y agua.
- Bounds derivados del footprint (valores actuales tras el Paso 2):
  `boundsMin = (footprintMin.x, minHeight − 0.2, footprintMin.y)`,
  `boundsMax = (footprintMax.x, maxHeight + 1.0, footprintMax.y)`.
- Orden de render: **terreno → gotas de lluvia → Fake Volume → superficie Marching Cubes**, con `glDepthMask(GL_TRUE)` para terreno/gotas y `GL_FALSE` para agua transparente.

### 10.3 Estado y alcance

- Compilación verificada con `make -j2` (C++17, `-Wall -Wextra -Werror -pedantic`): correcta.
- **Resuelto en el Paso 2:** `sph_integrate.comp` ya no usa `uBoundsMin.y` como plano fijo; muestrea la `heightTexture()`. Ver sección 11.
- Riesgo ya mitigado: el piso pasó de `minHeight − 0.5` a `minHeight − 0.2` y la colisión usa `terrainHeight(xz)`, por lo que el agua no se acumula por debajo de la malla.

---

## 11. Paso 2 implementado: colisión e interacción agua–terreno

### 11.1 Alineación de límites y estado inicial (`src/main.cpp`)

- `boundsMin.y = terrain.minHeight() − 0.2f`, `boundsMax.y = terrain.maxHeight() + 1.0f`. Los límites XZ siguen siendo el footprint completo del terreno.
- `surfaceY` se inicializa en `terrain.minHeight()` y `waterVolume = 0.0f`. El `reset` con `R` restaura el mismo valor base.
- La derivación de la superficie usa ahora la base del terreno y no el fondo del contenedor:

  `surfaceY = clamp(minHeight + waterVolume / areaXZ, minHeight, boundsMax.y − 0.1)`.

- `setWaterLevel()` recibe `terrain.minHeight()` en la inicialización y `surfaceY` en cada frame, de modo que `uWaterLevel` nunca queda por debajo del terreno.
- En `setRender`, el Fake Volume y Marching Cubes (incluido `generateMesh()`) se ejecutan solo si `waterVolume > 0.0f`. Las gotas de lluvia siguen dibujándose para visualizar la precipitación mientras se llena el depósito.

### 11.2 Enlace de la textura de altura (`GPUSPH.hpp` / `GPUSPH.cpp`)

- Nueva API pública `setTerrainHeightTexture(GLuint tex, vec2 minXZ, vec2 maxXZ)`. Guarda el handle externo y el footprint; `release()` solo lo desvincula (no lo borra, porque es propiedad de `Terrain`).
- `TERRAIN_HEIGHT_TEXTURE_UNIT = 4`, unidad reservada para muestrear la R32F compartida.
- `integrateParticles()` enlaza la textura y publica `uTerrainHeight`, `uTerrainMinXZ`, `uTerrainMaxXZ` y `uUseTerrainHeight` en cada substep; restaura `GL_TEXTURE0` al terminar.
- Si el handle es 0, `uUseTerrainHeight = 0` y se conserva el piso plano `uBoundsMin.y` como fallback, de modo que `GPUSPH` sigue siendo usable sin terreno.

### 11.3 Colisión en el compute shader (`sph_integrate.comp`)

- Nueva función `terrainHeightAt(vec2 xz)`:

  ```glsl
  vec2 extent = max(uTerrainMaxXZ - uTerrainMinXZ, vec2(1e-4));
  vec2 uv     = clamp((xz - uTerrainMinXZ) / extent, 0.0, 1.0);
  return max(texture(uTerrainHeight, uv).r, uBoundsMin.y);
  ```

- Se sustituye `const float ground = uBoundsMin.y;` por `float ground = terrainHeightAt(pos.xz);`.
- El aterrizaje de lluvia (`flags.x > 0.5 && pos.y <= ground + 0.05`) usa ahora la altura local, así que las gotas se asientan sobre la ladera y no sobre el fondo del contenedor.
- UVs fuera del footprint se clampan a los texels de borde; las paredes laterales mantienen una altura de suelo consistente.

### 11.4 Estado y alcance

- `make -j2` compila correctamente (C++17, `-Wall -Wextra -Werror -pedantic`). El GLSL se compila en runtime; no hay `glslangValidator` instalado en el entorno.
- **Resuelto en el Paso 3:** recorte CSG de Marching Cubes y del Fake Volume (sección 5.2). Queda como mejora opcional la atenuación de la intersección mediante depth texture del terreno.
- Consecuencia esperada del requisito `waterVolume > 0`: en los modos 1 (`UniformRise`) y 2 (`Wave`) el volumen no crece, por lo que el agua permanece oculta hasta activar lluvia con la tecla `3`.

---

## 12. Paso 3 implementado: recorte CSG agua–terreno

### 12.1 Fake Volume (`volumeFrag.frag`)

- Nuevos uniforms `uTerrainHeight` (R32F), `uTerrainMinXZ`, `uTerrainMaxXZ` y `uUseTerrainHeight`.
- Nueva función `terrainHeight(vec2 xz)` con clamp de UV al footprint; sin textura devuelve un centinela bajo y no recorta.
- Antes del cálculo de color: `ground = terrainHeight(vWorldPos.xz)` y `discard` si `vWorldPos.y < ground − 0.01`. Las paredes del cubo dentro de la montaña desaparecen.

### 12.2 Marching Cubes (`marching_cubes.comp`)

- Mismos uniforms de textura y footprint; campo de altura con fallback a `-1000` cuando no hay textura.
- Campo compuesto por esquina: `d[i] = min(densityAt(ci[i]) − uIsoLevel, c[i].y − terrainHeight(c[i].xz))`.
- `cubeIndex` usa `d[i] >= 0.0`; la interpolación de aristas usa `iso = 0.0` en las 12 aristas.
- Normales: se mantiene el gradiente de densidad para la superficie libre y se añade `terrainNormal()` por diferencias centrales de la height texture. `clippedSurfaceNormal()` mezcla ambas en una banda de `2 * cell.y` sobre el terreno, evitando un pliegue de normales en la costa.

### 12.3 Eliminación del aplanado (`meshVertex.glsl`)

- Se elimina `outPos.y = uWaterLevel + microWave` y el segundo `vWorldPos = outPos`.
- La malla usa `aPos.xyz` tal cual, por lo que la intersección CSG con la ladera es visible.
- `uWaterLevel`, `uTime` y `waveH()` se conservan como legacy por compatibilidad de fuentes, pero ya no afectan a los vértices.

### 12.4 Enlace en C++

- `GPUMarchingCubes::setTerrainHeightTexture()` (unidad `GL_TEXTURE3`) guarda el handle externo y el footprint; `generateMesh()` sube los uniforms y enlaza la textura antes del dispatch. `release()` solo la desvincula.
- `main.cpp` enlaza la misma textura al shader del Fake Volume en la unidad `GL_TEXTURE5` mediante el helper Doxygen `bindTerrainHeightTexture()`.
- El cubo del Fake Volume ahora va de `terrain.minHeight()` a `surfaceY` (`waterHeight = surfaceY − minHeight`), no desde `boundsMin.y`.

### 12.5 Estado

- `make -j2` compila correctamente (C++17, `-Wall -Wextra -Werror -pedantic`). El GLSL se valida en runtime; no hay validador offline instalado.
- Riesgo conocido: `density_splat.comp` sigue limitando la inyección de densidad con `uSplatMaxHeight = surfaceY − boundsMin.y + 0.3`. Como el agua ya no se aplana, conviene revisar ese techo si aparecen costas cortadas en laderas altas; no se ha tocado en este paso.
- No incluido: espuma de costa en `meshFrag.frag` y transición blanda mediante depth prepass del terreno (sección 5.3).

---

## 13. Paso 4 implementado: migración a partículas reales

### 13.1 Eliminación del Fake Volume (`src/main.cpp`)

- Se eliminan `waterVolume`, `volumeShader`, `volumeMesh`, el helper `bindTerrainHeightTexture()` y todo el bloque de dibujo del cubo. Los archivos `volumeVertex.glsl` / `volumeFrag.frag` se dejan en el proyecto pero sin uso, para no borrar sus comentarios Doxygen.
- `surfaceY` se conserva como referencia visual del nivel de agua y ahora se deriva del número de partículas activas.
- `WaterRenderParams` sigue vivo porque lo consumen el shader de gotas y el render de Marching Cubes; ya no representa un cubo, solo parámetros de color/nivel.

### 13.2 Emisión y límite de partículas

- Constantes nuevas en `main.cpp`: `MAX_PARTICLES = 20000`, `RAIN_BATCH_SIZE = 250`, `RAIN_EMIT_INTERVAL = 0.1f` y `WATER_LEVEL_MARGIN = 0.5f`.
- En `setUpdate`, modo `Rain`, `emitAccumulator` dispara `gpuSPH->addParticles(lote, boundsMin, boundsMax)` cada 0.1 s. El lote se recorta a `MAX_PARTICLES - particleCount()`, así que nunca se excede la capacidad.
- `GPUSPH::init()` recibe ahora `MAX_PARTICLES` en lugar de `50000`.
- La API `addParticles(count, boundsMin, boundsMax)` no existía en el árbol de trabajo; se implementó en `GPUSPH.hpp` / `GPUSPH.cpp` con Doxygen en inglés. Escribe después del prefijo ya simulado, respeta la capacidad y marca las nuevas partículas como gotas para que la integración las asiente sobre el heightfield.
- `resetSim()` ya no reconstruye un volumen: en lluvia vacía el SSBO y deja que el emisor vuelva a llenarlo; en los otros modos sube las partículas CPU iniciales. `R` reinicia el acumulador de emisión.

### 13.3 Nivel visual y parámetros del splat

- Fórmula aplicada cada frame:

  `surfaceY = minHeight + (particleCount / MAX_PARTICLES) * (maxHeight - minHeight + WATER_LEVEL_MARGIN)`,

  con el ratio clampado a `[0, 1]`. Se reenvía a `marchingCubes->setWaterLevel(surfaceY)` y a `waterParams.surfaceY`.
- Límites de simulación confirmados: `boundsMin.y = minHeight - 0.2f`, `boundsMax.y = maxHeight + 1.0f`.
- `uSplatHeight` y `uSplatMaxHeight` ya no usan solo `surfaceY - boundsMin.y`. Se calculan sobre `max(surfaceY, terrain.maxHeight())`, de modo que las partículas asentadas en laderas altas siguen inyectando densidad.
- `setFillScale(1.0f + fill * 5.0f)` usa el mismo ratio de partículas que `surfaceY`, evitando desincronización entre nivel visual y grosor del metaball.

### 13.4 CSG y malla (verificación)

- `marching_cubes.comp` conserva el campo compuesto `d[i] = min(dWater - uIsoLevel, c[i].y - terrainH)`, `cubeIndex` con `d[i] >= 0.0` e `edgePosition(0.0, ...)`, con `clippedSurfaceNormal()` mezclando densidad y normal del terreno.
- `meshVertex.glsl` sigue sin `outPos.y = uWaterLevel + microWave`; la malla respeta la topografía real del agua acumulada.
- Marching Cubes se ejecuta solo si `particleCount() > 0`, sustituyendo el antiguo gate por volumen.
- `make -j2` compila correctamente (C++17, `-Wall -Wextra -Werror -pedantic`). El GLSL se valida en runtime; no hay validador offline instalado.

### 13.5 Estado

- Completado: migración a llenado por partículas, emisión limitada, nivel derivado de partículas y documentación.
- No incluido: validación visual en pantalla de la costa y ajuste fino de `splatWeight` tras observar el resultado; opcionalmente revisar `meshFrag.frag` (espuma) y la transición blanda con depth prepass (sección 5.3).

---

## 14. Paso 4.1 corregido: emisión dispersa y splatting de lluvia

### 14.1 Emisor (`src/main.cpp`, `GPUSPH::addParticles`)

- El lote pasa de 250 cada 0.1 s a `RAIN_BATCH_PER_FRAME = 20` **por frame**, con tope `MAX_PARTICLES = 20000`. Se eliminan `RAIN_EMIT_INTERVAL` y `emitAccumulator`.
- `GPUSPH::addParticles()` genera X e Z aleatorios dentro del footprint, pero usa `y = boundsMax.y - 0.1f` y velocidad fija `(0, -5, 0)`. El flag `flags.x = 1.0` (`rainDrop`) se conserva explícitamente.
- Con 20 gotas/frame el techo ya no se satura en un solo frame; las gotas caen como puntos discretos y se asientan vía colisión SPH contra el heightfield.
- `gcfg.rainAlpha = 1.0f` para que las gotas se dibujen opacas. `surfaceY` se mantiene porque ahora alimenta el filtro `uSurfaceY` del splat y el nivel de Marching Cubes; no hay cubo asociado.

### 14.2 Filtrado en `density_splat.comp`

- Nuevo uniform `uSurfaceY`, enlazado desde `GPUMarchingCubes::generateMesh()` usando `m_waterLevel` (el mismo valor que `setWaterLevel()` recibe desde `main.cpp`).
- Filtro crítico añadido: si `flags.x > 0.5` y `velAge.y < 0.0`, la partícula se descarta antes de cualquier atomicAdd. La integración limpia el flag al aterrizar; solo entonces la partícula puede aportar densidad.
- Se mantiene el filtro de peso (`weight <= 0`) y el de velocidad de caída (`vy < uFallSpeedCutoff`).
- Nueva restricción de altura: `if (p.y > uSurfaceY + 0.5) return;`. Ninguna partícula por encima de la superficie visual inyecta densidad, lo que elimina la burbuja que envolvía la montaña.

### 14.3 CSG en `marching_cubes.comp`

- La fórmula `d[i] = min(dWater - uIsoLevel, c[i].y - terrainH)` se conserva con `cubeIndex` usando `d[i] >= 0.0`.
- `terrainHeight()` ahora clampa el texel muestreado a `[uGridMin.y, uGridMax.y]`, de modo que un valor disparado de la R32F no rompe el signo del campo dentro de la montaña.
- Documentación Doxygen añadida: bajo el heightfield el término de terreno es negativo y `min()` excluye la esquina.
- `setIsoLevel()` baja de `1.05` a `0.5` para que solo las zonas con densidad alta de agua asentada generen superficie.

### 14.4 Estado

- No queda ninguna creación de `volumeMesh` / `volumeShader` / `waterVolume` en `main.cpp`. Render: terreno → gotas opacas → malla Marching Cubes.
- `make -j2` compila correctamente (C++17, `-Wall -Wextra -Werror -pedantic`). El GLSL se valida en runtime; no hay validador offline instalado.
- Pendiente: validación visual en pantalla para confirmar que no reaparece la losa superior ni la burbuja perimetral.

---

## 15. Paso 5 implementado: campo artificial y llenado controlado

### 15.1 Estado inicial y volumen (`src/main.cpp`)

- Modo inicial cambiado a `Particles::Mode::Rain`, `waterVolume = 0.0f` y `surfaceY = terrain.minHeight()`. Se elimina `st.water` y su `init(1000, ...)`; el SSBO de GPUSPH arranca vacío con `uploadParticles({})`.
- `RAIN_RATE = 0.1f`. En cada frame de lluvia: `waterVolume += dt * RAIN_RATE` y se emiten 20 gotas/frame para la visualización.
- `surfaceY = clamp(minHeight + waterVolume / areaXZ, minHeight, boundsMax.y - 0.1)`. Sustituye al cálculo anterior basado en `particleCount`.
- `setWaterLevel(surfaceY)` recibe el nuevo valor y `waterParams.surfaceY` se sincroniza para el shader de gotas.

### 15.2 Campo artificial (`marching_cubes.comp`)

- Nuevo uniform `uSurfaceY`, enlazado en `GPUMarchingCubes::generateMesh()` desde `m_waterLevel`.
- El campo de cada esquina pasa a ser `dWater = (uSurfaceY - c[i].y) * 10.0`, sin leer la textura R32UI de densidad de partículas.
- CSG conservado: `d[i] = min(dWater, c[i].y - terrainH)` con `cubeIndex` usando `d[i] >= 0.0`. La superficie libre es plana en `surfaceY` y el heightfield recorta la isla central.
- `density_splat.comp` ya no influye en la superficie; GPUSPH se mantiene solo para las gotas de lluvia y su colisión visual contra el terreno.

### 15.3 Render

- Las gotas se siguen dibujando con `gpuSPH->renderRainDrops`. `waterVertex.glsl` documenta y aplica el recorte `aPos.y < uSurfaceY` para ocultar gotas dentro del cuerpo de agua.
- Marching Cubes solo se genera/dibuja si `surfaceY > terrain.minHeight() + 0.01`, por lo que no aparece agua en el primer frame.
- No queda Fake Volume: render terreno → gotas opacas → superficie MC.

### 15.4 Estado

- `make -j2` compila correctamente (C++17, `-Wall -Wextra -Werror -pedantic`). El GLSL se valida en runtime; no hay validador offline instalado.
- Pendiente: validación visual del nivel de llenado y de la forma en que el agua rodea la montaña.

---

## 16. Paso 6 implementado: llenado híbrido controlado

### 16.1 Fin de GPUSPH en `main.cpp`

- Se elimina por completo `GPUSPH` de `main.cpp`: sin include, sin `Params`, sin `init()`, sin `update()`, sin `renderRainDrops()` y sin `setExternalParticleBuffer()`.
- El acumulador `waterVolume` sigue en `main.cpp`, pero ahora solo crece cuando una gota CPU toca el terreno: `waterVolume += DROP_VOLUME`.
- `st.surfaceY` se calcula como `minHeight + waterVolume / areaXZ`, clampado al rango de la caja; se envía a `setWaterLevel()` y al uniform `uSurfaceY` del campo artificial.

### 16.2 Gotas CPU (`src/main.cpp`)

- Nuevo `struct RainDrop { glm::vec3 position; float speedY; }`, con `MAX_RAIN_DROPS = 600` y `RAIN_BATCH_PER_FRAME = 20`.
- Spawn: X/Z aleatorios dentro del footprint, `y = boundsMax.y - 0.1f`, `speedY = -DROP_SPEED` (`5.0f`).
- Integración por frame: `speedY -= DROP_GRAVITY * dt` (`9.8f`) y `position.y += speedY * dt`.
- Al satisfacer `position.y <= terrain.heightAt(pos.xz)`, la gota se elimina y suma `DROP_VOLUME = 0.0015f`. La altura del terreno se consulta con un nuevo `Terrain::heightAt()` bilineal, coherente con la R32F que usan los shaders.
- Las gotas se suben a una `Mesh` de puntos con `uploadPoints()` / `updatePoints()` y se dibujan con `waterVertex.glsl` + `waterFrag.frag`, opacas (`uAlpha = 1.0`) y recortadas sobre `uSurfaceY`.

### 16.3 Campo artificial y render final

- `marching_cubes.comp` mantiene `uSurfaceY` y el campo `phi = min((uSurfaceY - y) * 10.0, y - terrainHeight(xz))`, con `cubeIndex` sobre `d[i] >= 0`.
- `GPUMarchingCubes::generateMesh()` publica `uSurfaceY` desde `m_waterLevel`; `setWaterLevel()` documenta que alimenta este campo y el recorte de gotas.
- Orden de dibujo: terreno → gotas CPU opacas → superficie Marching Cubes transparente. No se dibuja ningún Fake Volume.

### 16.4 Estado

- `make -j2` compila correctamente (C++17, `-Wall -Wextra -Werror -pedantic`). El GLSL se valida en runtime; no hay validador offline instalado.
- GPUSPH, `density_splat.comp` y la física SPH siguen compilando como código no usado por el render principal.
- Pendiente: validación visual del ritmo de llenado (`DROP_VOLUME`) y del contacto gota-terreno en laderas pronunciadas.

---

## 17. Paso 7 implementado: columna de agua volumétrica

### 17.1 Recorte lateral en Marching Cubes

- `marching_cubes.comp` ahora evalúa el campo artificial como una intersección de tres restricciones:
  - tapa superior: `waterTop = uSurfaceY - c[i].y`;
  - CSG contra el terreno: `aboveGround = c[i].y - terrainH`;
  - paredes laterales: `dx = min(c[i].x - uGridMin.x, uGridMax.x - c[i].x)` y `dz = min(c[i].z - uGridMin.z, uGridMax.z - c[i].z)`.
- El campo final es `d[i] = min(dWater, min(dx, dz))`, con `cubeIndex` sobre `d[i] >= 0.0`.
- El factor `* 10.0` usado antes en `dWater` se elimina para que la definición coincida exactamente con el campo firmado especificado; el corte en cero es equivalente y las interpolaciones de borde quedan lineales en el mismo espacio.
- Como `uGridMin/uGridMax` usan los mismos límites XZ que el footprint del terreno, las paredes aparecen exactamente en el borde de la caja de simulación.

### 17.2 Nivel del agua gradual

- `surfaceY` se inicializa en `terrain.minHeight()` y `resetSurfaceLevel()` restaura ese valor al reiniciar el volumen.
- Se elimina la recomputación global `minHeight + waterVolume / areaXZ` en cada frame.
- Cada gota que impacta hace `waterVolume += DROP_VOLUME` y, de forma incremental, `surfaceY += DROP_VOLUME / tankAreaXZ`, limitado por `boundsMax.y - 0.1f`.
- `main.cpp` sigue enviando el valor con `marchingCubes->setWaterLevel(st.surfaceY)` y al uniform `uSurfaceY` del shader de gotas.

### 17.3 Gotas bajo el agua

- `waterVertex.glsl` conserva el recorte `aPos.y < uSurfaceY` y mueve el vértice a `vec4(2.0, 2.0, 2.0, 1.0)` con `gl_PointSize = 0.0`.
- `main.cpp` continúa pasando `st.surfaceY` a `uSurfaceY` antes de `drawPoints()`, por lo que las gotas desaparecen al entrar en la columna.

### 17.4 Estado

- `make -j2` compila correctamente (C++17, `-Wall -Wextra -Werror -pedantic`). El GLSL se valida en runtime; no hay validador offline instalado.
- Pendiente: validación visual de la altura de las paredes laterales y del brillo en las caras verticales.

---

## 18. Paso 8 implementado: corrección del gradiente y del llenado

### 18.1 Gradiente fuerte y paredes laterales

- `marching_cubes.comp` restaura la pendiente pronunciada en el término de la tapa: `dWater = (uSurfaceY - c[i].y) * 20.0`.
- El recorte lateral usa ahora el footprint real del terreno (`uTerrainMinXZ`/`uTerrainMaxXZ`, ya disponibles en el shader) y también se escala por `20.0`, de modo que las caras verticales tengan la misma nitidez que la tapa.
- Campo compuesto final: `d[i] = min(dWater, min(aboveGround, min(dx * 20.0, dz * 20.0)))`, con `cubeIndex` sobre `d[i] >= 0.0`.
- Se conserva el comentario Doxygen anterior y se añade una nota indicando que la forma de gradiente fuerte sustituye a la forma sin escalar.

### 18.2 CSG contra el terreno

- `terrainHeight(c[i].xz)` se muestrea desde la textura R32F compartida con `Terrain`, usando `uTerrainMinXZ`/`uTerrainMaxXZ` como dominio y `uUseTerrainHeight` como activador.
- El valor se vuelve a clamp explícitamente a `[uGridMin.y, uGridMax.y]` antes de calcular `aboveGround = c[i].y - terrainH`.
- Con ese clamp, cualquier voxel dentro de la montaña tiene `aboveGround <= 0` y queda fuera del agua; el agua solo permanece por encima del heightfield.

### 18.3 Velocidad de llenado

- `DROP_VOLUME` baja de `0.0015f` a `0.00015f` (10 veces más lento por gota).
- `surfaceY` sigue incrementándose de forma gradual con `surfaceY += DROP_VOLUME / tankAreaXZ` al impactar cada gota, sin recomputaciones globales por frame.
- `st.marchingCubes->setWaterLevel(st.surfaceY)` y el uniform `uSurfaceY` del shader de gotas siguen recibiendo el mismo valor.
- `RAIN_BATCH_PER_FRAME` se mantiene en 20; si la validación visual sigue mostrando un llenado rápido, el siguiente ajuste es reducirlo a 5.

### 18.4 Estado

- `make -j2` compila correctamente (C++17, `-Wall -Wextra -Werror -pedantic`). El GLSL se valida en runtime; no hay validador offline instalado.
- Pendiente: validación visual de la columna cerrada y del ritmo de subida del nivel.

---

## 19. Paso 9 implementado: paredes reales y llenado controlado

### 19.1 Margen de pared lateral

- `marching_cubes.comp` introduce `wallMargin = 0.05` y calcula:
  - `dx = min(c[i].x - uGridMin.x - wallMargin, uGridMax.x - c[i].x - wallMargin)`;
  - `dz = min(c[i].z - uGridMin.z - wallMargin, uGridMax.z - c[i].z - wallMargin)`;
  - `dWalls = min(dx, dz)`.
- Con este margen, los corners situados dentro de los 5 cm del borde tienen campo lateral negativo; Marching Cubes detecta el cruce por cero y emite caras verticales reales en las cuatro paredes.
- Campo final: `d[i] = min(dWater, min(aboveGround, dWalls * 20.0))`, manteniendo `dWater = (uSurfaceY - c[i].y) * 20.0` y `cubeIndex` sobre `d[i] >= 0.0`.
- Se verifica que `main.cpp` inicializa `uGridMin.xz` y `uGridMax.xz` desde `terrain.footprintMin()` y `terrain.footprintMax()` a través de `GPUMarchingCubes::init(boundsMin, boundsMax, ...)`, por lo que el margen cae dentro del footprint del terreno.

### 19.2 Llenado controlado

- `RAIN_BATCH_PER_FRAME` baja de 20 a 2 gotas nuevas por frame.
- `DROP_VOLUME` baja de `0.00015f` a `0.00005f`.
- Cada gota que impacta se elimina inmediatamente con `it = st.drops.erase(it)`; no existe otro punto del código que sume `waterVolume`.
- `surfaceY` se recalcula en el mismo impacto con `clamp(terrain.minHeight() + waterVolume / tankAreaXZ, minHeight, boundsMax.y - 0.1f)` y se sincroniza con `waterParams.surfaceY`, `setWaterLevel()` y el uniform `uSurfaceY`.

### 19.3 Estado

- `make -j2` compila correctamente (C++17, `-Wall -Wextra -Werror -pedantic`). El GLSL se valida en runtime; no hay validador offline instalado.
- Pendiente: validación visual de las paredes a 5 cm del borde y del nuevo ritmo de llenado.
