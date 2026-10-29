# Wrapper core: Environment, WWindow e Input

El módulo `Wrapper/core` contiene la capa de plataforma. Todo el estado de
ventanas, eventos e input pasa por GLFW.

## 1. Environment

`Environment` es el dueño de las ventanas y del bucle principal.

```cpp
#include <Environment.hpp>
```

### 1.1. Tipos de callback

```cpp
using InitCallback   = std::function<void(WWindow&)>;
using UpdateCallback = std::function<void(WWindow&, float)>;
using RenderCallback = std::function<void(WWindow&)>;
```

- `InitCallback`: se ejecuta una vez por ventana, con su contexto current y GLAD
  ya cargado.
- `UpdateCallback`: se ejecuta por ventana y frame, antes del render. Recibe
  `dt` en segundos.
- `RenderCallback`: se ejecuta por ventana y frame, después de `update` y antes
  del swap.

### 1.2. Métodos

| Método | Descripción |
|---|---|
| `WWindow& addWindow(WWindow::Config& cfg)` | Crea y posee una ventana. Devuelve una referencia válida mientras viva el `Environment`. |
| `setInit(InitCallback)` | Registra el callback de inicialización. |
| `setUpdate(UpdateCallback)` | Registra el callback de actualización. |
| `setRender(RenderCallback)` | Registra el callback de render. |
| `run()` | Carga GLAD, inicializa ventanas y entra en el bucle. |
| `stop()` | Solicita terminar el bucle. Es seguro desde otro hilo. |
| `track(std::shared_ptr<T>)` | Guarda una referencia hasta la limpieza final. Devuelve el mismo puntero. |

`addWindow` no se puede llamar después de que `run()` haya empezado; lanza
`std::runtime_error`. `run()` no está diseñado para llamarse dos veces.

`addWindow` modifica `cfg.shareContext`: si es `nullptr` y ya existe una
ventana, lo apunta a la primera ventana para compartir contexto. Si reutilizas
el mismo `Config`, ten en cuenta ese cambio.

### 1.3. Ejemplo

```cpp
Environment env;

WWindow::Config cfg;
cfg.title = "Demo";
cfg.width = 800;
cfg.height = 600;
WWindow& win = env.addWindow(cfg);

env.setInit([&](WWindow& w) {
    // Contexto current + GLAD cargado.
});

env.setUpdate([&](WWindow& w, float dt) {
    if (w.input().isKeyPressed(Key::Code::Escape))
        w.requestClose();
});

env.setRender([&](WWindow& w) {
    // Solo dibujar. Environment hará swapBuffers.
});

env.run();
```

### 1.4. Recursos GPU

`track` sirve para que un recurso OpenGL viva hasta que `Environment` limpie
con contexto current:

```cpp
auto shader = env.track(std::make_shared<Shader>());
```

No es obligatorio para recursos locales de `main`, porque `run()` deja el
contexto de la primera ventana activo al regresar. Es útil si el recurso se crea
en un callback o si quieres controlar explícitamente su vida hasta el final.
Declara `Environment` antes que esos recursos: las variables locales se destruyen
en orden inverso al de declaración.

## 2. WWindow

`WWindow` encapsula una ventana GLFW y su contexto OpenGL.

```cpp
#include <WWindow.hpp>
```

### 2.1. `WWindow::Config`

| Campo | Tipo | Valor por defecto | Descripción |
|---|---|---|---|
| `title` | `std::string` | `"Default Name"` | Título de la ventana. |
| `width` | `int` | `800` | Ancho lógico. |
| `height` | `int` | `600` | Alto lógico. |
| `x` | `int` | `0` | Posición X. |
| `y` | `int` | `0` | Posición Y. |
| `shareContext` | `WWindow*` | `nullptr` | Ventana con la que compartir contexto. |
| `glMajor` | `int` | `3` | Versión mayor de OpenGL. |
| `glMinor` | `int` | `3` | Versión menor de OpenGL. |
| `coreProfile` | `bool` | `true` | Perfil core. |
| `resizable` | `bool` | `true` | Ventana redimensionable. |
| `visible` | `bool` | `true` | Ventana visible. |
| `decorated` | `bool` | `true` | Borde y decoración. |
| `vsync` | `bool` | `true` | Sincronización vertical. |
| `samples` | `int` | `0` | Muestras MSAA. |

Si `x == 0 && y == 0`, no se llama a `glfwSetWindowPos`. Esto evita el error de
Wayland que no soporta posicionar ventanas. No es posible pedir explícitamente
la posición `(0, 0)` con este `Config`.

El perfil se aplica solo para OpenGL 3.2 o superior. Para versiones anteriores,
GLFW no recibe el hint de perfil.

### 2.2. Métodos

| Método | Descripción |
|---|---|
| `makeCurrent()` | Activa el contexto de esta ventana. |
| `releaseCurrent()` | Desactiva el contexto actual. |
| `swapBuffers()` | Intercambia los buffers de la ventana. |
| `pollEvents()` | Prepara el frame de input, hace `glfwPollEvents` y devuelve `!shouldClose()`. |
| `pollEventsAll()` | Solo hace `glfwPollEvents` (uso interno de `Environment`). |
| `waitEvents(double)` | Espera eventos con timeout. |
| `waitEvents()` | Espera eventos indefinidamente. |
| `setSwapInterval(int)` | Cambia VSync (`0` off, `1` on). |
| `nativeWindow()` | Devuelve `GLFWwindow*`. |
| `width()` / `height()` | Tamaño lógico. |
| `framebufferWidth()` / `framebufferHeight()` | Tamaño real en píxeles. |
| `aspect()` | `width / height`. |
| `shouldClose()` | `true` si la ventana debe cerrarse. |
| `requestClose()` | Solicita el cierre. |
| `consumeResize(int&, int&)` | Devuelve una vez el nuevo tamaño de framebuffer. |
| `input()` | Devuelve el `Input` de la ventana. |

`WWindow` no es copiable ni movible. `Environment` la posee mediante
`std::unique_ptr`.

### 2.3. Tamaño lógico y framebuffer

En pantallas HiDPI, el tamaño lógico y el framebuffer pueden ser distintos:

- `width()` / `height()`: tamaño de la ventana en unidades lógicas.
- `framebufferWidth()` / `framebufferHeight()`: píxeles reales.
- Usa el framebuffer para `glViewport`.
- Usa `aspect()` para la cámara.

Patrón recomendado en render:

```cpp
int rw, rh;
if (w.consumeResize(rw, rh))
    renderer.setViewport(rw, rh);
else
    renderer.setViewport(w.framebufferWidth(), w.framebufferHeight());
```

`consumeResize` devuelve `false` si no hubo cambio. En el primer frame suele ser
`false`, así que se usa el tamaño actual del framebuffer.

### 2.4. Cierre

`Escape` no cierra automáticamente. Debes hacerlo en `update`:

```cpp
if (w.input().isKeyPressed(Key::Code::Escape))
    w.requestClose();
```

`shouldClose` consulta tanto el flag atómico interno como
`glfwWindowShouldClose`.

## 3. Input

`Input` mantiene el estado de teclado, ratón, scroll y foco de una ventana.

```cpp
#include <Input.hpp>
```

### 3.1. Teclas

```cpp
Key::Code::W
Key::Code::Escape
Key::Code::LeftShift
```

`Key::Code` replica los valores de GLFW. Los valores no reconocidos se convierten
en `Key::Code::Unknown`.

Grupos principales:

- Letras `A`–`Z`.
- Dígitos `Num0`–`Num9`.
- Signos: `Space`, `Apostrophe`, `Comma`, `Minus`, `Period`, `Slash`,
  `Semicolon`, `Equal`, `LeftBracket`, `Backslash`, `RightBracket`,
  `GraveAccent`.
- Teclas especiales: `Escape`, `Enter`, `Tab`, `Backspace`, `Insert`, `Delete`,
  flechas, `PageUp`, `PageDown`, `Home`, `End`.
- Bloqueo: `CapsLock`, `ScrollLock`, `NumLock`, `PrintScreen`, `Pause`.
- Función: `F1`–`F25`.
- Teclado numérico: `Keypad0`–`Keypad9`, `KeypadDecimal`, `KeypadDivide`,
  `KeypadMultiply`, `KeypadSubtract`, `KeypadAdd`, `KeypadEnter`, `KeypadEqual`.
- Modificadores: `LeftShift`, `LeftControl`, `LeftAlt`, `LeftSuper`,
  `RightShift`, `RightControl`, `RightAlt`, `RightSuper`, `Menu`.

La lista completa está en `Wrapper/core/Input.hpp`.

### 3.2. Botones del ratón

```cpp
Mouse::Button::Left
Mouse::Button::Right
Mouse::Button::Middle
Mouse::Button::X1
Mouse::Button::X2
Mouse::Button::Button6
Mouse::Button::Button7
Mouse::Button::Button8
```

### 3.3. Métodos

| Método | Descripción |
|---|---|
| `beginFrame()` | Limpia flancos, delta de ratón y scroll. |
| `endFrame()` | Llama a `beginFrame()`. |
| `reset()` | Limpia todo el estado, incluido el ratón. |
| `isKeyDown(key)` | Tecla mantenida. |
| `isKeyPressed(key)` | Tecla pulsada en este frame. |
| `isKeyReleased(key)` | Tecla soltada en este frame. |
| `isMouseButtonDown(button)` | Botón mantenido. |
| `isMouseButtonPressed(button)` | Botón pulsado en este frame. |
| `isMouseButtonReleased(button)` | Botón soltado en este frame. |
| `mouseX()` / `mouseY()` | Posición actual del cursor. |
| `mouseDeltaX()` / `mouseDeltaY()` | Movimiento acumulado en el frame. |
| `scrollDelta()` | Scroll vertical acumulado. |
| `scrollDeltaX()` / `scrollDeltaY()` | Scroll acumulado por eje. |

### 3.4. Semántica de frames

`Environment` hace por ti:

```text
beginFrame() -> pollEvents() -> update/render -> endFrame()
```

Por eso:

- `isKeyPressed` es cierto solo en el frame en el que se pulsó.
- `isKeyReleased` es cierto solo en el frame en el que se soltó.
- `isKeyDown` sigue siendo cierto mientras la tecla esté mantenida.
- Las repeticiones de teclado de GLFW se ignoran.
- El primer movimiento del ratón no genera delta.
- Al perder el foco se resetea el input para evitar teclas atascadas.
- Al recuperar el foco se vuelve a ignorar el primer movimiento.

### 3.5. Ejemplo

```cpp
env.setUpdate([](WWindow& w, float dt) {
    Input& in = w.input();

    glm::vec3 move{0.f};
    if (in.isKeyDown(Key::Code::W))
        move.y += 1.f;
    if (in.isKeyDown(Key::Code::S))
        move.y -= 1.f;

    if (in.isMouseButtonDown(Mouse::Button::Right)) {
        float dx = static_cast<float>(in.mouseDeltaX());
        float dy = static_cast<float>(in.mouseDeltaY());
        // usar dx/dy
    }

    if (in.isKeyPressed(Key::Code::Space))
        std::printf("Salto\n");

    if (in.isKeyPressed(Key::Code::Escape))
        w.requestClose();
});
```
