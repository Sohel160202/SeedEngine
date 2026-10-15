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
- categorized prefab browser with descriptions and drag-and-drop placement
- Assets panel with project assets, starter GLTF models, and prefab categories
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

- product definitions with genre-specific prices and categories
- shop inventory, stock quantities, capacity, and restocking
- wallet, revenue, expense, and business metrics
- shop hours, simulation clock, day progression, and demand growth
- customer orders, fulfillment progress, arrivals, movement, and queue behavior
- deterministic checkout queues with multi-counter capacity
- reusable staff workers, roles, shifts, payroll, hiring, and upgrades
- station staffing requirements and worker assignments
- visual Staff & Stations roster for assignments, shifts, and wage costs
- visual Products & Stock panel for prices, capacity, and restocking
- visual Customers & Orders panel for queues, demand, and order progress
- visual Business Dashboard for money, performance, shop hours, and upgrades
- simulation-aware no-code Rules with customer, stock, money, and progression events
- one-click Shop Starter Rules and Low Stock Rule presets
- reusable genre preset libraries for shelves, products, checkout stations, staff, customers, and rule packs
- complete reusable starter-shop prefabs for supermarkets, card shops, restaurants, and game stores
- categorized visual prefab browser with descriptions, one-click placement, and viewport drag-and-drop
- original Seed Starter Asset Pack with self-contained GLTF shelves, counters, registers, baskets, and product props
- modular Shop Building presets for floors, walls, aisles, entrances, customer spawn points, and lighting
- shop interaction presets for customer queue and checkout areas with beginner-friendly prompts
- reusable Trigger Area preset with EnterTrigger and ExitTrigger rule events
- shop presets include starter rules publishing CustomerQueueEntered, CheckoutEntered, and area enter/exit events
- Checkout Area can automatically process the first waiting customer when the player enters it
- Customer Queue Area placement controls the actual in-world queue anchor; customer spawning remains gated by Shop Hours
- Complete Shop Setup creates a ready-to-play shop foundation in one action
- Help > First Game Tutorial now guides beginners through the complete shop-simulation loop
- viewport transform snapping with configurable move, rotate, and scale increments
- automatic copying of starter assets into project `Assets/SeedStarterPack`
- static and animated GLTF/GLB asset import with runtime resource binding
- customer purchases, payment events, order completion, and restocking interactions
- one-click Shop Simulator, Supermarket, Card Shop, Restaurant, and Game Store templates
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

Seed is now a working simulation-focused engine foundation rather than an empty renderer prototype. It includes a visual editor, project and scene formats, gameplay authoring, first- and third-person controllers, physics, static and animated GLTF/GLB import, reusable prefabs, shop simulation systems, no-code rules, starter templates, and original starter assets.

The current product milestone is the **Seed Simulation Kit**: a beginner should be able to assemble a small shop or management game by choosing a template, placing prefabs, assigning assets, authoring readable rules, pressing Play, and exporting the result without writing engine code.

## Simulation Kit roadmap

1. **Beginner simulation core — active foundation**
   Variables, simulation events, conditions, actions, stock, money, customers, orders, staff, checkout, upgrades, and progression rules are implemented. More beginner-friendly rule authoring remains.

2. **Reusable prefabs — foundation implemented**
   Seed can generate, save, instantiate, refresh, categorize, and drag-and-drop reusable gameplay prefabs. The next improvement is richer prefab previews and editing.

3. **Asset workflow — foundation implemented**
   Static and animated GLTF/GLB import, runtime binding, project asset discovery, original Seed starter assets, and asset assignment are working. Next: thumbnails, material previews, animation assignment, and a stronger character workflow.

4. **World-building tools — active phase**
   Transform snapping and modular Shop Building presets are implemented. Next: scene collections, trigger volumes, navigation, spawn-point authoring, and richer lighting presets.

5. **No-code game systems**
   UI, dialogue, inventory, quests, objectives, audio, level transitions, save slots, and victory/game-over flows.

6. **Build and export**
   Project settings, validation, asset packaging, platform profiles, and one-click desktop builds.

7. **Dogfooding**
   Build a complete presentable shop simulator entirely inside Seed Studio using only the beginner-facing systems.

The previous adventure scene remains useful as a general engine smoke scene, but the primary reference project is now the Shop Simulator template.

## Status

**Pre-alpha — independent simulation-focused 3D engine with visual authoring, reusable prefabs, GLTF/GLB assets, shop gameplay systems, no-code rules, starter templates, and cross-platform CI builds.**
