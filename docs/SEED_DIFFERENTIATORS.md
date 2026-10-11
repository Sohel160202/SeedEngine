# Seed Differentiators

Seed should not win by copying every Unity or Unreal feature. It should win by making common game-development pain points feel native, safe and understandable.

This document tracks product bets that can become architectural differentiators rather than optional plugins.

## 1. Apply Play Changes

During Play Mode, Seed records property changes separately from the authored scene. On Stop, the creator can review a human-readable diff and selectively apply safe changes back to the edit scene.

Goals:

- never silently lose useful tuning done while playing
- show exactly what changed
- apply individual properties, components or entities
- distinguish runtime-only/transient state from authoring state
- allow useful runtime-spawned objects to be promoted into authored scene objects
- keep Undo/Redo integration

Foundation status: Seed now has an engine-owned semantic `SceneDiff` model with persistent-entity matching, property-level changes, Safe / Review / Runtime State classification and selective application.

## 2. Human-readable Scene Diff + Merge

Seed scene/project data is already human-readable. Build first-class structural diff/merge on top of it instead of treating source control conflicts as a text-editor problem.

Goals:

- compare entities/components/properties semantically
- show renamed, moved, added and deleted entities
- resolve conflicts inside Seed Studio
- preserve stable persistent entity IDs
- integrate cleanly with Git/Perforce workflows later

The same semantic `SceneDiff` foundation used for Play Mode should power this rather than creating a second diff system.

## 3. Gameplay Save System by Default

Saving a game should be an engine concept, not a tutorial every project has to rebuild.

Goals:

- persistent gameplay state declared per component/property
- slots, checkpoints and autosave
- versioned migrations
- async save/load
- world/entity identity handled by Seed
- designer-facing Save Game inspector

## 4. Stable Live Iteration

Fast iteration must not trade correctness for speed.

Goals:

- isolate reloadable gameplay modules
- safe state migration when code/schema changes
- explicit fallback to restart only when required
- no asset corruption from hot reload
- visible diagnostics explaining what can/cannot be reloaded

## 5. Built-in Multiplayer Gameplay Model

Long term, Seed should make ordinary multiplayer gameplay understandable without forcing every project to invent replication conventions.

Goals:

- authority/ownership visible in Studio
- replicated properties/events as first-class concepts
- local prediction + reconciliation helpers
- deterministic/rollback-capable gameplay layer where appropriate
- multiplayer Play Mode with multiple local clients

## 6. Dependency-aware Gameplay Presets

Seed already moves in this direction with + Add Gameplay. Continue until common game features are usable concepts instead of plumbing exercises.

Examples:

- Player automatically receives the dependencies its selected movement mode needs
- Door can supply Interactable + Collision
- Pickup can supply collider/interaction requirements
- Quest/Dialogue/HUD systems expose game concepts, not framework internals

## 7. Seed Doctor — Explain Why It Does Not Work

A beginner should not need years of engine knowledge to discover that one checkbox, collision channel, ownership rule or missing dependency is why a feature fails.

Seed Doctor should inspect the authored game semantically and explain problems in game-language.

Examples:

- `Door cannot be interacted with because Interactable is missing. Add it?`
- `Third-Person camera can pass through this wall because the wall has no Solid Collision.`
- `Pickup requires an item ID before it can be collected.`
- `This replicated action is running on a client but requires authority.`

Goals:

- dependency graph for gameplay concepts
- editor-time validation before Play
- runtime diagnostics that identify the entity/component/property involved
- one-click safe fixes where possible
- plain-language explanation plus an advanced technical view
- never silently change project behavior without showing what Seed fixed

## 8. Time-Travel Gameplay Debugger

Seed should eventually make game-state bugs reproducible instead of asking creators to trigger them again and hope they notice the right frame.

A deterministic-enough recording layer can capture inputs, important gameplay events and semantic state changes while playing.

Goals:

- scrub backward through a recent Play session
- inspect `what changed on this frame?`
- follow an event chain such as Input → Interaction → Inventory → Door
- breakpoint on a property/event change rather than only a source-code line
- compare two runtime moments using the same semantic diff infrastructure
- export a compact reproducible bug trace
- support multiplayer traces later with authority/client timelines

This is deliberately a long-term feature, but Seed's stable IDs and semantic scene/state model should be designed so it remains possible.

## 9. Engine Dogfooding as a Product Rule

Every major Seed release should be validated by building and shipping a complete Seed game with it.

A feature is not considered mature only because its API exists. It must work in combination with the rest of the engine through a real project pipeline.

## Priority

Near term:

1. Apply Play Changes Studio workflow
2. semantic scene diff/merge UX
3. built-in Save Game
4. character GPU skinning + first complete Third-Person sample
5. first Seed Doctor validation rules

Later:

6. stable live-code/module reload
7. multiplayer authoring + prediction/rollback
8. collaborative editing/version-control UX
9. Time-Travel Gameplay Debugger

The product question for every feature is:

> Does this remove recurring game-development pain, or is it merely another checkbox?
