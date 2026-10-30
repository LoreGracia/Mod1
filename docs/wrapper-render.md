# Wrapper render: Shader, Mesh y Renderer

El módulo `Wrapper/render` contiene las clases que envuelven objetos OpenGL. Sus
métodos deben llamarse con un contexto OpenGL current y con GLAD ya cargado.

## 1. Shader

`Shader` envuelve un programa de OpenGL.

```cpp
#include <Shader.hpp>
```

### 1.1. Características

- Es movible, pero no copiable.
- El destructor llama a `glDeleteProgram` si hay un programa.
- `loadFromSource` y `loadFromFile` devuelven `false` si falla la compilación o
  el enlace.
- Los errores se escriben en `std::cerr`.
- Cachea las localizaciones de uniforms, incluido `-1` si el uniform no existe.
- `use()` debe llamarse antes de los setters de uniforms, porque `glUniform*`
  modifica el programa actualmente en uso.

### 1.2. Métodos

| Método | Descripción |
|---|---|
| `bool loadFromSource(vert, frag)` | Compila y enlaza desde código fuente. |
| `bool loadFromFile(vertPath, fragPath)` | Lee los archivos y llama a `loadFromSource`. |
| `use()` | Activa el programa con `glUseProgram`. |
| `id()` | Devuelve el `GLuint` del programa. |
| `setInt(name, v)` | `glUniform1i`. |
| `setFloat(name, v)` | `glUniform1f`. |
| `setVec3(name, v)` | `glUniform3fv`. |
| `setVec4(name, v)` | `glUniform4fv`. |
| `setMat4(name, m)` | `glUniformMatrix4fv` sin transponer. |

### 1.3. Ejemplo

```cpp
auto shader = std::make_shared<Shader>();

bool ok = shader->loadFromSource(
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "uniform mat4 uModel;\n"
    "uniform mat4 uViewProj;\n"
    "void main(){ gl_Position = uViewProj * uModel * vec4(aPos, 1.0); }\n",
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main(){ FragColor = vec4(1.0); }\n");

if (!ok)
    throw std::runtime_error("shader failed");

shader->use();
shader->setMat4("uModel", model);
shader->setMat4("uViewProj", viewProj);
```

`loadFromSource` puede usarse para recargar un shader. Si falla la
**compilación**, el programa anterior se conserva. Si la compilación tiene éxito
pero falla el **enlace**, el programa anterior ya se ha borrado y el shader queda
sin programa válido. Revisa siempre el valor de retorno.

## 2. Mesh

`Mesh` envuelve VAO, VBO y EBO.

```cpp
#include <Mesh.hpp>
```

### 2.1. Características

- Es movible, pero no copiable.
- El destructor libera VAO, VBO y EBO.
- `upload` reemplaza los buffers anteriores.
- Los índices son `uint32_t`.
- `draw` usa `glDrawElements` con `GL_UNSIGNED_INT`.

### 2.2. Formatos de vértice

Solo posiciones:

```cpp
mesh.upload(positions, indices, GL_TRIANGLES);
```

- Atributo 0: `vec3` posición.
- Stride: `3 * sizeof(float)`.
- No se activa ningún atributo de color.

Posiciones + colores:

```cpp
mesh.upload(positions, colors, indices, GL_TRIANGLES);
```

- Atributo 0: `vec3` posición.
- Atributo 1: `vec3` color.
- Stride: `6 * sizeof(float)`.
- Posiciones y colores deben tener la misma cantidad de vértices. El código
  calcula `positions.size() / 3` y asume que `colors` tiene al menos esa
  cantidad de componentes.

### 2.3. Métodos

| Método | Descripción |
|---|---|
| `upload(vertices, indices, mode)` | Sube posiciones e índices. |
| `upload(positions, colors, indices, mode)` | Sube posiciones y colores intercalados. |
| `draw()` | Dibuja el mesh. |
| `static Mesh triangle()` | Devuelve un mesh predefinido. Ver advertencia. |
| `static Mesh cube()` | Declarado en la cabecera, **sin implementación actual**. |
| `static Mesh plane(float size = 1.f)` | Plano en el eje XZ. |

Advertencias:

- `Mesh::triangle()` no genera un triángulo. Su implementación actual sube 8
  vértices y 36 índices, es decir, un cubo de 6 caras. El nombre es engañoso.
- `Mesh::cube()` está declarado pero no definido en `Mesh.cpp`. Llamarlo produce
  un error de enlace. Mientras no se implemente, usa `Mesh::triangle()`,
  `Mesh::plane()` o `upload()`.
- `Mesh::plane(size)` crea un plano horizontal de tamaño `size` centrado en el
  origen.

### 2.4. Ejemplo

```cpp
std::vector<float> positions = {
    -0.5f, -0.5f, 0.5f,
     0.5f, -0.5f, 0.5f,
     0.5f,  0.5f, 0.5f,
};
std::vector<uint32_t> indices = {0, 1, 2};

auto mesh = std::make_shared<Mesh>();
mesh->upload(positions, indices);
mesh->draw();
```

## 3. Renderer

`Renderer` centraliza el estado de render y evita llamadas redundantes a
OpenGL.

```cpp
#include <Renderer.hpp>
```

### 3.1. Estado inicial

El constructor de `Renderer` no llama a OpenGL. Asume el estado inicial de un
contexto OpenGL recién creado, pero guarda estos valores lógicos:

| Estado | Valor inicial |
|---|---|
| Viewport | desconocido (`-1`, `-1`) |
| Color de borrado | desconocido |
| `GL_DEPTH_TEST` | desactivado |
| `glDepthMask` | activado |
| `glDepthFunc` | `GL_LESS` |
| `GL_CULL_FACE` | desactivado |
| `glCullFace` | `GL_BACK` |
| `glFrontFace` | `GL_CCW` |
| `GL_BLEND` | desactivado |
| `glBlendFunc` | `GL_ONE`, `GL_ZERO` |

Si otro código cambió el estado de OpenGL, el `Renderer` puede tener una caché
desactualizada. En ese caso llama a los métodos con los valores deseados o a
`syncToDefaults()`.

### 3.2. Métodos

| Método | Descripción |
|---|---|
| `setViewport(w, h)` | Aplica `glViewport` solo si cambia. |
| `setClearColor(r, g, b, a)` | Aplica `glClearColor` solo si cambia. |
| `clear(color, depth, stencil)` | Limpia los buffers indicados. |
| `depthTest(on)` | Activa/desactiva `GL_DEPTH_TEST`. |
| `depthWrite(on)` | Cambia `glDepthMask`. |
| `depthFunc(func)` | Cambia `glDepthFunc`. |
| `cullFace(on)` | Activa/desactiva `GL_CULL_FACE`. |
| `cullMode(mode)` | Cambia `glCullFace`. |
| `frontFace(winding)` | Cambia `glFrontFace`. |
| `blend(on)` | Activa/desactiva `GL_BLEND`. |
| `blendFunc(src, dst)` | Cambia `glBlendFunc`. |
| `syncToDefaults()` | Invalida viewport/color y fuerza a reaplicar el estado actual. |

`syncToDefaults` no restaura los valores por defecto de OpenGL a pesar del
nombre. Lo que hace es invalidar la caché de viewport y color, y alternar
internamente los flags para obligar a `depthTest`, `cullFace` y `blend` a
reaplicar el estado lógico actual. No reaplica `depthWrite`, `depthFunc`,
`cullMode`, `frontFace` ni `blendFunc`; para esos, llama al setter correspondiente.

### 3.3. Orden recomendado

En `setInit`:

```cpp
renderer.depthTest(true);
renderer.depthFunc(GL_LESS);
renderer.depthWrite(true);
renderer.cullFace(true);
renderer.cullMode(GL_BACK);
renderer.frontFace(GL_CCW);
renderer.blend(false);
```

En `setRender`:

```cpp
int rw, rh;
if (w.consumeResize(rw, rh))
    renderer.setViewport(rw, rh);
else
    renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());

renderer.setClearColor(0.10f, 0.15f, 0.20f, 1.f);
renderer.clear(true, true, false);

// Dibujar...
```

`clear` no forma parte del constructor; hay que llamarlo cada frame antes de
dibujar.
