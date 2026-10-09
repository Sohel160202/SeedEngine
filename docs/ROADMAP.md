# Seed Engine Roadmap

This roadmap prioritizes **owning the engine architecture** while avoiding unnecessary reinvention of low-level libraries.

## Phase 0 — Seed Core

Status: **complete**

Delivered:

- C++20 engine library
- CMake build
- Seed Runtime executable
- Seed Studio executable
- runtime entity IDs
- Scene ownership
- typed component storage
- basic gameplay component data
- automated tests
- Windows / Linux / macOS CI

Exit condition: Seed Core compiles and tests on Windows, Linux and macOS.

Result: **passed**.

## Phase 1 — Platform Layer

Status: **complete for first desktop backend**

Goal: Seed opens its own native application window and owns its input/event lifecycle.

Delivered:

- Seed-owned `IPlatform` interface
- GLFW 3.5.1 backend hidden behind `IPlatform`
- native desktop window creation
- Seed-owned keyboard codes and button states
- Seed-owned mouse button codes
- mouse movement / wheel events
- quit / resize / focus events
- frame delta and frame index
- application frame loop
- Escape-to-close behavior in Seed Studio and Seed Runtime
- headless engine mode for tests
- backend code compiles on Windows, Linux and macOS
- manual Seed Studio launch verification on Windows

Still useful later:

- gamepad event translation
- richer platform file-system/native-dialog helpers

Exit condition: `SeedStudio` and `SeedRuntime` each open a Seed-owned window and run an engine frame loop.

Result: **passed**.

## Phase 2 — Renderer Foundation

Status: **first 3D foundation complete**

Goal: first visible Seed-rendered 3D scene.

Delivered:

- Seed-owned `IRenderer` interface
- `RendererBackend` selection
- renderer factory
- first OpenGL backend
- renderer lifecycle owned by Seed Core
- runtime OpenGL entry-point loading
- vertex/index buffers
- shader abstraction and error reporting
- indexed mesh drawing
- depth testing
- model/view/projection matrices
- Seed math layer
- Camera / Mesh / Material components
- scene-driven `RenderSystem`
- explicit Seed Studio viewport camera
- first 3D Seed Cube
- interactive editor fly camera
- manual 3D verification on Windows

Exit condition: Seed Runtime/Studio render entities from Seed Scene data.

Result for the first renderer foundation: **passed**.

Future renderer work will continue throughout development: textures, lighting, render targets, materials, batching, additional backends and production rendering features.

## Phase 3 — Projects + Scene Persistence

Status: **initial persistence milestone complete**

Goal: Seed projects become persistent and portable.

Delivered:

- Seed-owned `.seedproject`
- Seed-owned `.seedscene`
- human-readable, versioned JSON representation
- stable UUID-style persistent entity IDs separate from runtime `EntityId`
- stable asset IDs separate from runtime GPU handles
- project name + startup scene
- Transform persistence
- Camera persistence
- Mesh / Material asset references
- Interactable / Door / Health / Inventory persistence
- safe scene load into temporary scene before replacement
- built-in resource rebinding after load
- editor-only viewport camera excluded from game scene data
- automated project + scene round-trip tests
- Studio Save / Save As / Open Project workflow

Next inside Phase 3:

- asset database
- model import pipeline
- texture import pipeline
- migrations/version upgrades
- prefab-style reusable objects
- native path/file dialog helpers

Exit condition: create a scene, save it, close Seed, reopen it, and reproduce the same world.

Automated result: **passed**. Manual Windows persistence verification is the next check.

## Phase 4 — Seed Studio v0

Status: **active; first editor slice working**

Goal: Seed has its own visual editor.

Delivered:

- Seed-styled editor shell
- 3D viewport
- World hierarchy
- Inspector
- live transform editing
- Camera editing
- Mesh / Material inspection
- Assets placeholder panel
- Create Empty Entity
- Create Cube
- Duplicate entity (`Ctrl+D`)
- Delete entity (`Del`)
- New Project / Open Project
- New Scene
- Save / Save As
- dirty-document indicator
- persistent-ID inspection
- protected editor-only viewport camera

Next:

- rename entity
- transform gizmos
- real asset browser
- selection outline
- play/stop
- undo/redo
- dockable/resizable layout

Exit condition: build and edit a simple 3D scene without touching code.

## Phase 5 — Gameplay Authoring

Status: **next product-defining milestone**

Goal: Seed becomes meaningfully easier than traditional engines.

Next work:

- make `+ Add Gameplay` functional
- Interactable
- Health
- Door / Lock
- Inventory
- Pickup / Key
- Player preset
- beginner-readable validation
- events/actions
- save/load gameplay components through `.seedscene`

Exit condition: reproduce the original **Key → Inventory → Locked Door** prototype entirely inside Seed Studio using Seed-native systems.

## Phase 6 — First Adventure

Goal: build the first complete game in Seed.

Planned work:

- third-person controller
- camera
- collision + physics integration
- dialogue
- quests
- enemy AI
- audio
- HUD/UI
- game save/load
- build/export

Exit condition: a small polished third-person adventure made without writing game code.

## Phase 7 — Seed Logic

Goal: let beginners build custom gameplay without conventional programming.

Model:

```text
WHEN Player enters Forest
IF Quest MissingGirl is Active
AND Time is Night
THEN Spawn Wolf x3
AND Play WolfHowl
AND Update Quest
```

Seed Logic must compile/translate into inspectable runtime behavior rather than opaque generated code.

## Long-term

- extensible renderer backends
- plugin/SDK model
- multiplayer
- navigation
- animation systems
- visual effects
- packaging for more platforms
- template/game-kit ecosystem
- Game Director project overview
- optional AI-assisted authoring built on visible Seed systems

## Product test

Seed succeeds when a complete beginner can make something that looks and feels like a real game **without first becoming a game-engine programmer**.
