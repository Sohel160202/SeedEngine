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

Implemented foundation:

- engine-owned semantic `SceneDiff` with persistent-entity matching
- property/component/entity/parent structural changes rather than text-line diffs
- `Safe`, `Review`, and `Runtime State` classification
- exact selective application without copying unrelated runtime state
- runtime-spawned entity promotion support
- Seed Studio `Review Play Changes` modal on Stop
- Safe changes preselected; Review changes opt-in; Runtime State locked out
- human-readable Before/After values and semantic component/property paths
- Seed's temporary fallback Play Player is removed before diffing so engine plumbing never appears as creator work
- editor input and a new Play session are blocked until the pending review is explicitly applied or discarded
- runtime assets are rebound after selected changes are applied

Next milestone: make the entire Apply Selected operation one Studio history transaction so one `Ctrl+Z` restores the complete pre-apply authored scene.

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

Foundation status: Seed now has an engine-owned `SeedDoctor` validator with stable finding codes, creator-facing and technical explanations, entity identity, severity, and explicit safe-fix actions. The first rules cover Door/Interactable dependencies, Pickup identity/quantity, Player Inventory/Character Body dependencies, and Third-Person camera setup.

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

## Character pipeline status

Seed's first real animated-character foundation now includes:

- animated glTF import with joints, weights, skeletons, inverse bind matrices and clips
- backend-neutral skinned-mesh renderer contract
- OpenGL GPU vertex skinning with a 96-joint palette
- real Linux OpenGL skinning smoke coverage
- `SkinnedMeshComponent` + `AnimationPlayerComponent`
- scene-level clip evaluation and skin-matrix generation
- RenderSystem skinned-character routing through the ordinary Seed PBR lighting/material path
- Studio Play Mode animation ticking
- static + skinned PBR shaders in Seed Studio
- scene persistence for authored character asset, visibility/shadow settings, clip, speed and play/pause
- runtime GPU handles, pointers, animation time and evaluated pose deliberately rebuilt rather than serialized

The current shadow pass intentionally uses the character bind-pose silhouette. Palette-aware animated shadows are a follow-up milestone.

## Priority

Near term:

1. one-transaction Undo for Apply Play Changes
2. expose Seed Doctor findings + safe fixes in Studio
3. Studio character import/authoring for Skinned Mesh + Animation Player
4. movement-driven animation states and blending for the Third-Person sample
5. built-in Save Game
6. semantic scene diff/merge UX

Later:

7. stable live-code/module reload
8. multiplayer authoring + prediction/rollback
9. collaborative editing/version-control UX
10. Time-Travel Gameplay Debugger

The product question for every feature is:

> Does this remove recurring game-development pain, or is it merely another checkbox?
