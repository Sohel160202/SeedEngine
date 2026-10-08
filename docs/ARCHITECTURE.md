# Seed Engine Architecture

## Core principle

Seed owns the engine-facing concepts that make Seed a game engine:

- application lifecycle
- world/scene ownership
- entity/component model
- project and scene formats
- gameplay framework
- runtime
- editor / Seed Studio
- asset identity and metadata
- input/action model
- scripting/logic model
- subsystem interfaces

Focused third-party libraries may implement low-level jobs, but they must sit **behind Seed-owned interfaces**.

## Architectural boundary

```text
Seed Studio
    |
    v
Seed Project / Scene Data
    |
    v
Seed Engine Core
    |
    +-- Scene / Entity / Components
    +-- Gameplay Framework
    +-- Asset System
    +-- Input / Events
    +-- Serialization
    +-- Runtime Services
    |
    v
Seed Subsystem Interfaces
    |
    +-- Platform backend
    +-- Renderer backend
    +-- Physics backend
    +-- Audio backend
    +-- Navigation backend
```

No gameplay component should know which graphics API, window library, physics library, or editor UI toolkit is being used.

## 1. Seed Core

Responsibilities:

- engine startup/shutdown
- service ownership
- logging and diagnostics
- frame lifecycle
- timing
- IDs and common types

## 2. Seed Scene

Seed uses entities composed from components.

Initial requirements:

- stable `EntityId`
- entity names for authoring/debugging
- create/destroy operations
- typed component storage
- add/get/remove/has component operations
- scene ownership
- eventual parent/child relationships
- eventual `.seedscene` serialization

The scene model is a Seed concept. It must not mirror another engine's node hierarchy.

## 3. Seed Gameplay

Gameplay features are higher-level components/systems designed for creators.

Initial concepts:

- Transform
- Interactable
- Health
- Inventory
- Pickup
- Door
- Dialogue
- Quest
- Enemy
- Saveable

The beginner should see `Door`, not a collection of low-level scripts needed to simulate a door.

## 4. Seed Runtime

The runtime executable loads a Seed project and runs the game.

It must use the same engine library as Seed Studio.

Long-term runtime responsibilities:

- project boot
- scene loading
- simulation/update loop
- rendering
- physics
- audio
- input
- save data
- platform packaging

## 5. Seed Studio

Seed Studio is our own editor application.

Planned surfaces:

- viewport
- hierarchy/world browser
- inspector
- asset browser
- Add Gameplay
- Game Director
- Seed Logic
- project settings
- build/export

Seed Studio edits Seed data. It does not author another engine's project files.

## 6. Subsystem backends

We will not waste years rewriting solved low-level libraries simply to claim purity.

Acceptable examples include using focused libraries for:

- window/input plumbing
- Vulkan/Direct3D/OpenGL loading
- physics
- image/model decoding
- audio codecs
- editor GUI widgets

The rule is that backends are replaceable and do not define Seed's public authoring model.

## Dependency direction

Dependencies point inward toward Seed abstractions:

```text
Gameplay -> Scene -> Core
Renderer backend -> Renderer interface -> Core
Physics backend -> Physics interface -> Core
Studio UI -> Engine/Editor APIs -> Core
```

Core must never depend on gameplay or editor code.

## File formats

Planned Seed-owned formats:

- `.seedproject` — project manifest
- `.seedscene` — scene/world data
- `.seedasset` — optional imported-asset metadata
- `.seedlogic` — future Seed Logic graphs/rules

The on-disk representation can evolve, but these formats belong to Seed and are versioned by Seed.

## Codebase rules

- C++20 for the engine foundation.
- Prefer composition over giant inheritance trees.
- Runtime code must not depend on editor code.
- Beginner terminology comes before implementation terminology.
- Third-party APIs stay behind wrappers/interfaces.
- Avoid global state where ownership can be explicit.
- Components are data-first; systems own cross-entity behavior.
- Defaults should produce useful behavior immediately.
- Advanced internals remain inspectable and debuggable.
- Every subsystem and gameplay component should become testable in isolation.

## Historical note

The first Seed experiment used Godot to test the `Add Gameplay` authoring idea. That prototype successfully taught us about the desired UX, but Seed is no longer architected as a Godot plugin or Godot-based engine layer. Git history preserves that experiment.
