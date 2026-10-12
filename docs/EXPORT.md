# Seed export workflow

Seed exports a runnable package containing `SeedRuntime`, `SeedStudio`, and the engine documentation. A project can be copied into the package as optional content for distribution.

## Configure and build

From the repository root:

```sh
cmake -S . -B build -DSEED_BUILD_TESTS=OFF
cmake --build build --config Release
```

## Export a project

```sh
cmake \\
  -DSEED_BUILD_DIR=build \\
  -DSEED_EXPORT_DIR=dist/MyGame \\
  -DSEED_PROJECT_DIR=/path/to/MyGame \\
  -DSEED_CONFIG=Release \\
  -P tools/export_project.cmake
```

The exported directory contains:

```text
dist/MyGame/
├─ bin/
│  ├─ SeedRuntime
│  └─ SeedStudio
├─ project/
│  └─ MyGame.seedproject
└─ share/seed/docs/
```

On a single-platform build, CPack can also create an archive:

```sh
cd build
cpack -C Release
```

The exported runtime is intentionally asset-agnostic: imported models, textures, audio, and scenes remain project-owned files copied by `SEED_PROJECT_DIR`. The next packaging step is platform-specific dependency bundling and a project-aware runtime launcher that opens the project's startup scene.
