# Seed Adventure sample

This sample is intentionally asset-independent. It uses Seed's built-in cube mesh and material so the gameplay workflow can be opened without downloading character or environment packs.

The loop is:

1. Start as a third-person Player.
2. Talk to the Village Guide.
3. Find and pick up the Cabin Key.
4. Return to the Cabin Door and open it.
5. Inspect the Training Target as the combat/health example.
6. Save the scene's Player and quest progression through the Save Game service.

Open `SeedAdventure.seedproj` in Seed Studio, or launch the standalone runtime with:

```sh
SeedRuntime --project=samples/SeedAdventure/SeedAdventure.seedproj
```

The sample is a gameplay systems reference. Replace its built-in cubes and empty animation references with Mixamo, Kenney, or your own assets later; no engine code needs to change.
