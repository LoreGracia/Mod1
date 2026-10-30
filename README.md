# Mod1

This project is about creating a terrestrial simulation followed by a hydrodynamic one.

## Documentation

Detailed documentation is available in `docs/`:

- `docs/windows.md`: Windows setup, build order and troubleshooting.
- `docs/makefile.md`: Makefile variables, targets and build flow.
- `docs/execution-order.md`: runtime initialization and frame order.
- `docs/wrapper-core.md`: `Environment`, `WWindow` and `Input`.
- `docs/wrapper-render.md`: `Shader`, `Mesh` and `Renderer`.
- `docs/wrapper-scene.md`: `Transform`, `Camera`, `Entity`, `Scene` and math.

## Dependencies

- C++17 compiler and `make`
- GLFW 3.3 or newer (window, OpenGL context, events and input)
- GLAD 2.x generated for `gl:core=3.3` (vendored in `external/glad`)
- GLM (vendored in `external/glm`)

### Linux (Debian/Ubuntu)

```sh
sudo apt update
sudo apt install build-essential pkg-config libglfw3-dev
pkg-config --modversion glfw3
make
```

If `pkg-config` does not find GLFW, `scripts/setup_glfw_linux.sh` can build and
install it under `external/glfw`:

```sh
./scripts/setup_glfw_linux.sh
make
```

### Windows (MinGW-w64 / MSYS2)

The Makefile recipes use POSIX commands, so it sets `SHELL := sh`. Run the build
from an MSYS2 MinGW-w64 shell, or make sure `sh.exe` (MSYS2 or Git for Windows)
is available in `PATH`. Plain PowerShell/cmd.exe is not enough by itself.

Vendor the official GLFW binaries in `external/glfw`:

```sh
./scripts/setup_glfw_mingw.sh
make
```

To build GLFW from source with CMake instead:

```sh
./scripts/setup_glfw_mingw.sh --source
make
```

`make` from MSYS2 is enough. If you prefer `mingw32-make`, install it with
`pacman -S --needed mingw-w64-x86_64-make`.

The Makefile expects:

- Headers: `external/glfw/include/GLFW/glfw3.h`
- Libraries: `external/glfw/lib-mingw-w64`
- Link flags: `-lglfw3 -lopengl32 -lgdi32`

## GLAD

Only `external/glad/include/glad/gl.h`, `external/glad/include/KHR/khrplatform.h`
and `external/glad/src/gl.c` are used. GLX/WGL loaders are not part of the build.

To regenerate GLAD with the same configuration:

```sh
./scripts/regenerate_glad.sh
```

Equivalent web configuration at <https://glad.sh>:

- API: `gl:core=3.3`
- Profile: `core`
- Extensions: none
- Integrated loader: disabled (GLFW provides `glfwGetProcAddress`)

Every translation unit that includes GLFW must define `GLFW_INCLUDE_NONE`
before `<GLFW/glfw3.h>` and include `<glad/gl.h>` for OpenGL.

## Build targets

```sh
make            # build mod1
make re         # clean and rebuild
make fclean     # remove objects and binary
make help       # list available targets
```
