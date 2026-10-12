# Seed Engine

> **Grow your idea into a game.**

Seed Engine is a beginner-first 3D game engine being built as its **own engine**, not as a plugin or authoring layer for another game engine.

The goal is simple:

**A new creator should think about the game they want to make, not the engine plumbing required to make it.**

Seed presents game concepts such as **Player**, **Door**, **Pickup**, **Inventory**, **Enemy**, **Quest** and **Dialogue**, while progressively exposing deeper systems to advanced creators.

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

Seed is now well past the empty-engine stage. It has its own desktop applications, editor, renderer, project/scene formats, asset import, gameplay authoring, first/third-person controllers, character collision, camera collision, animation, combat feedback, semantic HUD state, save slots, and a standalone export path.

The next product milestone is a complete asset-independent vertical slice: a beginner can open a project, play through interaction, combat, dialogue, quest progression, save/load, and export the result without writing engine code.

## v0.1 product target

A complete beginner should be able to create a small, presentable third-person 3D game without writing code.

The vertical slice is growing toward:

- Third-Person Player preset
- imported animated character
- Idle / Walk / Run / Jump / Fall locomotion
- interaction
- health/damage
- pickups/inventory
- doors/locks/keys
- dialogue
- quests
- basic enemy AI
- HUD/UI
- audio
- save-game support
- build/export
- one complete asset-independent game authored through Seed Studio\n\nThe current reference project is `samples/SeedAdventure`, which exercises player movement, interaction, pickup/inventory, locked doors, dialogue, quests, health, combat authoring, save progression, and standalone runtime launch.

## Status

**Pre-alpha — independent 3D engine + visual editor + gameplay runtime + physics + Third-Person Player + static/animated glTF CPU asset pipeline.**
