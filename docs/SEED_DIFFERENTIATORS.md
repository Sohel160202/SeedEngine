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
- keep Undo/Redo integration

## 2. Human-readable Scene Diff + Merge

Seed scene/project data is already human-readable. Build first-class structural diff/merge on top of it instead of treating source control conflicts as a text-editor problem.

Goals:

- compare entities/components/properties semantically
- show renamed, moved, added and deleted entities
- resolve conflicts inside Seed Studio
- preserve stable persistent entity IDs
- integrate cleanly with Git/Perforce workflows later

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

## 7. Engine Dogfooding as a Product Rule

Every major Seed release should be validated by building and shipping a complete Seed game with it.

A feature is not considered mature only because its API exists. It must work in combination with the rest of the engine through a real project pipeline.

## Priority

Near term:

1. Apply Play Changes
2. semantic scene diff
3. built-in Save Game
4. character animation + first complete Third-Person sample

Later:

5. stable live-code/module reload
6. multiplayer authoring + prediction/rollback
7. collaborative editing/version-control UX

The product question for every feature is:

> Does this remove recurring game-development pain, or is it merely another checkbox?
