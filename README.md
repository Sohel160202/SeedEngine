# Seed Engine

> **Grow your idea into a game.**

Seed Engine is a beginner-first game engine being built as its **own engine**, not as a plugin or authoring layer for another game engine.

The goal is simple:

**A new creator should think about the game they want to make, not the engine plumbing required to make it.**

Seed should make polished 3D games approachable without trapping users in a toy tool. Beginners work with concepts such as **Player**, **Door**, **Enemy**, **Inventory**, **Quest**, and **Dialogue**. Advanced users can progressively access deeper logic, scripting, native systems, rendering, and engine extensions.

## Direction

Seed owns its core architecture:

- **Language:** C++20
- **Build system:** CMake
- **Seed Core:** application lifecycle, frame loop, services
- **Seed Scene:** entities and typed components
- **Seed Runtime:** runs Seed games
- **Seed Studio:** our own editor and authoring environment
- **Seed Gameplay:** beginner-facing reusable gameplay concepts
- **Seed Input:** Seed-owned keyboard/mouse event model
- **Seed Platform:** replaceable native platform abstraction
- **Seed Renderer:** replaceable renderer abstraction and backend selection
- **Seed project/scene formats:** will be owned by Seed

Seed may use focused third-party libraries for low-level jobs. Those libraries do **not** define Seed's object model, project format, gameplay framework, editor, or runtime architecture.

## Important architecture rule

**Seed is not built on another game engine.**

The earlier Godot prototype proved the `Add Gameplay` UX concept. It is no longer the active product architecture. Its history remains in Git so we can learn from it without carrying Godot into the engine.

GLFW 3.5.1 is currently used only as a replaceable low-level desktop window/input backend behind Seed's `IPlatform` interface. GLFW types and key codes are translated at the backend boundary and are not exposed to Seed gameplay or project content.

See `docs/DEPENDENCIES.md` for the dependency policy.

## What exists now

### Seed Core

- standalone C++20 `SeedEngine` library
- engine lifecycle
- frame timing / frame index
- headless mode
- automated tests

### Seed Scene

- entity creation/destruction
- entity names
- typed component storage
- reusable gameplay component data

Current components include:

- `TransformComponent`
- `InteractableComponent`
- `DoorComponent`
- `HealthComponent`
- `InventoryComponent`

### Seed Platform

- native application windows
- Seed-owned keyboard codes
- Seed-owned mouse button codes
- key/button state translation
- mouse movement and wheel events
- resize / focus / quit events
- resizable windows
- Escape-to-close in Runtime and Studio

### Seed Renderer

- `IRenderer`
- renderer factory
- first OpenGL backend
- runtime OpenGL function loading
- clear frame
- framebuffer resize handling
- vsync
- frame presentation

### Applications

- `SeedRuntime`
- `SeedStudio`

Both are real standalone Seed executables linked to the same Seed Engine library.

## The Seed mental model

Seed organizes creation around three beginner-readable ideas.

### World
What exists?

Characters, environments, props, items, lights, cameras and levels.

### Behaviour
What can things do?

Move, interact, fight, talk, collect, open, chase and trade.

### Rules
What happens when something occurs?

Quests, conditions, progression, events, victory, defeat and game state.

## Progressive complexity

Seed should never punish a creator for becoming more advanced.

1. **Designer** — presets, properties, gameplay components
2. **Seed Logic** — readable event / condition / action authoring
3. **Advanced visual logic** — lower-level control where useful
4. **Code** — native C++ and future supported scripting/extension paths

## Repository layout

```text
SeedEngine/
├─ engine/                 # Seed Engine static library
│  ├─ include/seed/
│  │  ├─ core/
│  │  ├─ gameplay/
│  │  ├─ input/
│  │  ├─ platform/
│  │  ├─ render/
│  │  └─ scene/
│  └─ src/
├─ runtime/                # Seed Runtime executable
├─ editor/                 # Seed Studio executable
├─ tests/                  # Core engine tests
├─ docs/                   # Architecture, dependency policy and roadmap
├─ CMakeLists.txt
└─ README.md
```

## Build

Requirements:

- CMake 3.24+
- a C++20 compiler
- Git access during the initial CMake configure so GLFW can be fetched

On Ubuntu/Debian, building the current X11 backend also requires:

```bash
sudo apt install xorg-dev
```

Build Seed:

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Current executables:

```text
SeedRuntime
SeedStudio
```

## Current milestone

Phase 0 (**Seed Core**) is complete and has compiled/tested successfully through GitHub Actions on Windows, Linux and macOS.

Phase 1 (**Platform Layer**) is implemented. The next manual verification is launching the native Studio/Runtime windows on a desktop machine.

Phase 2 (**Renderer Foundation**) has started. The first Seed OpenGL backend now owns clear-frame rendering and presentation. Next comes the first triangle, GPU buffers, shaders, camera matrices, and eventually rendering Seed Scene entities.

See `docs/ROADMAP.md` for the full path.

## v0.1 product target

A person with no programming experience should eventually be able to create a small, presentable third-person 3D game without writing code.

The v0.1 creator-facing vertical slice will contain:

- Seed Player preset
- third-person movement + camera
- interaction
- health and damage
- pickups and inventory
- doors, locks and keys
- dialogue
- simple quests
- basic enemy AI
- save/load
- HUD and settings
- one complete game made through Seed Studio

## Status

**Pre-alpha — independent engine foundation + native platform + renderer bootstrap.**
