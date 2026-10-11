# Seed Animation Architecture

Seed Animation is designed as an engine subsystem, not as a glTF feature and not as an OpenGL feature.

## Architectural boundary

```text
Animated file (.gltf / .glb)
        ↓
AssetImporter adapter
        ↓
Seed-owned character data
  ├─ Skinned vertices
  ├─ Skeleton
  └─ Animation clips
        ↓
AnimationSystem
        ↓
SkeletonPose / skin matrix palette
        ↓
Renderer skinning backend
```

TinyGLTF is permitted only inside the import adapter. Renderer APIs must never consume TinyGLTF nodes, skins, samplers, or accessors.

Likewise, `AnimationSystem` does not know about OpenGL, shaders, vertex buffers, entities, or Seed Studio.

## Seed-owned data

`AnimationData.h` currently defines:

- `Quaternion`
- `JointTransform`
- `SkeletonJoint`
- `Skeleton`
- `Vec3Keyframe`
- `QuaternionKeyframe`
- `JointAnimationTrack`
- `AnimationClip`
- `SkeletonPose`

A `SkeletonPose` stores:

1. local joint transforms;
2. solved global joint matrices;
3. final skin matrices (`global * inverse bind`).

## Pose evaluation

`AnimationSystem` supports:

- bind-pose initialization;
- linear Vec3 key interpolation;
- normalized shortest-path quaternion interpolation;
- clip looping;
- non-looping clamp;
- parent/child hierarchy solving;
- skin matrix palette generation;
- defensive cycle handling for malformed skeleton data.

## Animated glTF import v1

`AssetImporter::import_animated_model` returns `ImportedAnimatedModelData`.

Current supported character data:

- first skinned triangle primitive;
- `POSITION`;
- `NORMAL` (or Seed-generated normals);
- `TEXCOORD_0`;
- `COLOR_0`;
- `JOINTS_0` — four influences;
- `WEIGHTS_0` — four influences, normalized by Seed;
- skin joint hierarchy;
- FLOAT MAT4 inverse bind matrices;
- PBR material factors/textures;
- LINEAR translation animation channels;
- LINEAR quaternion rotation animation channels;
- LINEAR scale animation channels;
- external `.gltf` buffer/image dependency discovery.

Seed intentionally rejects unsupported interpolation rather than silently producing incorrect animation.

## Not renderer-ready yet

Animated import and animation evaluation do **not** yet make a character visible as an animated mesh.

The next renderer milestone needs:

1. a renderer-facing skinned vertex descriptor;
2. a skinned mesh resource/handle;
3. a maximum supported joint palette or palette buffer abstraction;
4. skinned vertex shader input;
5. joint palette upload per draw;
6. normal skinning;
7. shadow-pass skinning;
8. runtime animated-mesh component binding.

These should be renderer capabilities consuming Seed data. They must not move animation logic into the renderer.

## Locomotion after GPU skinning

Once a character can render a clip, the next beginner-facing layer is a locomotion preset:

```text
Idle
  ↕
Walk
  ↕
Run

Any Grounded State → Jump → Fall → Land
```

The creator should select clips by meaning (`Idle`, `Walk`, `Run`, `Jump`) rather than construct an animation graph for the common case.

Advanced animation graphs can come later as progressive complexity.
