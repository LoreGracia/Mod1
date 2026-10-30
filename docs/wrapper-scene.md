# Wrapper scene: Transform, Camera, Entity y Scene

El módulo `Wrapper/scene` contiene la representación de objetos, cámara y
escena. También se documentan las utilidades matemáticas de `Wrapper/math`.

## 1. Vec3

```cpp
#include <Vec3.hpp>
```

`Vec3` es un vector de tres `float`.

| Miembro | Descripción |
|---|---|
| `x`, `y`, `z` | Componentes. |
| `operator+`, `operator-` | Suma y resta. |
| `operator*(float)` | Producto por escalar. |
| `operator-()` | Negación. |
| `operator+=`, `operator-=` | Suma/resta acumulada. |
| `length()` | Longitud. |
| `normalized()` | Devuelve el vector normalizado o `(0,0,0)` si la longitud es casi cero. |

Funciones libres:

```cpp
float dot(const Vec3& a, const Vec3& b);
Vec3  cross(const Vec3& a, const Vec3& b);
```

No hay `operator/`, ni producto escalar por la izquierda, ni comparaciones.

## 2. Mat4

```cpp
#include <Mat4.hpp>
```

Matriz 4x4 almacenada en **column-major**, compatible con OpenGL.

| Miembro | Descripción |
|---|---|
| `m[16]` | Datos. |
| `data()` | Puntero a los datos. |
| `identity()` | Matriz identidad. |
| `multiply(a, b)` | Multiplicación de matrices. |
| `operator*` | Multiplicación. |
| `rotationX(deg)` | Rotación alrededor de X. |
| `rotationY(deg)` | Rotación alrededor de Y. |
| `rotationZ(deg)` | Rotación alrededor de Z. |
| `lookAt(eye, target, up)` | Matriz de vista. |
| `perspective(fovDeg, aspect, nearZ, farZ)` | Matriz de proyección. |

Los ángulos de rotación están en grados. `perspective` recibe el FOV en grados.

## 3. Transform

`Transform` representa posición, rotación y escala.

```cpp
#include <scene/Transform.hpp>
```

| Miembro | Tipo | Valor inicial |
|---|---|---|
| `position` | `glm::vec3` | `(0,0,0)` |
| `rotation` | `glm::quat` | identidad |
| `scale` | `glm::vec3` | `(1,1,1)` |

### 3.1. Métodos

| Método | Descripción |
|---|---|
| `matrix()` | Devuelve `T * R * S`. |
| `translate(d)` | Suma `d` a `position`. |
| `rotate(angleDeg, axis)` | Rota en grados alrededor de un eje. |
| `rotateEuler(pitchDeg, yawDeg, rollDeg)` | Aplica rotación Euler. |

`rotate` normaliza el eje y el cuaternión resultante. Evita pasar un eje de
longitud cero.

`rotateEuler` usa el orden:

```cpp
rotation = normalize(qy * qx * qz);
```

donde `qx`, `qy`, `qz` son rotaciones alrededor de X, Y y Z.

### 3.2. Ejemplo

```cpp
Transform t;
t.position = {0.f, 1.f, 0.f};
t.scale = {2.f, 2.f, 2.f};
t.rotate(45.f, {0.f, 1.f, 0.f});

glm::mat4 model = t.matrix();
```

## 4. Camera

`Camera` mantiene posición, objetivo, FOV y planos de recorte.

```cpp
#include <Camera.hpp>
```

### 4.1. Constructor

```cpp
Camera(const glm::vec3& position,
       const glm::vec3& target,
       const glm::vec3& up = {0, 1, 0});
```

Valores por defecto:

| Campo | Valor |
|---|---|
| `position` | `(0, 0, 5)` |
| `target` | `(0, 0, 0)` |
| `worldUp` | `(0, 1, 0)` |
| `fov` | `60` grados |
| `near` | `0.1` |
| `far` | `1000` |
| `distance` | `5` |

El constructor calcula `distance`, `pitch` y `yaw` a partir de
`position - target`.

### 4.2. Métodos

| Método | Descripción |
|---|---|
| `view()` | `glm::lookAt(position, target, worldUp)`. |
| `projection(aspect)` | `glm::perspective(radians(fov), aspect, near, far)`. |
| `move(delta)` | Mueve posición y objetivo (pan). |
| `lookAt(target)` | Cambia solo el objetivo. |
| `setPosition(p)` | Cambia solo la posición. |
| `setFov(deg)` | Cambia el FOV. |
| `setClip(n, f)` | Cambia near/far. |
| `setOrbitTarget(t)` | Cambia el objetivo de órbita. |
| `orbit(dYaw, dPitch, dDist)` | Orbita alrededor del objetivo. |
| `position()` | Posición actual. |
| `forward()` | Dirección normalizada hacia el objetivo. |
| `right()` | Vector derecha. |
| `up()` | Vector arriba. |

`move` es un desplazamiento tipo pan: mueve cámara y objetivo a la vez. No es una
cámara libre que solo mueve la posición.

`lookAt`, `setPosition` y `setOrbitTarget` no recalculan `yaw`, `pitch` ni
`distance`. Si después llamas a `orbit`, se usarán los valores internos
anteriores. Si necesitas sincronizar la órbita tras cambiar posición u objetivo,
crea una cámara nueva; `setOrbitTarget` solo cambia el objetivo.

`orbit` limita el pitch a `[-89, 89]` y la distancia mínima a `0.1`.

### 4.3. Ejemplo

```cpp
Camera cam({4.f, 2.f, 4.f}, {0.f, 0.f, 0.f});

glm::mat4 view = cam.view();
glm::mat4 proj = cam.projection(16.f / 9.f);
glm::mat4 vp = proj * view;

cam.orbit(10.f, 0.f, 0.f); // yaw
cam.orbit(0.f, 0.f, -0.5f); // zoom out/in
cam.move(cam.right() * 0.5f); // pan
```

## 5. Entity

`Entity` combina un `Transform`, un `Mesh` y un `Shader`.

```cpp
#include <Entity.hpp>
```

### 5.1. Constructores

```cpp
Entity();
Entity(std::shared_ptr<Mesh> mesh, std::shared_ptr<Shader> shader);
```

El constructor por defecto crea una entidad sin mesh ni shader. `render` no hace
nada si falta alguno.

### 5.2. Miembros y métodos

| Miembro | Descripción |
|---|---|
| `transform()` | Devuelve el `Transform`. |
| `setMesh(mesh)` | Asigna el mesh. |
| `setShader(shader)` | Asigna el shader. |
| `velocity` | Velocidad lineal (`glm::vec3`). |
| `angularVel` | Velocidad angular en radianes por segundo (`glm::vec3`). |
| `update(dt)` | Integra velocidad y velocidad angular. |
| `render(viewProj)` | Activa el shader, envía `uModel` y `uViewProj` y dibuja. |

`update` hace, de forma equivalente:

```cpp
position += velocity * dt;
if (length(angularVel) > 1e-6f)
    transform.rotate(degrees(length(angularVel)) * dt, angularVel / length(angularVel));
```

Por tanto, `angularVel` se interpreta en radianes por segundo. La dirección del
vector es el eje de rotación y su longitud es la velocidad angular.

`render` asume que el shader tiene uniforms llamados exactamente:

```glsl
uniform mat4 uModel;
uniform mat4 uViewProj;
```

### 5.3. Ejemplo

```cpp
auto entity = std::make_shared<Entity>(mesh, shader);
entity->transform().position = {0.f, 0.f, 0.f};
entity->velocity = {1.f, 0.f, 0.f};
entity->angularVel = {0.f, 1.f, 0.f};

entity->update(dt);
entity->render(viewProj);
```

## 6. Scene

`Scene` agrupa entidades y una cámara.

```cpp
#include <Scene.hpp>
```

### 6.1. Métodos

| Método | Descripción |
|---|---|
| `addEntity(std::shared_ptr<Entity>)` | Añade una entidad no nula. |
| `removeEntity(Entity*)` | Elimina la entidad con ese puntero. |
| `update(dt)` | Llama a `update(dt)` en todas las entidades. |
| `render(aspect)` | Calcula `projection(aspect) * view()` y renderiza todas las entidades. |
| `camera()` | Devuelve la cámara. |
| `entities()` | Devuelve el vector de entidades. |

`Scene::render` no configura viewport, no limpia la pantalla y no hace swap. Eso
lo hace el `Renderer` y `Environment`.

### 6.2. Ejemplo

```cpp
Scene scene;
scene.addEntity(cube);
scene.addEntity(floor);

scene.update(dt);
scene.render(w.aspect());
```

## 7. Orden recomendado de uso

1. Crear `Shader` y `Mesh` en `setInit`.
2. Crear `Entity` con esos recursos.
3. Añadir entidades a `Scene` o gestionarlas manualmente.
4. En `update`, actualizar cámara y entidades.
5. En `render`, configurar `Renderer`, limpiar y llamar a `Scene::render` o a
   `Entity::render`.
6. Dejar que `Environment` haga `swapBuffers`.
