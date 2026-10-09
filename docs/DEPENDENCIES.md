# Seed Engine Dependency Policy

Seed Engine is its own game engine. Using a focused third-party library for a low-level technical job does not make that library Seed's architecture.

The rule is simple:

> A dependency may implement a backend. It must not define Seed's public game model.

## Current dependencies

### GLFW 3.5.1

Purpose:

- native desktop window creation
- operating-system event polling
- keyboard input collection
- mouse input collection
- timing access

Boundary:

- Seed code outside the platform backend does not include GLFW headers.
- GLFW key and mouse codes are translated immediately to Seed-owned `KeyCode`, `MouseButton`, and `ButtonState` values.
- Seed scenes, entities, gameplay components, editor concepts and project formats contain no GLFW types.
- Rendering is owned by Seed's `IRenderer`; GLFW only supplies the native window/context integration used by the current OpenGL backend.
- Replacing GLFW should require a new `IPlatform` implementation, not changes to Seed gameplay or project content.

### nlohmann/json

Purpose:

- low-level JSON parsing and writing for Seed project/scene files

Boundary:

- Seed owns the `.seedproject` and `.seedscene` schemas, versioning and serialization semantics.
- No public Seed gameplay/editor concept depends on nlohmann/json types.
- Replacing the JSON implementation must not require creators to rebuild gameplay content.

### Dear ImGui

Purpose:

- immediate-mode drawing/input backend for the current Seed Studio editor UI

Boundary:

- ImGui is linked only into Seed Studio's UI layer.
- `SeedEngine` and `SeedRuntime` do not expose ImGui as part of their public gameplay model.
- Seed owns Studio layout, selection, Inspector behavior, authoring concepts and workflow.

### TinyGLTF 2.9.7

Purpose:

- parsing `.gltf` and `.glb` source files
- decoding glTF image data used during import

Boundary:

- TinyGLTF types are confined to `AssetImporter.cpp` and never appear in Seed's public asset API.
- `AssetImporter` converts source files into Seed-owned `ImportedModelData`, vertices, indices and texture pixels.
- Seed owns persistent `model:` asset IDs, the project `Assets/` layout, GPU resource caching, `MeshComponent`, `MaterialComponent`, renderer handles and Studio import UX.
- Replacing TinyGLTF must not invalidate `.seedproject` / `.seedscene` content or change beginner-facing terminology.

## Allowed dependency categories

Focused libraries may be considered for:

- platform/window integration
- graphics API loading
- physics solving
- audio codecs / device access
- model and image decoding
- font rasterization
- compression
- serialization helpers

## Seed-owned architecture

The following are not delegated to another game engine:

- engine lifecycle
- application/frame loop
- scene model
- entity/component model
- gameplay framework
- input abstraction
- renderer abstraction
- asset database and persistent asset IDs
- project and scene formats
- Seed Studio
- Seed Logic
- game runtime

## Review rule

Before adding a dependency, answer:

1. Is this a focused library rather than another game engine?
2. Can Seed wrap it behind an interface we own?
3. Can we replace it later without invalidating `.seedproject` or `.seedscene` files?
4. Does beginner-facing Seed terminology remain independent from the dependency?

If the answer to any of these is no, the dependency needs architectural review before adoption.
