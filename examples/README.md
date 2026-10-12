# Seed First Adventure

`FirstAdventure.seedscene` is Seed's asset-free gameplay sample.

It intentionally uses only built-in cubes and Seed gameplay components so the engine can be dogfooded without imported character, animation, or environment assets.

## Gameplay loop

1. Start as the third-person Adventure Player.
2. Talk to Old Guide.
3. Receive the **Restore the Gate** quest.
4. Pick up the Power Cell.
5. Open the sealed gate.

## Systems demonstrated

- Third-person Player and camera
- Primitive rendering and collision
- Interactable objects
- Branching dialogue choices and quest activation
- Inventory and pickups
- Item-locked sliding doors
- Quest item rewards
- Scene persistence

This is an engine example, not the final Seed game. It is intentionally small so future systems can be added without needing an asset migration first.
