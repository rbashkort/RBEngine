# RBEngine - 2D ECS Game Engine

A lightweight, data-oriented 2D game engine written in C++17 using Flecs (ECS) and RmlUi (HTML/CSS-like UI).

## 🚀 Dependencies and Build (Arch Linux)

**Dependencies:**
- `cmake`, `make`, `gcc` (Build tools)
- `glfw` (Windowing)
- `mesa` (OpenGL drivers)

```bash
sudo pacman -S cmake make gcc glfw mesa
```

Build:
```
git clone https://github.com/rbashkort/RBEngine.git
cd RBEngine
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 🎮 How to Use
1. Open src/main.cpp.
2. Define your scenes using the ECS API.
3. Build and run!

### ✅ Features (Implemented)
- [x] ECS Architecture (Flecs based) - fast and modular.
- [x] Rendering System (OpenGL 3.2).
- [x] Physics System (SAT Collision detection for Rects, Circles, Polygons).
- [x] UI System (RmlUi) - Layouts using HTML/CSS syntax.
- [x] Input System - Keyboard & Mouse handling.
- [x] Scene Management - Load/Reload scenes easily.
- [x] Sprite System - Textures, Colors, Basic Shapes.
- [x] Camera System Refactoring - Proper Screen-to-World coordinate conversion for mouse interaction.

### 📝 TODO (Roadmap)
- [ ] Sound System - Integration with miniaudio or Soloud.
- [ ] Animation System - Sprite sheet support.
- [ ] Particle System - Basic effects.
- [ ] Multi-window support - Managing multiple GLFW windows.
- [ ] Visual Effects - Shaders, Post-processing (Bloom, etc.).
- [ ] Documentation - Wiki with examples.

### 🛠 Project Structure
* engine/ - Core engine source code.
* src/ - User game code (entry point).
* assets/ - Resources (images, fonts, rml files).
* thirdparty/ - Libraries (Flecs, RmlUi, ImGui).