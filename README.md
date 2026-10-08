# Seed Engine

> **Grow your idea into a game.**

Seed Engine is a beginner-first game engine being built as its **own engine**, not as a plugin or authoring layer for another game engine.

The goal is simple:

**A new creator should think about the game they want to make, not the engine plumbing required to make it.**

Seed should make polished 3D games approachable without trapping users in a toy tool. Beginners work with game concepts such as **Player**, **Door**, **Enemy**, **Inventory**, **Quest**, and **Dialogue**. Advanced users can progressively access deeper logic, scripting, native systems, rendering, and engine extensions.

## Direction

Seed now owns its core architecture:

- **Language:** C++20
- **Build system:** CMake
- **Seed Core:** application lifecycle, services, diagnostics
- **Seed Scene:** entities, components, scene ownership, serialization
- **Seed Runtime:** runs Seed games
- **Seed Studio:** our own editor and authoring environment
- **Seed Gameplay:** beginner-facing reusable gameplay concepts
- **Seed project/scene formats:** owned by Seed
- **Rendering, platform, physics and audio:** accessed through Seed-owned abstractions

Seed may use focused third-party libraries for low-level jobs such as window creation, graphics API loading, physics, model decoding, image decoding, audio codecs, and UI rendering. Those libraries do **not** define Seed's object model, project format, gameplay framework, editor, or runtime architecture.

## Important architecture rule

**Seed is not built on another game engine.**

The earlier Godot prototype proved the `Add Gameplay` UX concept. It is no longer the active product architecture. Its history remains in Git so we can learn from it without carrying Godot into the engine.

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
│  └─ src/
├─ runtime/                # Seed game runtime executable
├─ editor/                 # Seed Studio executable
├─ tests/                  # Core engine tests
├─ docs/                   # Architecture and roadmap
├─ CMakeLists.txt
└─ README.md
```

## Current milestone — Seed Core

The first independent milestone is deliberately low-level and small:

- compile Seed as a standalone C++ engine library
- create and destroy Seed entities
- attach typed Seed components
- maintain a Seed scene
- boot a Seed runtime executable
- boot a Seed Studio executable
- prove both applications use the same Seed Engine library
- keep all of this independent of another game engine

After that we add the first platform/window layer, renderer abstraction, math library, asset system, scene serialization, and the first visible Seed Studio viewport.

## Build

Requirements:

- CMake 3.24+
- a C++20 compiler

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Current executables:

```text
SeedRuntime
SeedStudio
```

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

**Pre-alpha — independent engine foundation.**

The project has pivoted away from the earlier Godot-based authoring proof-of-concept. Active development now targets a standalone C++ Seed Engine, Seed Runtime and Seed Studio.
