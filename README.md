# Seed Engine

> **Grow your idea into a game.**

Seed Engine is an experimental beginner-first game creation environment built around one principle:

**A new creator should think about the game they want to make, not the engine plumbing required to make it.**

Seed is intended to make polished 3D games approachable without trapping users in a toy engine. Beginners start with gameplay concepts such as **Player**, **Door**, **Enemy**, **Inventory**, **Quest**, and **Dialogue**. As their skills grow, Seed should progressively reveal deeper logic, scripting, and native extension points.

## Product direction

Seed is not trying to build a renderer, physics engine, importer, audio stack, and platform layer from scratch in version one. The first product uses a proven open-source runtime foundation and focuses our engineering on the part we want to reinvent: **game authoring**.

Current prototype direction:

- **Runtime foundation:** Godot 4.x
- **Seed authoring layer:** custom editor tools and Seed-specific project metadata
- **Gameplay framework:** reusable components/systems rather than tutorial-style scripts
- **Advanced extension path:** GDScript first, native C++/GDExtension where it provides real value
- **First target:** desktop third-person adventure games

This direction is intentionally replaceable. Seed-owned project data and gameplay concepts should remain separated from runtime-specific implementation wherever practical.

## The Seed mental model

Seed organizes game creation around three ideas.

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

The first vertical slice will include:

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

## What already works

The repository now contains the first functional Seed gameplay slice:

- `SeedInteractable` — reusable interaction entry point and prompt text
- `SeedInventory` — item storage, add/remove/query API, inventory events
- `SeedPickup` — converts a world object into a collectible item
- `SeedDoor` — rotating/sliding doors with optional key requirements
- `SeedHealth` — health, damage, healing, death, and related events
- `SeedPlayerInteractor` — camera-based interaction targeting with prompt focus events
- **Add Gameplay editor dock** — select an object and add Seed capabilities without attaching scripts manually
- **First Seed demo** — collect a key and use it to open a locked door

## Run the current prototype

1. Install a current Godot 4.x editor.
2. Clone or download this repository.
3. Import/open the repository root as a Godot project using `project.godot`.
4. Confirm the **Seed Engine** editor plugin is enabled under **Project → Project Settings → Plugins**.
5. Run the project.
6. In the **First Seed** demo:
   - `WASD` — move
   - `Mouse` — look
   - `E` — interact
   - collect the gold key
   - approach the door and open it

The editor plugin automatically creates Seed's default `seed_interact` input action when necessary. The runtime interaction component also has an `E` fallback for the prototype.

## Add Gameplay workflow

The first authoring workflow is deliberately simple:

1. Select a scene object.
2. Open the **Seed** dock.
3. Click **Interaction**, **Inventory**, **Door**, **Health**, **Pickup**, or **Player Interaction**.
4. Seed adds the required gameplay component as a child node.
5. Configure the component through ordinary inspector properties.
6. Press Play.

`Door` and `Pickup` automatically ensure the selected object also has an interaction component.

## Progressive complexity

Seed should never punish users for becoming more advanced.

1. **Designer** — presets, properties, toggles, gameplay components
2. **Logic** — readable event/condition/action authoring
3. **Visual scripting** — advanced graph-level control where appropriate
4. **Code** — scripts, extensions, and native systems

## Repository layout

```text
SeedEngine/
├─ addons/seed_engine/
│  ├─ runtime/components/  # Reusable Seed gameplay components
│  ├─ runtime/player/      # Player-facing Seed runtime systems
│  ├─ plugin.cfg
│  └─ seed_plugin.gd       # Seed Add Gameplay editor dock
├─ docs/                   # Product, UX, architecture, and engineering decisions
├─ examples/first_seed/    # Current playable vertical-slice demo
├─ project.godot           # Seed Engine development project
└─ README.md
```

## First milestone — First Seed

The current milestone proves one complete beginner-readable gameplay loop:

```text
Player
  ↓
looks at Key
  ↓
[E] Pick up Cabin Key
  ↓
SeedInventory receives CabinKey
  ↓
Player looks at locked Door
  ↓
SeedDoor checks SeedInventory
  ↓
Door opens
```

This is intentionally small. The important part is that the same components can be authored through **Add Gameplay** rather than manually wiring bespoke scripts.

## Next milestone

The next Seed milestone is **First Adventure**:

- reusable Seed Player preset
- third-person camera/controller
- interaction prompt system as a reusable engine feature rather than demo-only UI
- item definitions instead of raw string IDs
- beginner-friendly Door inspector UX
- simple dialogue component
- simple quest component
- saveable component and save/load service
- one short playable adventure built from those systems

## Status

**Pre-alpha — first functional vertical slice implemented.**

Seed is now beyond the concept-only stage: the repository contains a runnable development project, editor authoring tools, reusable gameplay components, and a small playable proof of the core workflow.
