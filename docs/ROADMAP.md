# Seed Engine Roadmap

This roadmap prioritizes **owning the engine architecture** while avoiding unnecessary reinvention of low-level libraries.

## Phase 0 — Seed Core

Status: **complete**

Delivered:

- C++20 engine library
- CMake build
- Seed Runtime executable
- Seed Studio executable
- Entity IDs
- Scene ownership
- typed component storage
- basic gameplay component data
- automated tests
- Windows / Linux / macOS CI

Exit condition: Seed Core compiles and tests on Windows, Linux and macOS.

Result: **passed**.

## Phase 1 — Platform Layer

Status: **implemented / native launch verification next**

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

Remaining before Phase 1 is fully signed off:

- manually launch the native Seed Studio and Seed Runtime windows on a desktop machine
- add gamepad event translation
- add platform file-system path helpers

Exit condition: `SeedStudio` and `SeedRuntime` each open a Seed-owned window and run an engine frame loop.

## Phase 2 — Renderer Foundation

Status: **started**

Goal: first visible Seed-rendered 3D scene.

Delivered so far:

- Seed-owned `IRenderer` interface
- `RendererBackend` selection
- renderer factory
- first OpenGL backend
- renderer lifecycle owned by Seed Core
- graphics context selected through Seed platform configuration
- OpenGL entry points loaded at runtime
- framebuffer resize handling
- clear color
- frame presentation
- vsync control

Next:

- renderer compile validation across Windows / Linux / macOS
- triangle
- vertex/index buffers
- shader abstraction
- depth state
- camera matrices
- mesh handles
- basic mesh rendering
- basic materials

Exit condition: Seed Runtime renders multiple entities from Seed Scene data.

## Phase 3 — Assets + Scene Files

Goal: Seed projects become persistent and portable.

Planned work:

- `.seedproject`
- `.seedscene`
- asset IDs
- asset database
- model import pipeline
- texture import pipeline
- scene serialization/versioning
- prefab-style reusable objects

Exit condition: create a scene, save it, close Seed, reopen it, and reproduce the same world.

## Phase 4 — Seed Studio v0

Goal: Seed has its own visual editor.

Planned work:

- dockable editor shell
- 3D viewport
- hierarchy/world browser
- inspector
- transform gizmos
- asset browser
- create/delete/duplicate entity
- play/stop
- undo/redo

Exit condition: build and edit a simple 3D scene without touching code.

## Phase 5 — Gameplay Authoring

Goal: Seed becomes meaningfully easier than traditional engines.

Planned work:

- Add Gameplay panel
- Player preset
- Interactable
- Health
- Inventory
- Pickup
- Door / Lock / Key
- beginner-readable validation
- events/actions

Exit condition: reproduce the original key-and-door prototype entirely inside Seed Studio using Seed-native systems.

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
- save/load
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
