# Orden de ejecución

Este documento describe el orden de compilación y el orden de ejecución en
tiempo de ejecución. El orden es importante porque GLFW debe estar inicializado,
el contexto OpenGL debe estar activo y GLAD debe estar cargado antes de usar
cualquier función OpenGL.

## 1. Orden de compilación

Cuando ejecutas `make`:

1. Se detecta la plataforma.
2. Se comprueba que existen las herramientas y GLFW
   (`check_requirements`).
3. Se crean `.obj` y `.dep` si no existen.
4. Se compila `external/glad/src/gl.c` con `gcc`.
5. Se compila cada `.cpp` de `src` y `Wrapper` con `g++`.
6. Se enlazan todos los `.o` con GLFW, OpenGL y las bibliotecas del sistema.
7. Se genera `mod1` (o `mod1.exe` en Windows).

El paso 2 puede fallar antes de compilar si GLFW no está disponible. En Windows
debes ejecutar `scripts/setup_glfw_mingw.sh` antes de `make`.

## 2. Orden de ejecución del programa

### 2.1. Creación del entorno

```cpp
Environment env;
```

El constructor de `Environment` no crea ninguna ventana ni inicializa GLFW
todavía. Solo prepara los vectores internos.

### 2.2. Creación de la primera ventana

```cpp
WWindow::Config cfg;
cfg.title = "Mod1";
cfg.width = 900;
cfg.height = 600;
env.addWindow(cfg);
```

`addWindow` llama al constructor de `WWindow`. En ese momento:

1. Se inicializa GLFW si es la primera vez.
2. Se configuran los hints de OpenGL.
3. Se crea la ventana GLFW.
4. Se instalan los callbacks de teclado, ratón, scroll, tamaño, cierre y foco.
5. Se obtienen tamaño lógico y tamaño de framebuffer.
6. Se activa el contexto de la ventana.
7. Se configura VSync.

Todavía **no** se ha cargado GLAD. No llames a funciones OpenGL dentro del
constructor de una `WWindow` ni antes de `Environment::run()`.

### 2.3. Configuración de callbacks

```cpp
env.setInit(...);
env.setUpdate(...);
env.setRender(...);
```

Solo se guardan los callbacks. No se ejecutan todavía.

### 2.4. `Environment::run()`

El orden exacto dentro de `run()` es:

1. Si no hay ventanas, termina.
2. Activa el contexto de la primera ventana.
3. Carga GLAD con `glfwGetProcAddress`.
4. Imprime versión, fabricante y renderer de OpenGL.
5. Ejecuta `setInit` para la primera ventana.
6. Para cada ventana adicional:
   - Activa su contexto.
   - Ejecuta `setInit` para esa ventana.
7. Marca el entorno como en ejecución.
8. Calcula el tiempo inicial.
9. Entra en el bucle principal.

### 2.5. Bucle principal

En cada iteración:

1. Comprueba si queda alguna ventana abierta.
2. Si todas están cerradas, sale del bucle.
3. Ejecuta `input().beginFrame()` en todas las ventanas.
4. Ejecuta `WWindow::pollEventsAll()` una sola vez.
5. Calcula `dt` con `std::chrono::steady_clock`.
6. Para cada ventana abierta:
   - Activa su contexto.
   - Ejecuta `setUpdate(window, dt)`.
   - Ejecuta `setRender(window)`.
   - Hace `swapBuffers()`.
   - Ejecuta `input().endFrame()`.

Consecuencias:

- `isKeyPressed` y `isMouseButtonPressed` solo son válidos durante el frame en
  el que ocurrió el evento.
- `isKeyDown` sigue siendo válido hasta que se suelta la tecla.
- El delta del ratón y el scroll se acumulan durante el frame y se limpian al
  terminar.
- No llames a `glfwPollEvents()` dentro de `update` o `render`; romperías el
  orden de los estados de input.
- No llames a `swapBuffers()` dentro de `render`; `Environment` ya lo hace.

### 2.6. Salida del bucle

Cuando todas las ventanas están cerradas:

1. Se activa de nuevo el contexto de la primera ventana.
2. Se limpian los recursos registrados con `Environment::track`.
3. `run()` regresa.
4. Se destruyen las variables locales de `main` que contengan recursos OpenGL.
5. Se destruye `Environment`.
6. Se destruyen las ventanas GLFW.
7. Al destruir la última ventana se llama a `glfwTerminate()`.

El contexto de la primera ventana se deja activo a propósito para que los
recursos locales de `main` se destruyan con un contexto válido. Para que esto
funcione, declara `Environment` **antes** que los recursos OpenGL locales; las
variables se destruyen en orden inverso al de declaración.

## 3. Orden con varias ventanas

```cpp
WWindow& a = env.addWindow(cfgA);
WWindow& b = env.addWindow(cfgB);
```

- La primera ventana crea el contexto principal.
- La segunda ventana comparte contexto con la primera por defecto
  (`cfg.shareContext == nullptr`).
- `setInit` se ejecuta una vez por cada ventana, con su contexto activo.
- `setUpdate` y `setRender` se ejecutan por cada ventana abierta.
- El mismo `dt` se usa para todas las ventanas de ese frame.
- Si una ventana se cierra, se omite en los siguientes frames.

`Environment::addWindow` no permite crear una ventana sin contexto compartido:
si `shareContext` es `nullptr` y ya existe una ventana, lo apunta a la primera.
Para una ventana sin compartir contexto tendrías que crear un `WWindow` directo
fuera de `Environment` y gestionar su ciclo de vida, o modificar el wrapper.

## 4. Orden de recursos OpenGL

El orden seguro es:

1. Crear la ventana con `Environment::addWindow`.
2. Configurar callbacks.
3. En `setInit`, crear `Shader`, `Mesh` y demás recursos OpenGL.
4. En `setRender`, dibujar.
5. Dejar que `Environment` haga `swapBuffers`.
6. Al salir, dejar que los `shared_ptr` y `Environment` liberen los recursos.

`Environment::track` permite conservar recursos hasta el final:

```cpp
auto mesh = env.track(std::make_shared<Mesh>());
```

Los recursos se liberan en `cleanupGpuResources()` con el contexto de la primera
ventana activo.

## 5. Errores de orden frecuentes

| Error | Consecuencia |
|---|---|
| Llamar a `glGenBuffers` antes de `run()` | GLAD no está cargado o no hay contexto current. |
| Llamar a `glfwPollEvents` dentro de `update` | Se pierden o duplican flancos de input. |
| Llamar a `swapBuffers` dentro de `render` | Doble swap y parpadeos. |
| Destruir `Shader`/`Mesh` sin contexto current | `glDelete*` puede fallar o corromper estado. |
| Usar `width()`/`height()` para el viewport | Tamaño incorrecto en pantallas HiDPI. |
| Crear ventanas después de `run()` | `addWindow` lanza `std::runtime_error`. |
| Usar `Escape` esperando cierre automático | Debes llamar a `requestClose()` desde `update`. |
