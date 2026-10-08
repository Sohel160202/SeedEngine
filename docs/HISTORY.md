# Seed Engine Prototype History

## Prototype 001 — Godot authoring experiment

Seed's first experiment used Godot 4.x to quickly test a beginner-facing **Add Gameplay** workflow with concepts such as Interactable, Inventory, Pickup, Door and Health.

That prototype proved an important product idea: creators should add game behaviour using game-design language instead of manually wiring engine implementation details.

It also exposed a strategic problem: continuing on that path would make Seed a Godot plugin/framework rather than its own game engine.

The experiment ended at commit:

`5f940cca726e8da15cff1c05e7695cb4dc5eb7e7`

The files were intentionally removed from the active tree after the architectural pivot. Git history preserves the complete prototype.

## Pivot — Independent Seed Engine

From this point forward Seed is built as a standalone C++20 engine with its own:

- engine core
- scene/entity/component model
- runtime
- Seed Studio editor
- gameplay framework
- project/scene formats
- subsystem interfaces

Third-party libraries may be used behind Seed-owned low-level interfaces, but another game engine will not define Seed's runtime or authoring model.
