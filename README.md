# Gravitational Particles — Pro Edition.
### What this is.

This isn't a `GL_POINTS`-and-immediate-mode toy — it's architected the same way the games industry builds 2D VFX pipelines:

- **OpenGL 3.3 Core Profile programmable pipeline** — every primitive is drawn through shaders; no legacy `glBegin/glEnd` remains.
- **GPU instancing** — 10,000+ particles are drawn in a single draw call via `glDrawArraysInstanced`, not one at a time.
- **HDR framebuffer + bloom** — the scene renders into a 16-bit float buffer, the brightest regions "bleed" through a two-pass separable Gaussian blur (ping-pong framebuffers), then get additively composited back onto the scene — the same technique used in Unreal Engine and most modern games.
- **Tone mapping** — the final composite pass applies Reinhard tone mapping and gamma correction so bright areas don't clip to flat white.
- **Clean architecture** — the code is split into independent classes (`Shader`, `Framebuffer`, `ParticleSystem`, `StarField`) with RAII-managed OpenGL resources (destructors free VAOs/VBOs/FBOs/shaders automatically).
- **Zero warnings** building with `-Wall -Wextra`.
- **Cross-platform CMake build** — `find_package` for GLFW/GLEW/OpenGL, no hardcoded paths.

### Project layout.
gravity-particles/
├── CMakeLists.txt          # find_package-based build (GLFW, GLEW, OpenGL).
├── LICENSE
├── README.md
├── include/
│   ├── Shader.h             # shader program compilation/linking.
│   ├── Framebuffer.h        # HDR framebuffer for scene & bloom.
│   ├── ParticleSystem.h     # physics + instanced particle rendering.
│   ├── StarField.h          # twinkling starfield background.
│   └── Math.h                # orthographic projection matrix.
├── src/
│   ├── main.cpp              # window, input, main loop, bloom pipeline.
│   ├── Shader.cpp
│   ├── Framebuffer.cpp
│   ├── ParticleSystem.cpp
│   └── StarField.cpp
└── shaders/
    ├── particle.vert / .frag  # instanced billboard particles with soft glow.
    ├── star.vert / .frag      # twinkling point stars (gl_PointSize).
    ├── fullscreen.vert        # shared vertex shader for post-processing.
    ├── blur.frag              # separable Gaussian blur (9-tap).
    └── composite.frag         # additive scene + bloom composite + tone mapping.
```

### How it works, physically.
1. **Physics (CPU)** — every frame, gravitational attraction toward each active source is computed for all particles (`F = G·m / r²`), updating velocity and position. One source spawns in the center at startup; left-click adds more.
2. **Scene render (GPU, HDR)** — stars and particles are drawn into a `GL_RGBA16F` texture. Particle brightness can exceed 1.0, which is exactly what feeds the bloom pass.
3. **Blur (ping-pong)** — 8 passes of alternating horizontal/vertical Gaussian blur between two lower-resolution framebuffers (for performance).
4. **Composite** — the sharp scene and the blurred bloom layer are added together, tone mapping and gamma correction are applied, and the result is presented to the screen.

### Requirements.
- A C++17-compatible compiler (`g++` ≥ 9, `clang++` ≥ 10, MSVC ≥ 2019)
- CMake ≥ 3.10
- **GLFW3** — window creation and input handling
- **GLEW** — OpenGL function loading
- A GPU/driver supporting **OpenGL 3.3 Core Profile** (virtually any system from the last ~15 years, including Intel integrated graphics)

### Installing dependencies.
**Ubuntu / Debian:**
```bash
sudo apt update
sudo apt install cmake libglfw3-dev libglew-dev libgl1-mesa-dev
```
**Fedora:**
```bash
sudo dnf install cmake glfw-devel glew-devel mesa-libGL-devel
```
**Arch Linux:**
```bash
sudo pacman -S cmake glfw-x11 glew
```
**macOS (Homebrew):**
```bash
brew install cmake glfw glew
```
**Windows (vcpkg):**
```powershell
vcpkg install glfw3 glew
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake
```

### Building and running.
```bash
git clone <repository_URL>   # or just unpack the project archive
cd gravity-particles

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

./gravity_particles
```

CMake automatically copies the `shaders/` folder next to the executable after building — run the binary from inside `build/` (or wherever it and its `shaders/` folder were copied together).

### Controls
| Action | Key / Button |
|---|---|
| Add a gravitational source | Left mouse button (click) |
| Drag the source with the cursor | Left mouse button (hold) |
| Clear all sources | Right mouse button |
| Explode particles + bloom flash | Spacebar |
| Cycle color palette | `C` |
| Quit | `Esc` |

### Tuning
All key parameters live in the `Config` namespace at the top of `src/main.cpp`:

```cpp
namespace Config {
    int windowW = 1100;
    int windowH = 800;
    int particleCount = 6000;    // number of particles
    int starCount = 250;         // number of background stars
    float bloomStrength = 1.4f;  // glow intensity
    int blurPasses = 8;          // blur quality (even number)
    int blurDownscale = 2;       // speeds up blur by lowering its resolution
}
```
On a modern GPU, `particleCount` can comfortably go up to 20,000–50,000 — the bottleneck is CPU-side physics (`O(n·k)`, k = number of sources), not rendering, so performance scales gracefully.

### Where to take it next
The project is deliberately structured to make extending it straightforward:
- **Compute shaders** — move physics onto the GPU (`glDispatchCompute`) for millions of particles instead of thousands
- **Barnes–Hut / quadtree** — true particle-to-particle gravity instead of attraction to fixed sources only
- **ImGui panel** — runtime parameter tuning without recompiling
- **MSAA / FXAA** — edge anti-aliasing
- **Video export** — frame-by-frame render to PNG files via `stb_image_write`

### Troubleshooting
**`Could not find a package configuration file for glfw3`**
`libglfw3-dev` (or your distro's equivalent) isn't installed. See "Installing dependencies" above.
**`shaders/particle.vert: Failed to open file`**
The program isn't running from the right directory — shaders are looked up via the relative path `shaders/`. Run it from inside `build/` (CMake copies a `shaders/` folder next to the binary there).
**Black screen / low FPS**
Verify your driver supports OpenGL 3.3+ (`glxinfo | grep "OpenGL version"` on Linux). On virtual machines or over SSH without GPU passthrough, 3D rendering may not work at all.


## Лицензия / License

MIT — см. файл [LICENSE](LICENSE) / see the [LICENSE](LICENSE) file.




Профессиональная реализация N-body гравитационной симуляции на **современном OpenGL 3.3** (программируемый конвейер, instanced rendering) с полноценным **bloom-постпроцессингом**, написанная на C++17 и собираемая через CMake.

A production-grade N-body gravitational particle simulation built on **modern OpenGL 3.3** (programmable pipeline, instanced rendering) with full **bloom post-processing**, written in C++17 and built with CMake.

![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3%20Core-orange)
![CMake](https://img.shields.io/badge/build-CMake-064F8C)
![License](https://img.shields.io/badge/license-MIT-green)

---
### Что это такое

Это не учебная демка на `GL_POINTS` и immediate mode, а архитектурно выстроенный проект того же уровня, что использует игровая индустрия для 2D-эффектов:

- **Программируемый конвейер OpenGL 3.3 Core Profile** — все примитивы рисуются шейдерами, ничего не осталось от устаревшего `glBegin/glEnd`.
- **GPU instancing** — до 10 000+ частиц отрисовываются за один draw call через `glDrawArraysInstanced`, а не поштучно.
- **HDR framebuffer + bloom** — сцена рендерится в 16-битный float-буфер, ярчайшие области "перетекают" через двухпроходное сепарабельное гауссово размытие (ping-pong framebuffers), затем аддитивно комбинируются со сценой — тот же приём, что в Unreal Engine и большинстве современных игр.
- **Tone mapping** — финальный композитинг применяет Reinhard tone mapping и гамма-коррекцию, чтобы яркие зоны не "пересвечивались" в чистый белый.
- **Чистая архитектура** — код разбит на независимые классы (`Shader`, `Framebuffer`, `ParticleSystem`, `StarField`) с RAII-управлением ресурсами OpenGL (деструкторы освобождают VAO/VBO/FBO/шейдеры автоматически).
- **Ноль предупреждений** при сборке с `-Wall -Wextra`.
- **Кроссплатформенная сборка через CMake** — `find_package` для GLFW/GLEW/OpenGL, без хардкода путей.

### Архитектура проекта

```
gravity-particles/
├── CMakeLists.txt          # сборка через find_package (GLFW, GLEW, OpenGL)
├── LICENSE
├── README.md
├── include/
│   ├── Shader.h             # компиляция/линковка шейдерных программ
│   ├── Framebuffer.h        # HDR framebuffer для сцены и bloom
│   ├── ParticleSystem.h     # физика + instanced-рендеринг частиц
│   ├── StarField.h          # мерцающий звёздный фон
│   └── Math.h                # ортографическая проекционная матрица
├── src/
│   ├── main.cpp              # окно, ввод, главный цикл, bloom pipeline
│   ├── Shader.cpp
│   ├── Framebuffer.cpp
│   ├── ParticleSystem.cpp
│   └── StarField.cpp
└── shaders/
    ├── particle.vert / .frag  # instanced billboard-частицы с мягким свечением
    ├── star.vert / .frag      # точечные звёзды с мерцанием (gl_PointSize)
    ├── fullscreen.vert        # общий вершинный шейдер для постобработки
    ├── blur.frag              # сепарабельное гауссово размытие (9-тэп)
    └── composite.frag         # аддитивный композитинг сцены + bloom + tone mapping
```

### Как это физически работает

1. **Физика (CPU)** — на каждом кадре для всех частиц вычисляется гравитационное притяжение к активным источникам (`F = G·m / r²`), обновляются скорость и позиция. Один источник в центре создаётся при старте; ЛКМ добавляет новые.
2. **Рендер сцены (GPU, HDR)** — звёзды и частицы рисуются в текстуру `GL_RGBA16F`. Яркость частиц может уходить за пределы 1.0 — это и создаёт материал для bloom.
3. **Размытие (ping-pong)** — 8 проходов попеременного горизонтального/вертикального гауссова размытия между двумя framebuffer'ами уменьшенного разрешения (для производительности).
4. **Композитинг** — резкая сцена и размытый bloom-слой складываются аддитивно, применяется tone mapping и гамма-коррекция, результат выводится на экран.

### Требования

- Компилятор с поддержкой C++17 (`g++` ≥ 9, `clang++` ≥ 10, MSVC ≥ 2019)
- CMake ≥ 3.10
- **GLFW3** — создание окна и обработка ввода
- **GLEW** — загрузка функций OpenGL
- Видеокарта/драйвер с поддержкой **OpenGL 3.3 Core Profile** (практически любая система за последние ~15 лет, включая встроенную графику Intel)

### Установка зависимостей

**Ubuntu / Debian:**
```bash
sudo apt update
sudo apt install cmake libglfw3-dev libglew-dev libgl1-mesa-dev
```

**Fedora:**
```bash
sudo dnf install cmake glfw-devel glew-devel mesa-libGL-devel
```

**Arch Linux:**
```bash
sudo pacman -S cmake glfw-x11 glew
```

**macOS (Homebrew):**
```bash
brew install cmake glfw glew
```

**Windows (vcpkg):**
```powershell
vcpkg install glfw3 glew
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[путь_к_vcpkg]/scripts/buildsystems/vcpkg.cmake
```

### Сборка и запуск

```bash
git clone <URL_репозитория>   # либо просто распакуйте архив проекта
cd gravity-particles

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

./gravity_particles
```

CMake автоматически копирует папку `shaders/` рядом с исполняемым файлом после сборки — запускать нужно именно из директории `build/` (или туда, куда скопирован бинарник вместе с `shaders/`).

### Управление

| Действие | Клавиша / кнопка |
|---|---|
| Добавить гравитационный источник | Левая кнопка мыши (клик) |
| Двигать источник за курсором | Левая кнопка мыши (удержание) |
| Сбросить источники | Правая кнопка мыши |
| Взрыв частиц + вспышка bloom | Пробел |
| Сменить цветовую палитру | `C` |
| Выход | `Esc` |

### Настройка

Все ключевые параметры вынесены в `namespace Config` в начале `src/main.cpp`:

```cpp
namespace Config {
    int windowW = 1100;
    int windowH = 800;
    int particleCount = 6000;    // количество частиц
    int starCount = 250;         // количество звёзд фона
    float bloomStrength = 1.4f;  // сила свечения
    int blurPasses = 8;          // качество размытия (чётное число)
    int blurDownscale = 2;       // ускоряет blur, снижая его разрешение
}
```

На современном GPU `particleCount` спокойно можно поднять до 20 000–50 000 — узкое место здесь физика на CPU (`O(n·k)`, где k — число источников), а не рендер, так что производительность масштабируется отлично.

### Дальнейшие направления развития

Проект специально спроектирован так, чтобы было легко наращивать функциональность:

- **Compute shaders** — перенос физики на GPU (`glDispatchCompute`) для миллионов частиц вместо тысяч
- **Barnes–Hut / квадродерево** — для честной гравитации "частица-частица" вместо притяжения только к фиксированным источникам
- **ImGui-панель** — рантайм-настройка параметров без пересборки
- **MSAA / FXAA** — сглаживание краёв
- **Экспорт видео** — покадровый рендер в файлы PNG через `stb_image_write`

### Решение проблем

**`Could not find a package configuration file for glfw3`**
Не установлен `libglfw3-dev` (или аналог для вашего дистрибутива). См. раздел «Установка зависимостей».

**`shaders/particle.vert: Не удалось открыть файл`**
Программа запущена не из той директории — шейдеры ищутся по относительному пути `shaders/`. Запускайте из папки `build/` (там CMake создаёт копию `shaders/` рядом с бинарником).

**Чёрный экран / низкий FPS**
Проверьте, что видеодрайвер поддерживает OpenGL 3.3+ (`glxinfo | grep "OpenGL version"` на Linux). На виртуальных машинах и через SSH без GPU-проброса 3D-рендеринг может не работать.
