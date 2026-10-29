# Makefile: guía detallada

El `Makefile` compila la aplicación `mod1`, compila GLAD y enlaza GLFW. Está
preparado para Linux y Windows con MinGW-w64.

## 1. Detección de plataforma

```make
ifneq (,$(filter Windows_NT,$(OS)))
    PLATFORM := WINDOWS
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        PLATFORM := LINUX
    else
        PLATFORM := UNIX_UNKNOWN
    endif
endif
```

- En Windows, la variable de entorno `OS` vale `Windows_NT`.
- En Linux se usa `uname -s`.
- Cualquier otro sistema produce `Unsupported platform`.

En Windows se fuerza una shell POSIX:

```make
ifeq ($(PLATFORM),WINDOWS)
    SHELL := sh
    .SHELLFLAGS := -c
endif
```

Esto es obligatorio porque las recetas usan `printf`, `rm`, `mkdir -p`,
`mktemp`, `[` y `command -v`. Sin `sh.exe`, el Makefile no puede ejecutar sus
recetas en Windows. La shell debe estar en el `PATH`.

## 2. Variables principales

| Variable | Valor por defecto | Uso |
|---|---|---|
| `NAME` | `mod1` | Nombre del ejecutable. |
| `SRC_DIR` | `src` | Directorio de `main.cpp`. |
| `OBJ_DIR` | `.obj` | Objetos compilados. |
| `DEP_DIR` | `.dep` | Dependencias `.d`. |
| `EXTER_DIR` | `external` | Dependencias externas. |
| `GLDIR` | `external/glad/src` | Fuente de GLAD. |
| `GLFW_VENDOR_DIR` | `external/glfw` | GLFW vendorizado. |
| `WRAPPER_DIR` | `Wrapper` | Código del wrapper. |
| `CORE_DIR` | `Wrapper/core` | `WWindow`, `Input`, `Environment`. |
| `MATH_DIR` | `Wrapper/math` | `Vec3`, `Mat4`. |
| `RENDER_DIR` | `Wrapper/render` | `Shader`, `Mesh`, `Renderer`. |
| `SCENE_DIR` | `Wrapper/scene` | `Camera`, `Transform`, `Entity`, `Scene`. |
| `SOURCES` | `main.cpp` | Fuentes de la aplicación. |
| `GLSOURCES` | `external/glad/src/gl.c` | Fuente C de GLAD. |
| `WRAPSOURCES` | lista de `.cpp` | Fuentes del wrapper. |
| `INCLUDES` | rutas `-I...` | GLM, GLAD, GLFW y `Wrapper`. |
| `CXX` | `g++` | Compilador C++. |
| `CC` | `gcc` | Compilador C. |
| `CXXFLAGS` | C++17 + warnings | Flags de C++. |
| `CFLAGS` | `-std=c99 -O2` | Flags de C. |
| `LDLIBS` | según plataforma | Bibliotecas de enlace. |

### Variables de personalización

| Variable | Ejemplo | Efecto |
|---|---|---|
| `DEBUG` | `make DEBUG=1` | Añade `-DDEBUG=1`. |
| `SANITIZE` | `make SANITIZE=1` | Añade AddressSanitizer en Linux. |
| `CHECK` | `make CHECK=1` | Hace que `mod1` dependa de `check_requirements` también fuera de `all`. |
| `GLFW_CFLAGS` | `make GLFW_CFLAGS=...` | Sustituye los flags de compilación de GLFW. |
| `GLFW_LIBS` | `make GLFW_LIBS=...` | Sustituye los flags de enlace de GLFW. |
| `LINT_DIRS` | `make lint LINT_DIRS="src Wrapper"` | Directorios para clang-format/clang-tidy. |

`GLFW_VERSION` y `GLFW_ARCH` son variables de entorno leídas por los scripts de
GLFW, no variables internas del Makefile. Por ejemplo:

```sh
GLFW_VERSION=3.4 ./scripts/setup_glfw_mingw.sh
GLFW_ARCH=32 ./scripts/setup_glfw_mingw.sh
```

## 3. Includes

```make
INCLUDES := $(addprefix -I, ./external/glm)
INCLUDES += $(addprefix -I, ./external/glm/detail)
INCLUDES += $(addprefix -I, ./external/glm/ext)
INCLUDES += $(addprefix -I, ./external/glm/gtc)
INCLUDES += $(addprefix -I, ./external/glm/simd)
INCLUDES += $(addprefix -I, ./external/glad/include/)
INCLUDES += $(addprefix -I, ./$(GLFW_VENDOR_DIR)/include)
INCLUDES += $(addprefix -I, ./Wrapper)
INCLUDES += $(addprefix -I, ./Wrapper/core)
INCLUDES += $(addprefix -I, ./Wrapper/math)
INCLUDES += $(addprefix -I, ./Wrapper/render)
INCLUDES += $(addprefix -I, ./Wrapper/scene)
```

Notas:

- `external/glm/gtx` **no** se incluye. En Windows, el sistema de archivos no
  distingue mayúsculas y `#include <Transform.hpp>` podía resolver a
  `external/glm/gtx/Transform.hpp` en lugar de `Wrapper/scene/Transform.hpp`.
- Las cabeceras del proyecto se incluyen por nombre gracias a las rutas
  `-I Wrapper/...`.
- `Transform.hpp` se incluye de forma explícita en el código para evitar
  ambigüedades.

## 4. Plataforma Linux

```make
ifeq ($(PLATFORM),LINUX)
    CXX      := g++
    CC       := gcc
    CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -g -pedantic
    CFLAGS   := -std=c99 -O2

    GLFW_PKG_CONFIG_PATH := $(GLFW_VENDOR_DIR)/lib/pkgconfig

    ifeq ($(strip $(GLFW_CFLAGS)),)
        GLFW_CFLAGS := $(shell PKG_CONFIG_PATH="$(GLFW_PKG_CONFIG_PATH):$${PKG_CONFIG_PATH:-}" pkg-config --cflags glfw3 2>/dev/null)
    endif

    ifeq ($(strip $(GLFW_LIBS)),)
        GLFW_LIBS := $(shell PKG_CONFIG_PATH="$(GLFW_PKG_CONFIG_PATH):$${PKG_CONFIG_PATH:-}" pkg-config --libs glfw3 2>/dev/null)
    endif

    LDLIBS := $(GLFW_LIBS)
endif
```

Orden de búsqueda de GLFW en Linux:

1. `external/glfw/lib/pkgconfig/glfw3.pc` mediante `PKG_CONFIG_PATH`.
2. El `PKG_CONFIG_PATH` que ya tuviera el usuario.
3. El GLFW instalado en el sistema.

Si `GLFW_CFLAGS` o `GLFW_LIBS` se pasan por línea de comandos, se respetan y no
se consulta `pkg-config` para esa parte.

## 5. Plataforma Windows

```make
else ifeq ($(PLATFORM),WINDOWS)
    CXX      := g++
    CC       := gcc
    CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -g -pedantic
    CFLAGS   := -std=c99 -O2

    GLFW_CFLAGS ?= -I$(GLFW_VENDOR_DIR)/include
    GLFW_LIBS   ?= -L$(GLFW_VENDOR_DIR)/lib-mingw-w64 -lglfw3 -lopengl32 -lgdi32
    LDLIBS      := $(GLFW_LIBS)
endif
```

Windows no usa `pkg-config` por defecto. Usa los binarios vendorizados:

```text
external/glfw/include/GLFW/glfw3.h
external/glfw/lib-mingw-w64/libglfw3.a
```

Enlace:

```text
-Lexternal/glfw/lib-mingw-w64 -lglfw3 -lopengl32 -lgdi32
```

`-lglfw3` enlaza la biblioteca estática `libglfw3.a`. No se necesita
`glfw3.dll` con esta configuración.

## 6. Requisitos

```make
ifeq ($(PLATFORM),LINUX)
    REQUIRED_TOOLS := g++ gcc make ld pkg-config
else
    REQUIRED_TOOLS := g++ gcc ld
endif
```

En Windows no se comprueba `make` porque el propio Makefile ya se está
ejecutando con él. Así funciona tanto con `make` como con `mingw32-make`.

`check_requirements` hace:

1. Comprueba cada herramienta con `command -v`.
2. En Linux, comprueba `pkg-config --exists glfw3` o las cabeceras vendorizadas.
3. En Windows, comprueba las cabeceras y las bibliotecas de MinGW.
4. Compila y enlaza una sonda que incluye GLFW + GLAD (no llama funciones).
5. Limpia el directorio temporal.

La sonda usa un directorio temporal con `mktemp`, por lo que también necesita
una shell POSIX en Windows.

## 7. Targets

| Target | Descripción |
|---|---|
| `all` | Target por defecto. Ejecuta `check_requirements` y compila `mod1`. |
| `mod1` | Enlaza el ejecutable. |
| `check_requirements` | Verifica herramientas, GLFW, GLAD y una compilación de prueba. |
| `help` | Lista los targets documentados. |
| `glfw-linux` | Ejecuta `scripts/setup_glfw_linux.sh`. |
| `glfw-mingw` | Ejecuta `scripts/setup_glfw_mingw.sh`. |
| `clean` | Elimina `.obj` y `.dep`. |
| `fclean` | Ejecuta `clean` y elimina `mod1`. |
| `re` | Ejecuta `fclean` y después `all`. |
| `lint-build` | Construye la imagen Docker de lint. |
| `lint` | Ejecuta clang-format y clang-tidy en modo lectura. |
| `lint-summary` | Ejecuta el linter en modo resumen. |
| `lint-fix` | Aplica `clang-format -i`. |
| `lint-shell` | Abre una shell en el contenedor de lint. |
| `lint-clean` | Elimina la imagen y contenedores de lint. |

`re` está definido como:

```make
re: fclean all
```

Por eso siempre hace una compilación limpia.

## 8. Flujo de compilación

```text
make
  └── all
      ├── check_requirements
      └── mod1
          ├── .obj/main.o
          ├── .obj/external/glad/src/gl.o
          ├── .obj/Wrapper/core/WWindow.o
          ├── .obj/Wrapper/core/Environment.o
          ├── .obj/Wrapper/core/Input.o
          ├── .obj/Wrapper/render/Shader.o
          ├── .obj/Wrapper/render/Mesh.o
          ├── .obj/Wrapper/render/Renderer.o
          ├── .obj/Wrapper/scene/Camera.o
          ├── .obj/Wrapper/scene/Transform.o
          ├── .obj/Wrapper/scene/Entity.o
          └── .obj/Wrapper/scene/Scene.o
```

Los directorios `.obj` y `.dep` se crean con reglas de directorio y
`create_dirs`. Los objetos usan `-MMD -MF` para generar dependencias de
cabeceras; al final del Makefile se incluyen:

```make
-include $(DEPS)
-include $(GLDEP)
-include $(WRAPDEP)
```

Gracias a esto, cambiar una cabecera recompila los `.cpp` afectados.

## 9. Reglas de compilación

C++:

```make
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(OBJ_DIR) | $(DEP_DIR)
	$(call compile_cpp,)

$(OBJ_DIR)/$(WRAPPER_DIR)/%.o: $(WRAPPER_DIR)/%.cpp $(OBJ_DIR) | $(DEP_DIR)
	$(call compile_cpp,$(WRAPPER_DIR)/)
```

C (GLAD):

```make
$(OBJ_DIR)/external/glad/src/%.o: $(GLDIR)/%.c $(OBJ_DIR) | $(DEP_DIR)
	$(call compile_c,$(GLDIR)/)
```

Enlace:

```make
$(NAME): $(OBJS) $(GLOBJS) $(WRAPOBJS)
	@ if $(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) $(GLOBJS) $(WRAPOBJS) $(LDLIBS) -o $(NAME); then ...
```

Las bibliotecas van después de los objetos, que es el orden correcto para que el
enlazador resuelva los símbolos de GLFW.

## 10. Limpieza

`clean` elimina `.obj` y `.dep`. `fclean` elimina además `mod1`:

```sh
make clean
make fclean
```

No elimina `external/glfw`, `external/glad` ni los scripts. Si quieres volver a
descargar GLFW, borra `external/glfw` manualmente y ejecuta el script de nuevo.

## 11. Lint

Los targets de lint usan Docker Compose:

```text
test/codeStyle/docker-compose.yml
```

Necesitan Docker. `LINT_DIRS` permite limitar los directorios:

```sh
make lint LINT_DIRS="src Wrapper/core"
```

## 12. Silencio de comandos

El Makefile termina con:

```make
.SILENT:
```

Esto oculta los comandos ejecutados y deja solo los mensajes `INFO`, `SUCCESS`,
`WARNING` y `ERROR`. Si necesitas depurar una receta, ejecuta `make -n` para ver
los comandos sin ejecutarlos.

## 13. Diagnóstico rápido

Ver las variables efectivas:

```sh
make -p | grep -E '^(LDLIBS|CXXFLAGS|GLFW_LIBS|GLFW_CFLAGS) ?[:?]?='
```

Ver el enlace sin compilar:

```sh
make -n re
```

En Windows, si `make -p` muestra `LDLIBS` vacío, revisa que no hayas definido
`GLFW_LIBS` vacío en el entorno. El valor esperado es:

```text
-Lexternal/glfw/lib-mingw-w64 -lglfw3 -lopengl32 -lgdi32
```
