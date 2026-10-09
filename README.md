# Seed Engine

> **Grow your idea into a game.**

Seed Engine is a beginner-first 3D game engine being built as its **own engine**, not as a plugin or authoring layer for another game engine.

The goal is simple:

**A new creator should think about the game they want to make, not the engine plumbing required to make it.**

Seed presents game concepts such as **Player**, **Door**, **Pickup**, **Inventory**, **Enemy**, **Quest** and **Dialogue**, while progressively exposing deeper logic and native systems to advanced creators.

## Architecture

Seed owns its core product architecture:

- **Language:** C++20
- **Build system:** CMake
- **Seed Core:** application lifecycle and frame loop
- **Seed Scene:** entities, typed components, persistent identities and scene queries
- **Seed Runtime:** gameplay execution
- **Seed Studio:** Seed's visual editor and authoring environment
- **Seed Gameplay:** beginner-facing gameplay concepts
- **Seed Physics:** Seed-owned collision/character-controller layer
- **Seed Assets:** persistent asset IDs, source importing and runtime resource cache
- **Seed Input:** Seed-owned keyboard/mouse event model
- **Seed Platform:** replaceable native platform abstraction
- **Seed Renderer:** replaceable renderer abstraction and current OpenGL backend
- **Seed Math:** vectors, matrices and 3D transform/camera math
- **Seed project/scene formats:** Seed-owned `.seedproject` and `.seedscene`

Seed may use focused third-party libraries for low-level jobs. Those libraries do **not** define Seed's object model, project format, gameplay framework, asset model, editor, or runtime architecture. See `docs/DEPENDENCIES.md`.

The earlier Godot prototype only proved the original `Add Gameplay` UX idea. It is no longer part of the active product architecture.

## What exists now

### Seed Core + Scene

- standalone C++20 `SeedEngine` library
- runtime entity IDs plus stable UUID-style persistent IDs
- typed component storage and multi-component queries
- deep scene cloning for Play Mode
- headless mode and automated tests
- Windows / Linux / macOS CI

Current component families include transforms, cameras, rendering, gameplay, inventory, pickup/door authoring, player controllers and physics bodies/colliders.

### Platform + Renderer

- native desktop windows and event translation through `IPlatform`
- GLFW hidden behind the current desktop backend
- `IRenderer` abstraction with current OpenGL backend
- runtime OpenGL function loading
- shaders and compile/link diagnostics
- vertex/index buffers and indexed mesh drawing
- UV-capable static mesh vertices
- GPU texture resources
- material base color + base-color texture submission
- model/view/projection transforms
- depth testing, resize handling, vsync and presentation
- scene-driven `RenderSystem`

### Projects + persistence

Seed owns human-readable project and scene formats:

```text
MyGame.seedproject
Scenes/
└─ Main.seedscene
Assets/
└─ Imported/
```

Implemented:

- project name + startup scene
- stable persistent entity IDs
- Transform, Camera, render, gameplay and physics component persistence
- persistent asset IDs instead of GPU handles
- editor-only viewport camera excluded from saved game scenes
- Save / Save As / Open Project
- automated persistence round-trip tests

### Seed Studio

- native Seed Studio desktop application
- World panel and entity selection
- 3D viewport + editor fly camera
- Inspector with live component editing
- Create Empty Entity / Cube
- Create → Player → First Person preset
- Duplicate / Delete
- dirty-document tracking
- project Save/Open workflow
- Play / Stop on a temporary cloned runtime scene
- project Assets panel

### Beginner-facing gameplay authoring

`+ Add Gameplay` currently supports working Seed concepts including:

- Interactable
- Health
- Door / Lock
- Pickup
- Inventory
- Player — First Person
- Solid Collision

Seed automatically supplies important dependencies where appropriate. For example, Door can receive Interactable/Collision without forcing a beginner to assemble engine plumbing manually.

The current runtime loop supports:

```text
Player
  ↓
Interact
  ↓
Pickup key
  ↓
Inventory
  ↓
Locked Door
  ↓
Consume key (optional)
  ↓
Door opens
```

### Physics v0

Seed's first physics layer currently provides:

- static box colliders
- first-person character body
- gravity
- ground detection
- jumping
- floor/wall blocking
- basic wall sliding
- collider movement with moving doors

It is intentionally a character/static-world foundation rather than a full rigid-body solver yet.

### Asset Import v0

Seed's first real content import path supports static glTF 2.0 sources:

```text
.glb / .gltf
   ↓
Seed AssetImporter
   ↓
Seed-owned vertices / indices / texture pixels
   ↓
project-relative model: asset ID
   ↓
Seed GPU mesh + texture resources
```

Current import scope:

- `.glb` and `.gltf`
- first triangle primitive
- POSITION
- optional vertex color
- TEXCOORD_0
- indices
- base-color factor
- base-color texture
- copying imported sources into the Seed project's `Assets/Imported/` folder
- copying relative `.gltf` buffer/image dependencies
- stable project-relative `model:` asset IDs
- runtime resource cache + scene rebinding after reopen
- imported asset references surviving `.seedscene` save/load

TinyGLTF is only the low-level source parser/image-decoding backend. Seed owns the persistent asset model, renderer resources, project layout and editor workflow.

Not yet included in Asset Import v0: skeletal animation, skinning, multiple primitives/materials, full glTF scene-node hierarchy, normal/PBR lighting, or automatic imported-mesh collision.

## Applications

```text
SeedStudio   # creator/editor application
SeedRuntime  # standalone Seed runtime executable
```

Both link to the same Seed Engine library.

## Seed's creator mental model

### World
What exists?

Characters, environments, props, items, lights, cameras and levels.

### Behaviour
What can things do?

Move, interact, fight, talk, collect, open, chase and trade.

### Rules
What happens when something occurs?

Conditions, progression, events, quests, victory, defeat and game state.

## Progressive complexity

1. **Designer** — presets, properties, gameplay concepts
2. **Seed Logic** — readable event / condition / action authoring
3. **Advanced visual logic** — lower-level control where useful
4. **Code** — native C++ and future extension paths

## Repository layout

```text
SeedEngine/
├─ engine/
│  ├─ include/seed/
│  │  ├─ assets/
│  │  ├─ core/
│  │  ├─ gameplay/
│  │  ├─ math/
│  │  ├─ physics/
│  │  ├─ platform/
│  │  ├─ project/
│  │  ├─ render/
│  │  └─ scene/
│  └─ src/
├─ runtime/
├─ editor/
├─ tests/
├─ docs/
├─ CMakeLists.txt
└─ README.md
```

## Build

Requirements:

- CMake 3.24+
- a C++20 compiler
- Git access during initial configure so focused dependencies can be fetched

Ubuntu/Debian also needs the current X11 development headers:

```bash
sudo apt install xorg-dev
```

Build:

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Current direction

Seed has moved beyond the original engine-foundation stage: scene editing, persistence, gameplay authoring, Play Mode, a first-person Player, interaction/inventory/doors and character collision are all part of the active engine.

The current content milestone is **real imported 3D assets and textures**. The next rendering milestone is expected to focus on normals + lighting and richer glTF material/primitive support so Seed can start building its first non-placeholder environment.

## v0.1 product target

A person with no programming experience should eventually be able to create a small, presentable third-person 3D game without writing code.

The creator-facing vertical slice is planned to grow toward:

- first-person and third-person Player presets
- interaction
- health and damage
- pickups and inventory
- doors, locks and keys
- dialogue
- simple quests
- basic enemy AI
- save-game support
- HUD and settings
- imported assets, materials, lighting and audio
- build/export workflow
- one complete game authored through Seed Studio

## Status

**Pre-alpha — independent 3D engine + visual editor + gameplay runtime + physics foundation + first static model/texture import pipeline.**
