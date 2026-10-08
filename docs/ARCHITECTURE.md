# Seed Engine Architecture

## Core principle

Seed owns the **authoring model**. The underlying runtime owns low-level engine services.

This separation keeps Seed focused on beginner-friendly creation while preserving the option to evolve the runtime later.

## Layers

### 1. Seed Authoring Layer
The beginner-facing product.

Responsibilities:
- Add Gameplay workflow
- Inspector/property UX
- Game templates
- Seed Logic authoring
- Validation and beginner-friendly errors
- Project overview / Game Director
- Build/export presets

### 2. Seed Gameplay Framework
Reusable runtime-neutral gameplay concepts.

Initial concepts:
- Interactable
- Health
- Damageable
- Pickup
- Inventory
- Door
- Lock / Key Requirement
- Dialogue
- Quest
- Enemy
- Saveable

Every concept should have:
- a small, explicit data model
- editor configuration
- runtime behavior
- predictable events
- serialization support where needed
- validation

### 3. Runtime Adapter
Maps Seed concepts to the host runtime.

Initial adapter target: Godot 4.x.

The adapter should handle:
- nodes/scenes
- physics
- navigation
- animation
- audio
- UI
- input
- persistence
- packaging/export

Seed project metadata should avoid unnecessary dependence on host-specific node paths or implementation details.

### 4. Advanced Extension Layer
For creators who outgrow the beginner workflow.

Planned access levels:
- Seed Logic
- GDScript
- GDExtension / C++

## Seed object model

A Seed-authored gameplay object consists conceptually of:

```text
SeedObject
├─ Presentation
│  ├─ model / sprite
│  ├─ animation
│  └─ audio
├─ Gameplay Components
│  ├─ Interactable
│  ├─ Health
│  └─ ...
└─ Rules / Events
   ├─ conditions
   └─ actions
```

A runtime adapter may represent this differently internally, but the beginner-facing model should remain stable.

## First prototype

The first proof-of-concept should support one polished loop:

1. Add a 3D object to a scene.
2. Select it.
3. Use Seed's **Add Gameplay** panel.
4. Add `Door`.
5. Set opening type, angle, duration, optional required key, and save-state behavior.
6. Press Play.
7. Interact with the door using Seed's player interaction system.

The second component should be `Health`, because it proves that Seed can attach generic reusable gameplay capability to arbitrary objects.

## Rules for the codebase

- Prefer composition over giant inheritance trees.
- Beginner-facing terminology comes before engine terminology.
- Runtime internals must not leak into beginner UI unless absolutely necessary.
- Defaults must produce usable behavior immediately.
- Advanced settings should be hidden behind deliberate disclosure.
- Generated/configured systems must remain inspectable and debuggable.
- No AI-generated opaque code as the primary authoring model.
- Every Seed component must eventually be testable in isolation.
