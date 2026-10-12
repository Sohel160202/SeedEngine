# Seed Engine Roadmap

Seed is being built as its **own beginner-first game engine**. We use focused low-level libraries where useful, but Seed owns the editor, runtime, scene/component model, project formats, gameplay systems, physics queries, animation data and renderer abstractions.

## Phase 0 — Seed Core

Status: **complete**

Delivered:

- C++20 engine library + CMake
- Seed Runtime executable
- Seed Studio executable
- runtime entity IDs + stable persistent IDs
- typed component storage
- Seed Scene ownership
- automated tests
- Windows / Linux / macOS CI

Result: **passed**.

## Phase 1 — Platform Layer

Status: **complete for first desktop backend**

Delivered:

- Seed-owned `IPlatform`
- GLFW hidden behind the platform adapter
- native desktop windows
- Seed-owned keyboard / mouse event model
- quit / resize / focus events
- frame timing and application loop
- headless test mode
- Windows / Linux / macOS builds

Still useful later:

- gamepad translation
- native file dialogs / richer OS helpers

Result: **passed**.

## Phase 2 — Renderer Foundation

Status: **substantial first renderer working**

Delivered:

- Seed-owned `IRenderer`
- first OpenGL backend
- runtime OpenGL entry-point loading
- vertex/index buffers + shaders
- Camera / Mesh / Material scene components
- Seed math layer
- scene-driven `RenderSystem`
- editor viewport camera
- PBR-style material inputs
- base-color, metallic/roughness and normal maps
- sky/environment lighting
- directional lighting + shadow map pass
- imported static glTF meshes/materials
- Linux renderer smoke test in CI
- renderer-neutral `SkinnedVertex` / `SkinnedMeshDesc` resource definition staged for character skinning

Next renderer milestone:

- material/texture quality parity for animated characters
- skinned shadow filtering and performance tuning
- runtime character preview in Seed Studio

## Phase 3 — Projects + Assets + Scene Persistence

Status: **working**

Delivered:

- `.seedproject`
- `.seedscene`
- human-readable versioned JSON
- Transform / Camera / lighting / hierarchy persistence
- gameplay component persistence
- Seed model asset IDs: `model:`
- Seed animated-character asset IDs: `character:`
- static `.gltf` / `.glb` import
- PBR material/texture import
- animated `.gltf` / `.glb` CPU import
- external `.gltf` dependency discovery
- stable runtime asset cache
- animated character cache containing skinned vertices, Skeleton, clips and material data

Next:

- asset metadata database
- source/reimport tracking
- prefabs / reusable objects
- format migrations
- native file-picker helpers

## Phase 4 — Seed Studio

Status: **working editor, actively expanding**

Delivered includes:

- Seed-styled editor shell
- 3D viewport
- World hierarchy
- Inspector
- Assets panel / import workflow
- live transform editing
- transform gizmos
- create / duplicate / delete
- project / scene open-save flow
- Play / Stop
- Undo / Redo
- editor-only viewport camera
- imported model authoring
- Add Gameplay workflow

Next editor polish:

- entity rename UX
- selection outline
- cleaner player/camera discovery
- dedicated **Create → Player → Third Person** action
- richer asset browser/reimport status
- dock/layout polish

## Phase 5 — Gameplay Authoring

Status: **first product-defining slice working**

Delivered:

- functional **+ Add Gameplay** workflow
- Interactable
- Health
- Inventory
- Pickup
- Door / Lock / Key requirements
- gameplay persistence
- First-Person Player
- Third-Person Player
- automatic Player dependencies
- beginner-readable Player inspector
- Play Mode integration

The original Seed proof loop can now be represented natively:

```text
Player
  ↓
Pick up Key
  ↓
Inventory receives item
  ↓
Locked Door checks requirement
  ↓
Door opens
```

Next:

- reusable item definitions
- dialogue
- quests
- event/action rules
- saveable gameplay state
- beginner-facing validation/fix actions across more systems

## Phase 6 — First Adventure

Status: **active**

### Third-Person foundation — delivered

- Seed-native Third-Person view mode
- camera-relative movement
- sprint
- gravity / jumping / grounding
- character collision
- character rotates toward travel direction
- orbit yaw/pitch
- configurable camera distance / height / shoulder offset
- camera-aware interaction targeting
- interaction reach measured from the character, not the camera
- Seed Physics camera-wall probe
- orbit camera automatically collapses in front of solid geometry
- Third-Person settings persist in `.seedscene`
- cross-platform regression tests

### Animation foundation — delivered

Seed now owns:

- Skeleton / Joint data
- bind-local transforms
- inverse-bind matrices
- animation clips and per-joint tracks
- translation / rotation / scale keys
- quaternion interpolation
- looping / clamped clips
- hierarchy pose solving
- skin matrix palette generation
- animated glTF skin import
- `JOINTS_0` / `WEIGHTS_0` import
- four influences per vertex
- LINEAR glTF animation channels
- runtime `character:` asset cache
- 96-joint skinning-v0 limit enforced before rendering
- generated animated-character test fixture in CI

Important: **animated characters are now visually skinned by the OpenGL renderer.** The camera pass and shadow pass both consume the evaluated joint palette, so deformation and silhouettes stay synchronized.

### First Adventure still needs

- material/texture quality parity for animated characters
- visible character preview and authoring workflow in Seed Studio
- Idle / Walk / Run / Jump / Fall locomotion preset
- animation blending
- dialogue
- quests
- enemy AI/navigation
- audio
- game HUD/UI
- game save/load
- build/export workflow

Exit condition: a small polished third-person adventure built in Seed Studio without writing game code.

## Phase 7 — Seed Logic

Goal: let beginners build custom gameplay without conventional programming.

```text
WHEN Player enters Forest
IF Quest MissingGirl is Active
AND Time is Night
THEN Spawn Wolf x3
AND Play WolfHowl
AND Update Quest
```

Seed Logic should translate into inspectable Seed runtime behavior rather than opaque generated code.

## Long-term

- additional renderer backends
- plugin / SDK model
- multiplayer
- navigation
- richer animation graphs
- visual effects
- packaging for more platforms
- template/game-kit ecosystem
- Game Director overview
- optional AI-assisted authoring built on visible Seed systems

## Product test

Seed succeeds when a complete beginner can make something that looks and feels like a real game **without first becoming a game-engine programmer**.
