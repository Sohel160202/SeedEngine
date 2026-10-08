# Seed Engine

> **Grow your idea into a game.**

Seed Engine is an experimental beginner-first game creation environment built around one principle:

**A new creator should think about the game they want to make, not the engine plumbing required to make it.**

Seed is intended to make polished 3D games approachable without trapping users in a toy engine. Beginners start with gameplay concepts such as **Player**, **Door**, **Enemy**, **Inventory**, **Quest**, and **Dialogue**. As their skills grow, Seed should progressively reveal deeper logic, scripting, and native extension points.

## Product direction

Seed is not trying to build a renderer, physics engine, importer, audio stack, and platform layer from scratch in version one. The first product will use a proven open-source runtime foundation and focus our engineering on the part we want to reinvent: **game authoring**.

The current prototype direction is:

- **Runtime foundation:** Godot 4.x
- **Seed authoring layer:** custom editor tools and Seed-specific project metadata
- **Gameplay framework:** reusable components/systems rather than tutorial-style scripts
- **Advanced extension path:** GDScript first, native C++/GDExtension where it provides real value
- **First target:** desktop third-person adventure games

This direction is intentionally replaceable. Seed-owned project data and gameplay concepts should remain separated from runtime-specific implementation wherever practical.

## The Seed mental model

Seed organizes game creation around three ideas:

### World
What exists?

Characters, environments, props, items, levels, cameras, lights.

### Behaviour
What can things do?

Move, interact, fight, talk, collect, open, chase, trade.

### Rules
What happens when something occurs?

Quests, conditions, progression, events, victory/defeat, game state.

## Seed v0.1 goal

A person with no programming experience should be able to create a small, presentable third-person 3D game without writing code.

The first vertical slice should include:

- Third-person player + camera
- Interaction system
- Health and damage
- Pickups and inventory
- Doors, locks, and keys
- Dialogue
- Simple quests
- Basic enemy AI
- Save/load
- HUD and pause/settings UI
- One complete sample game made entirely with Seed's beginner-facing workflow

## Progressive complexity

Seed should never punish users for becoming more advanced.

1. **Designer** — presets, properties, toggles, gameplay components
2. **Logic** — readable event/condition/action authoring
3. **Visual scripting** — advanced graph-level control where appropriate
4. **Code** — scripts, extensions, and native systems

## Repository layout

```text
SeedEngine/
├─ addons/seed_engine/     # Initial Seed editor/plugin prototype
├─ docs/                   # Product, UX, architecture, and engineering decisions
├─ examples/               # Seed-built example projects/vertical slices
└─ README.md
```

## First milestone — Seed Prototype

The first prototype is intentionally small. It should prove that the Seed workflow feels meaningfully simpler than a traditional game engine.

Our first interaction loop:

1. Select an object in a scene.
2. Open **Add Gameplay**.
3. Choose a gameplay concept such as **Door** or **Health**.
4. Configure it using plain-language properties.
5. Press Play and have the feature work immediately.

If this loop feels great, we have the beginning of Seed Engine.

## Status

**Pre-alpha / foundation stage.**

The architecture, interaction language, editor prototype, and first gameplay components are being established now.
