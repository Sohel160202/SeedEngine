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
- **Seed Core:** application lifecycle and frame loop
- **Seed Scene:** entities, typed components, persistent identities and scene queries
- **Seed Runtime:** runs Seed games
- **Seed Studio:** our own editor and authoring environment
- **Seed Gameplay:** beginner-facing reusable gameplay concepts
- **Seed Input:** Seed-owned keyboard/mouse event model
- **Seed Platform:** replaceable native platform abstraction
- **Seed Renderer:** replaceable renderer abstraction and backend selection
- **Seed Math:** vectors, matrices and 3D transform/camera math
- **Seed project/scene formats:** Seed-owned `.seedproject` and `.seedscene`

Seed may use focused third-party libraries for low-level jobs. Those libraries do **not** define Seed's object model, project format, gameplay framework, editor, or runtime architecture.

## Important architecture rule

**Seed is not built on another game engine.**

The earlier Godot prototype proved the `Add Gameplay` UX concept. It is no longer the active product architecture. Its history remains in Git so we can learn from it without carrying Godot into the engine.

GLFW 3.5.1 is currently used only as a replaceable low-level desktop window/input backend behind Seed's `IPlatform` interface. GLFW types and key codes are translated at the backend boundary and are not exposed to Seed gameplay or project content.

Dear ImGui is used only inside `SeedStudio` as an editor-widget drawing dependency. `SeedEngine` and `SeedRuntime` do not depend on ImGui, and Seed owns the editor layout, selection model, component editing and authoring workflows.

nlohmann/json is used only as the low-level parser/writer behind Seed-owned human-readable project and scene formats. Seed defines their schemas, versioning rules and runtime meaning.

See `docs/DEPENDENCIES.md` for the dependency policy.

## What exists now

### Seed Core

- standalone C++20 `SeedEngine` library
- engine lifecycle
- frame timing / frame index
- headless mode
- automated tests
- Windows / Linux / macOS CI

### Seed Scene

- runtime entity IDs
- stable UUID-style persistent entity IDs
- entity creation/destruction
- entity names
- typed component storage
- component queries
- entity iteration for tooling
- reusable gameplay component data

Current components include:

- `TransformComponent`
- `CameraComponent`
- `MeshComponent`
- `MaterialComponent`
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

### Seed Renderer

- `IRenderer`
- renderer factory
- first OpenGL backend
- runtime OpenGL function loading
- shaders and shader error reporting
- vertex/index buffers
- indexed mesh drawing
- model/view/projection matrices
- depth testing
- framebuffer resize handling
- vsync and presentation
- scene-driven `RenderSystem`
- explicit editor-camera rendering for Seed Studio

### Seed Projects + Scenes

Seed now owns two initial persistence formats:

```text
MyGame.seedproject
Scenes/
└─ Main.seedscene
```

Implemented:

- human-readable `.seedproject` files
- human-readable `.seedscene` files
- format version fields
- project name + startup scene
- stable entity UUID persistence
- Transform serialization
- Camera serialization
- Mesh/Material asset IDs
- Interactable, Door, Health and Inventory serialization
- safe scene round-trip loading
- built-in runtime resource rebinding after load
- editor-only viewport camera excluded from game scene files
- automated persistence round-trip tests

Runtime GPU handles are deliberately **not** written to disk. Scene files store stable asset IDs such as `builtin:cube` and `builtin:seed_default`.

### Seed Studio v0

- native 3D editor window
- Seed-styled top menu
- **World** panel with entity selection
- central **3D Viewport** area
- **Inspector** panel
- live Transform editing
- Camera property editing
- Mesh / Material inspection
- **Assets** panel placeholder
- editor fly camera: RMB + mouse, WASD, Q/E, Shift, mouse wheel
- first scene-driven 3D Seed Cube
- New Project / Open Project
- Save / Save As
- New Scene
- dirty-document `*` indicator
- Create Empty Entity / Cube
- Duplicate (`Ctrl+D`)
- Delete (`Del`)
- project status feedback
- `+ Add Gameplay` entry point reserved for the beginner workflow

### Applications

- `SeedRuntime`
- `SeedStudio`

Both are standalone Seed executables linked to the same Seed Engine library.

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
│  │  ├─ math/
│  │  ├─ platform/
│  │  ├─ project/
│  │  ├─ render/
│  │  └─ scene/
│  └─ src/
├─ runtime/                # Seed Runtime executable
├─ editor/                 # Seed Studio + editor-only UI layer
├─ tests/                  # Core + persistence tests
├─ docs/                   # Architecture, dependency policy and roadmap
├─ CMakeLists.txt
└─ README.md
```

## Build

Requirements:

- CMake 3.24+
- a C++20 compiler
- Git access during initial CMake configure so focused dependencies can be fetched

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

Phase 0 (**Seed Core**) is complete.

Phase 1 (**Platform Layer**) is complete for the first desktop backend and has been manually verified on Windows.

Phase 2 (**Renderer Foundation**) renders scene-driven 3D entities with perspective cameras, transforms, depth testing, GPU buffers and shaders.

Phase 3 (**Projects + Scene Persistence**) now has working `.seedproject` / `.seedscene` formats, persistent UUIDs, stable built-in asset IDs, save/load and automated round-trip tests.

The first major slice of Phase 4 (**Seed Studio v0**) is also working: World selection, live Inspector editing, project save/open, create/duplicate/delete entity operations, and the 3D editor camera all run in the native Seed Studio application.

The next product-defining milestone is Phase 5: making **`+ Add Gameplay` functional** so creators can attach Seed-native concepts such as Interactable, Health and Door directly from the Inspector.

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

**Pre-alpha — independent 3D engine + visual editor + first Seed-owned project/scene persistence.**
