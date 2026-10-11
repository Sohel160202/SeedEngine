# Seed Third-Person Player

Seed's Third-Person Player is a beginner-facing gameplay preset implemented entirely with Seed-owned runtime, scene, hierarchy, physics, rendering, and authoring systems.

## Creator workflow

The current authoring path is intentionally simple:

1. Select the entity that should become the player (for example an imported character).
2. Open **+ Add Gameplay**.
3. Choose **Player — Third Person**.
4. Seed automatically ensures:
   - `TransformComponent`
   - `PlayerControllerComponent` in Third Person mode
   - `InventoryComponent`
   - `CharacterBodyComponent`
   - a child game Camera entity
5. Press **Play**.

The Third-Person Player inspector exposes game concepts rather than engine plumbing:

- Move Speed
- Sprint Multiplier
- Look Sensitivity
- Face Movement Direction
- Interaction Distance
- Aim Radius
- Camera Distance
- Camera Height
- Shoulder Offset
- Minimum / Maximum Pitch

## Runtime behavior

Third Person is a real view mode in `PlayerControllerComponent`, not an editor-only camera trick.

At runtime Seed:

- resolves the Player's child orbit camera;
- keeps camera yaw/pitch separate from character facing;
- moves the character relative to camera yaw;
- optionally turns the character toward movement direction;
- uses Character Body gravity, jumping, grounding, and collision;
- aims interactions from the gameplay camera while enforcing reach from the character body;
- collapses the orbit camera toward the character when solid world collision blocks the desired camera position.

## Camera collision

Camera collision uses `PhysicsSystem::clip_camera_position`.

The query is Seed-owned and independent of Third Person. It casts a small-volume segment from the camera anchor to the desired camera position against enabled solid `BoxColliderComponent` objects and returns a safe clipped position before the obstruction.

This can later be reused by:

- shoulder cameras;
- lock-on cameras;
- cutscene cameras;
- vehicle cameras;
- editor camera safety.

## Persistence

Third-person settings are serialized inside `.seedscene` as part of `PlayerController` data.

Existing scenes remain compatible because scenes without a `view_mode` field default to First Person.

## Current limitations

This milestone deliberately focuses on controller/camera architecture. It does not yet provide:

- skeletal character animation;
- animation state machines / blend spaces;
- camera smoothing or spring-arm lag;
- lock-on targeting;
- crouch;
- slopes / stairs / advanced capsule sweeps;
- a dedicated **Create → Player → Third Person** menu item (Third Person is currently added through **+ Add Gameplay**).

## Next recommended milestone

**Character Presentation + Animation Foundation**

The next major step should make a Third-Person Player look like a game character rather than merely move like one:

1. skeletal mesh asset representation;
2. skeleton + skin import from glTF;
3. animation clip import;
4. runtime pose evaluation;
5. simple locomotion state (`Idle / Walk / Run / Jump / Fall`);
6. beginner-facing animation preset in Seed Studio.

That takes Seed from a functional third-person controller to the foundation of a presentable third-person game.
