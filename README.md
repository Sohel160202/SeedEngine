# Seed Engine

> **Grow your simulation into a game.**

Seed Engine is an independent, beginner-first 3D game engine specialized for building simulation and management games.

Seed is designed for creators who want to make games such as:

- supermarket and grocery simulators
- card-shop and game-store simulators
- restaurant, café and service-business games
- farming, crafting and collection games
- small tycoon and management experiences

It is being built as its own engine, not as a plugin or authoring layer for another game engine.

The goal is simple:

**A new creator should think about the world, customers, products and business they want to simulate—not the engine plumbing required to make it.**

Seed presents approachable concepts such as **Shop**, **Product**, **Inventory**, **Customer**, **Order**, **Wallet**, **Door**, **Pickup**, **Quest** and **Dialogue**, while progressively exposing deeper systems to advanced creators.

## Architecture

Seed owns its product architecture:

- **Language:** C++20
- **Build:** CMake
- **Seed Core:** application lifecycle + frame loop
- **Seed Scene:** entities, typed components, hierarchy and persistent identity
- **Seed Runtime:** gameplay execution
- **Seed Studio:** visual editor / authoring environment
- **Seed Gameplay:** beginner-facing gameplay concepts
- **Seed Physics:** character/static-world collision + reusable queries
- **Seed Animation:** skeletons, clips, poses and skin palettes
- **Seed Assets:** source import, persistent asset IDs and runtime cache
- **Seed Platform:** replaceable native platform layer
- **Seed Renderer:** replaceable renderer abstraction; OpenGL is the first backend
- **Seed Math:** vectors, matrices and 3D transforms
- **Seed formats:** `.seedproject` and `.seedscene`

Focused third-party libraries are allowed for low-level jobs, but they do **not** define Seed's scene model, project format, gameplay architecture, editor, runtime, animation model or renderer API.

The old Godot experiment only proved the original **Add Gameplay** UX idea. It is not part of the active Seed architecture.

## What works today

### Seed Core + Scene

- standalone `SeedEngine` C++ library
- runtime entity IDs + stable persistent IDs
- typed component storage and component queries
- parent/child hierarchy
- deep Scene cloning for Play Mode
- headless tests
- Windows / Linux / macOS CI

### Platform + Renderer

- native Seed Studio / Seed Runtime windows
- Seed-owned input/event model
- GLFW hidden behind `IPlatform`
- Seed-owned `IRenderer`
- OpenGL backend
- vertex/index buffers
- shader compilation/link diagnostics
- textures
- indexed static meshes
- depth testing
- Camera / Mesh / Material components
- PBR-style base-color / metallic / roughness inputs
- base-color maps
- metallic/roughness maps
- normal maps
- sky/environment lighting
- directional lighting
- directional shadow pass
- Linux renderer smoke test in CI

The renderer now supports Seed-native **skinned vertices**, GPU joint-palette uploads, skinned material draws and skinned shadow draws.

### Projects + persistence

Seed owns portable, human-readable project data:

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
- Transform / Camera / lighting / hierarchy persistence
- gameplay + physics persistence
- imported asset references instead of raw GPU handles
- Save / Save As / Open Project
- editor-only viewport camera excluded from saved game scenes
- automated round-trip tests

### Seed Studio

Seed Studio is already a working visual editor, not just a launcher.

Current capabilities include:

- 3D viewport
- World hierarchy
- Inspector
- live transforms
- transform gizmos
- entity create / duplicate / delete
- imported asset workflow
- Assets panel
- project/scene open-save flow
- dirty-state tracking
- Play / Stop using a cloned runtime Scene
- Undo / Redo
- editor fly camera
- **+ Add Gameplay**

### Gameplay authoring

`+ Add Gameplay` currently includes working Seed-native concepts such as:

- Interactable
- Health
- Inventory
- Pickup
- Door / Lock / Key requirements
- Solid Collision
- Player — First Person
- Player — Third Person

Seed automatically supplies important dependencies for beginner-facing presets.

The original proof loop is now a real Seed-native runtime path:

```text
Player
  ↓
Interact
  ↓
Pick up Key
  ↓
Inventory receives item
  ↓
Locked Door checks requirement
  ↓
Door opens
```

## Simulation-first direction

Seed’s first product focus is the simulation game genre. The engine is being shaped around the repeating systems that make these games engaging:

    Player
      ↓
    Interact with shop
      ↓
    Manage products and stock
      ↓
    Serve customers and fulfill orders
      ↓
    Earn money
      ↓
    Restock, expand and improve the business

The simulation foundation currently includes:

- product definitions with prices
- shop inventory and stock quantities
- wallet, revenue and expense tracking
- shop hours and simulation time
- customer orders and fulfillment progress
- automatic customer arrivals
- visible customer movement
- reusable customer capacity limits
- dynamic customer spawning from reusable templates
- population caps and configurable spawn intervals
- queue-length limits and upgrade-based customer growth
- day-based demand growth for management progression
- deterministic checkout queues
- customer purchases and order-completion events
- restocking interactions
- a working Shop Simulator starter template
- simulation data saved in Seed scenes

The current reference project is a small shop simulation where a beginner can place products, restock the shop, observe customers queue and purchase items, and monitor the business HUD.

Simulation systems are designed to remain reusable across different games. A supermarket, card shop, restaurant or game store should use the same underlying concepts with different products, rules, scenes and presentation.

## Third-Person Player

Third Person is a real Seed Player mode rather than an editor camera trick.

Current behavior:

- camera-relative WASD movement
- sprint
- gravity / jumping / grounding
- character collision
- orient character toward travel direction
- orbit yaw + pitch
- configurable distance / height / shoulder offset
- interaction aiming from the gameplay camera
- interaction reach enforced from the character body
- child gameplay Camera
- orbit-camera collision against solid Seed BoxColliders
- camera automatically pulls inward instead of passing through walls
- settings persisted in `.seedscene`

The Studio inspector exposes beginner-facing properties such as **Move Speed**, **Sprint Multiplier**, **Camera Distance**, **Camera Height**, **Shoulder Offset** and **Pitch Limits**.

See `docs/THIRD_PERSON_PLAYER.md`.

## Physics v0

Seed's current physics foundation provides:

- static Box Colliders
- Character Body
- gravity
- ground detection
- jumping
- floor/wall blocking
- basic wall sliding
- moving-door collision participation
- reusable segment-vs-solid-world camera probe

It is intentionally a character/static-world foundation rather than a complete rigid-body simulation yet.

## Static glTF import

Static `.gltf` / `.glb` import currently supports:

- first triangle primitive
- POSITION
- NORMAL or generated normals
- TEXCOORD_0
- optional vertex color
- indices
- base-color factor/texture
- metallic/roughness factor + texture
- normal texture
- external `.gltf` dependency discovery/copying
- persistent `model:` asset IDs
- runtime resource cache + scene rebinding

TinyGLTF is only the source-file adapter. Imported runtime data is converted into Seed-owned structures.

## Seed Animation foundation

Seed now owns its animation model independently of glTF and OpenGL:

- Skeleton
- SkeletonJoint
- bind-local joint transforms
- inverse-bind matrices
- AnimationClip
- per-joint translation / rotation / scale tracks
- quaternion interpolation
- looping and non-looping evaluation
- hierarchy pose solving
- final skin-matrix palette generation

See `docs/ANIMATION.md`.

### Animated glTF import

Animated `.gltf` / `.glb` CPU import currently supports:

- first skinned triangle primitive
- `POSITION`
- `NORMAL` or generated normals
- `TEXCOORD_0`
- optional `COLOR_0`
- `JOINTS_0`
- `WEIGHTS_0`
- four influences per vertex
- skin joint hierarchy
- FLOAT MAT4 inverse-bind matrices
- LINEAR translation / rotation / scale animation channels
- PBR material factors/textures
- external dependency discovery

Animated assets use persistent Seed IDs such as:

```text
character:Assets/Imported/Hero.glb
```

`AssetRuntime` can already resolve/cache a character into:

- Seed `SkinnedVertex` data
- Skeleton
- AnimationClips
- PBR material properties
- texture resources

Seed skinning v0 currently caps characters at **96 joints** before they reach the renderer.

### Important current boundary

**Animated character data imports, caches, evaluates and reaches the OpenGL renderer through a GPU skinning path.**

The renderer consumes Seed-owned vertices and matrix palettes while Seed Animation remains responsible for clip evaluation and pose solving. The remaining product work is authoring and validating a complete character locomotion preset.

## Applications

```text
SeedStudio   # visual creator/editor
SeedRuntime  # standalone Seed game runtime
```

Both use the same Seed Engine library.

## Creator mental model

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

1. **Designer** — presets, properties and gameplay concepts
2. **Seed Logic** — readable event / condition / action authoring
3. **Advanced visual logic** — lower-level control where useful
4. **Code** — native C++ and future extension paths

## Repository layout

```text
SeedEngine/
├─ engine/
│  ├─ include/seed/
│  │  ├─ animation/
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
- C++20 compiler
- Git access during initial configure so focused dependencies can be fetched

Ubuntu/Debian also needs the X11 development headers:

```bash
sudo apt install xorg-dev
```

Build + test:

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Export a runnable package with an optional project:

```bash
cmake -S . -B build -DSEED_BUILD_TESTS=OFF
cmake --build build --config Release
cmake -DSEED_BUILD_DIR=build -DSEED_EXPORT_DIR=dist/MyGame \
  -DSEED_PROJECT_DIR=/path/to/MyGame -DSEED_CONFIG=Release \
  -P tools/export_project.cmake
```

See `docs/EXPORT.md` for the package layout and CPack archive workflow.
See `docs/BEGINNER_WORKFLOW.md` for the complete no-code creator workflow.

## Current direction

Seed is now well past the empty-engine stage. It has its own desktop applications, editor, renderer, project/scene formats, asset import, gameplay authoring, first/third-person controllers, character collision, camera collision, animation, simulation components, customer behavior, checkout queues, HUD state, save slots, and a standalone export path.

The next product milestone is the **Seed Simulation Kit**: a beginner can create a small, complete simulation game without writing engine code.

## Simulation Kit roadmap

The roadmap is intentionally focused:

1. **Beginner simulation core** — variables, conditions, events and actions for stock, money, customers, orders and progression.
2. **Reusable prefabs** — Player, Customer, Product, Pickup, Door, Checkout, NPC and shop stations.
3. **Asset workflow** — drag-and-drop GLTF/GLB assets, thumbnails, previews and animation assignment.
4. **World-building tools** — grid snapping, collections, trigger volumes, navigation, spawn points and lighting presets.
5. **No-code game systems** — UI, dialogue, inventory, quests, objectives, audio, level transitions, save slots and victory/game-over flows.
6. **Build and export** — project settings, validation, asset packaging and one-click desktop builds.
7. **Dogfooding** — build a complete small shop simulator entirely inside Seed Studio.

The previous adventure scene remains useful as a general engine smoke scene, but the primary reference project is now the Shop Simulator template.

## v0.1 product target

A complete beginner should be able to create a small, presentable simulation or management game without writing code.

The first complete vertical slice is growing toward:

- Player preset
- Customer preset with queue behavior
- Product and stock setup
- Shop inventory and wallet
- Checkout interaction
- order fulfillment
- restocking
- simulation HUD
- dialogue and simple quests
- save-game support
- build/export
- one complete shop simulator authored through Seed Studio

## Status

**Pre-alpha — independent 3D engine + visual editor + gameplay runtime + physics + Third-Person Player + static/animated glTF CPU asset pipeline.**
