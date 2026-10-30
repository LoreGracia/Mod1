# Windows: instalación, compilación y ejecución

Esta guía usa **MSYS2** con el toolchain **MinGW-w64 de 64 bits**. El Makefile
ejecuta recetas POSIX (`printf`, `rm`, `mkdir -p`, `mktemp`, `[`), por lo que
fuerza `SHELL := sh` cuando detecta Windows. Por eso la compilación debe
ejecutarse desde una shell que tenga `sh.exe`, normalmente la terminal
**MSYS2 MinGW 64-bit** o una terminal con MSYS2/Git Bash en el `PATH`.

## 1. Requisitos

- Windows 10 u 11.
- MSYS2 instalado, por defecto en `C:\msys64`.
- Toolchain MinGW-w64 de 64 bits.
- `make`.
- `curl` y `unzip` para descargar GLFW.
- GLFW vendorizado en `external/glfw` o instalado de forma que el Makefile lo
  encuentre.

## 2. Instalar MSYS2 y el toolchain

Descarga MSYS2 desde <https://www.msys2.org/> e instálalo. Después abre la
terminal **MSYS2 MinGW 64-bit**. No uses la terminal "MSYS2 MSYS" para compilar
si no tienes el toolchain MinGW en el `PATH`.

Actualiza la base de datos y los paquetes:

```sh
pacman -Syu
```

Si MSYS2 pide cerrar la terminal, ciérrala, abre de nuevo **MSYS2 MinGW 64-bit**
y continúa.

Instala las herramientas necesarias:

```sh
pacman -S --needed make mingw-w64-x86_64-gcc unzip curl
```

Comprueba que están disponibles:

```sh
command -v make g++ gcc sh
make --version
g++ --version
```

No es necesario instalar `mingw32-make`; el Makefile funciona con `make` de
MSYS2. Si prefieres usar `mingw32-make`, instala:

```sh
pacman -S --needed mingw-w64-x86_64-make
```

## 3. Entrar en el repositorio

En MSYS2, `C:` está montado en `/c`. El home `/home/Dannyel` es el home interno
de MSYS2 y no contiene el repositorio.

```sh
cd /c/Users/Dannyel/Documents/GitHub/Mod1
pwd
ls
```

Debes ver `Makefile`, `src`, `Wrapper`, `external`, etc. Si la ruta cambia,
ajústala. Si el proyecto está en otra unidad, usa `/d/...`, `/e/...`, etc.

También puedes comprobar la ruta con:

```sh
ls /c/Users/Dannyel/Documents/GitHub
```

## 4. Vendorizar GLFW para MinGW-w64

El Makefile de Windows espera:

```text
external/glfw/include/GLFW/glfw3.h
external/glfw/lib-mingw-w64/libglfw3.a
```

Ejecuta:

```sh
./scripts/setup_glfw_mingw.sh
```

El script descarga los binarios oficiales de GLFW 3.4 para MinGW-w64 y copia
`include` y `lib-mingw-w64` a `external/glfw`. Si no tienes `unzip`:

```sh
pacman -S --needed unzip
```

Para compilar GLFW desde fuente con CMake:

```sh
pacman -S --needed cmake
./scripts/setup_glfw_mingw.sh --source
```

Para 32 bits:

```sh
GLFW_ARCH=32 ./scripts/setup_glfw_mingw.sh
```

En ese caso necesitas el toolchain MinGW de 32 bits y un Makefile configurado
para esa arquitectura. La configuración por defecto documentada aquí es 64 bits.

Verifica los archivos:

```sh
ls -l external/glfw/include/GLFW/glfw3.h
ls -l external/glfw/lib-mingw-w64
```

## 5. Compilar

Desde la raíz del repositorio:

```sh
make
```

Para limpiar y recompilar todo:

```sh
make re
```

El target `re` ejecuta `fclean` y después `all`, que incluye
`check_requirements`.

El Makefile de Windows enlaza:

```text
-Lexternal/glfw/lib-mingw-w64 -lglfw3 -lopengl32 -lgdi32
```

`libglfw3.a` es la biblioteca estática oficial de GLFW para MinGW-w64. Si usas
la biblioteca de importación de la DLL (`libglfw3dll.a`), necesitarás copiar
`glfw3.dll` junto al ejecutable o añadirlo al `PATH`.

## 6. Ejecutar

```sh
./mod1.exe
```

o, si MSYS2 resuelve la extensión:

```sh
./mod1
```

## 7. Orden exacto de ejecución

El orden recomendado en Windows es:

1. Abrir **MSYS2 MinGW 64-bit**.
2. Ejecutar `pacman -Syu` si es la primera instalación.
3. Instalar `make`, `mingw-w64-x86_64-gcc`, `unzip` y `curl`.
4. Entrar al repositorio con `cd /c/.../Mod1`.
5. Ejecutar `./scripts/setup_glfw_mingw.sh`.
6. Comprobar que existen `external/glfw/include/GLFW/glfw3.h` y
   `external/glfw/lib-mingw-w64/libglfw3.a`.
7. Ejecutar `make` o `make re`.
8. Ejecutar `./mod1.exe` desde la raíz del repositorio.

El paso 5 debe ir antes del 7 porque `check_requirements` busca GLFW y el enlace
usa la ruta vendorizada. El paso 7 debe ejecutarse desde la raíz porque el
Makefile usa rutas relativas (`src/`, `Wrapper/`, `external/`, `.obj/`).

## 8. VS Code

VS Code no lista MSYS2 por defecto. Puedes añadir un perfil de terminal.

Abre `Ctrl+Shift+P` → **Preferences: Open User Settings (JSON)** y añade:

```json
"terminal.integrated.profiles.windows": {
  "MSYS2 MinGW 64-bit": {
    "path": "C:\\msys64\\msys2_shell.cmd",
    "args": ["-defterm", "-here", "-no-start", "-mingw64"]
  }
}
```

Reinicia VS Code y selecciona **MSYS2 MinGW 64-bit** en la lista de terminales.
Si MSYS2 está en otra ruta, cambia `C:\\msys64`.

Alternativa: usar la terminal integrada de PowerShell y añadir MSYS2 al `PATH`
solo para esa sesión:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$env:PATH"
cd "C:\Users\Dannyel\Documents\GitHub\Mod1"
where.exe sh
where.exe g++
make re
```

No añadas esas rutas de forma permanente si no es necesario; pueden cambiar qué
versiones de `make`, `g++` o `sh` usa el sistema.

## 9. Problemas frecuentes

### `printf` no se reconoce como comando

El Makefile está usando `cmd.exe`. Asegúrate de usar la versión actual del
Makefile, que define `SHELL := sh` en Windows, y ejecuta desde MSYS2.

### `mingw32-make: command not found`

Usa `make`, o instala:

```sh
pacman -S --needed mingw-w64-x86_64-make
```

### `Missing required tool: unzip`

```sh
pacman -S --needed unzip
```

### `GLFW not found`

Ejecuta:

```sh
./scripts/setup_glfw_mingw.sh
```

y comprueba:

```sh
ls external/glfw/include/GLFW/glfw3.h
ls external/glfw/lib-mingw-w64
```

### `undefined reference to glfwInit` y similares

Comprueba que el enlace incluye GLFW:

```sh
make -p | grep LDLIBS
```

En Windows debe aparecer:

```text
LDLIBS := -Lexternal/glfw/lib-mingw-w64 -lglfw3 -lopengl32 -lgdi32
```

Si definiste `GLFW_LIBS` manualmente y lo dejaste vacío, elimínalo del entorno o
pasa los flags correctos:

```sh
make GLFW_LIBS="-Lexternal/glfw/lib-mingw-w64 -lglfw3 -lopengl32 -lgdi32" re
```

### Error de GLM sobre `GLM_GTX_transform` o `Transform does not name a type`

El include de `Transform.hpp` está resolviendo al header experimental de GLM.
El proyecto ya usa las rutas correctas:

```cpp
#include "Transform.hpp"        // dentro de Wrapper/scene
#include <scene/Transform.hpp>  // desde src/main.cpp
```

y el Makefile no añade `-I external/glm/gtx`. Si añades rutas nuevas, no vuelvas
a incluir ese directorio.

### `glfw3.dll` no encontrado al ejecutar

La configuración por defecto enlaza estáticamente `libglfw3.a`, así que no
debería necesitar DLL. Si cambias a `libglfw3dll.a`, copia `glfw3.dll` junto a
`mod1.exe` o añade su carpeta al `PATH`.

### Errores de Mesa/EGL en Linux

No aplican a Windows. En Linux sin sesión gráfica pueden aparecer avisos de
Mesa/EGL; el bucle puede seguir funcionando.
