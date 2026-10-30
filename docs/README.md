# Documentación de Mod1

Esta carpeta reúne la documentación de uso del proyecto: el sistema de
compilación, el orden de ejecución, las funciones públicas del wrapper y los
pasos específicos para compilar y ejecutar en Windows.

## Índice

- [Windows: instalación, compilación y ejecución](windows.md)
- [Makefile: variables, targets y flujo de compilación](makefile.md)
- [Orden de ejecución del programa](execution-order.md)
- [Wrapper core: Environment, WWindow e Input](wrapper-core.md)
- [Wrapper render: Shader, Mesh y Renderer](wrapper-render.md)
- [Wrapper scene: Transform, Camera, Entity y Scene](wrapper-scene.md)

## Estructura del proyecto

```text
Mod1/
├── Makefile
├── README.md
├── informe.txt
├── docs/
├── scripts/
│   ├── setup_glfw_linux.sh
│   ├── setup_glfw_mingw.sh
│   └── regenerate_glad.sh
├── src/
│   └── main.cpp
├── Wrapper/
│   ├── core/
│   │   ├── Environment.hpp/.cpp
│   │   ├── Input.hpp/.cpp
│   │   └── WWindow.hpp/.cpp
│   ├── math/
│   │   ├── Mat4.hpp
│   │   └── Vec3.hpp
│   ├── render/
│   │   ├── Mesh.hpp/.cpp
│   │   ├── Renderer.hpp/.cpp
│   │   └── Shader.hpp/.cpp
│   └── scene/
│       ├── Camera.hpp/.cpp
│       ├── Entity.hpp/.cpp
│       ├── Scene.hpp/.cpp
│       └── Transform.hpp/.cpp
└── external/
    ├── glad/
    ├── glm/
    └── glfw/          # se crea al ejecutar los scripts de GLFW
```

## Inicio rápido

### Linux

```sh
sudo apt update
sudo apt install build-essential pkg-config libglfw3-dev
make
./mod1
```

Si no está GLFW en el sistema:

```sh
./scripts/setup_glfw_linux.sh
make
```

### Windows

El orden importa. Usa una terminal **MSYS2 MinGW 64-bit**, no PowerShell ni
cmd.exe directamente:

```sh
pacman -Syu
pacman -S --needed make mingw-w64-x86_64-gcc unzip curl
cd /c/Users/Dannyel/Documents/GitHub/Mod1
./scripts/setup_glfw_mingw.sh
make re
./mod1.exe
```

La explicación completa está en [windows.md](windows.md).

## Reglas importantes

1. GLFW se inicializa al crear la primera `WWindow`.
2. GLAD se carga en `Environment::run()`, no antes.
3. No se puede llamar a funciones OpenGL antes de que exista un contexto
   current y GLAD esté cargado.
4. El bucle de eventos lo controla `Environment`; no llames a
   `glfwPollEvents()` ni a `WWindow::pollEvents()` dentro de los callbacks
   `update`/`render`.
5. El viewport debe usar el tamaño del **framebuffer**, no el tamaño lógico de
   la ventana.
6. Los recursos OpenGL deben destruirse con un contexto current. `Environment`
   deja el contexto de la primera ventana activo al terminar `run()`.
7. En Windows, el Makefile fuerza `SHELL := sh`; por eso hace falta MSYS2 o Git
   for Windows en el `PATH`.
8. No añadas `-I external/glm/gtx` al Makefile. Provoca que, en Windows,
   `#include <Transform.hpp>` resuelva al header experimental de GLM en lugar
   de `Wrapper/scene/Transform.hpp`.

## Ejemplo mínimo de ciclo de vida

```cpp
#include <Environment.hpp>
#include <WWindow.hpp>

int main()
{
    Environment env;

    WWindow::Config cfg;
    cfg.title = "Ejemplo";
    cfg.width = 800;
    cfg.height = 600;
    env.addWindow(cfg);

    env.setInit([](WWindow&) {
        // Aquí ya hay contexto current y GLAD cargado.
    });

    env.setUpdate([](WWindow& w, float dt) {
        if (w.input().isKeyPressed(Key::Code::Escape))
            w.requestClose();
    });

    env.setRender([](WWindow& w) {
        // Aquí el contexto de w ya está current.
        // Solo dibujar. Environment hará swapBuffers después.
    });

    env.run();
}
```

## Convenciones de includes

El proyecto incluye las cabeceras del wrapper por nombre:

```cpp
#include <Camera.hpp>
#include <Entity.hpp>
#include <Environment.hpp>
#include <Input.hpp>
#include <Mesh.hpp>
#include <Renderer.hpp>
#include <Shader.hpp>
#include <WWindow.hpp>
#include <scene/Transform.hpp>
```

`Transform.hpp` se incluye con ruta explícita para evitar colisiones de nombre
con GLM en Windows. Si añades cabeceras nuevas, evita nombres que también
existan en GLM o usa rutas relativas/explícitas.
