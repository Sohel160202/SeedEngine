# Beginner workflow

Seed's beginner workflow is intentionally built around concepts instead of engine plumbing.

## Make a first game

1. Open Seed Studio.
2. Choose File > New Project.
3. Start with `Third-Person Adventure` or `Collect and Open`.
4. Select an object and use `+ Add Gameplay`.
5. Add a Player, Pickup, Door, Health, Quest, or Gameplay Rules component.
6. In Gameplay Rules, author behavior as `When` and `Then`.
7. Press F5 to play inside Seed Studio.
8. Use WASD to move, RMB to look, E to interact, and F to attack.
9. Save and export when the loop feels right.

## Example rules

- When `Interact` → Publish Event `Talked`
- When `Talked` → Start Quest `find_key`
- When `PickupCollected` → Give Item `Coin`
- When `Hit` → Apply Damage
- When `Death` → Publish Event `AreaCleared`

Rules are stored in the scene and executed by Seed's semantic gameplay event system. A creator can start with these presets and progressively move toward lower-level C++ only when needed.

## What assets do

The starter templates use built-in cubes so the workflow works without external downloads. Later, a creator can import a static or animated glTF asset and assign it to the world or Player. Mixamo, Kenney, and custom assets are content choices—not engine requirements.

## Standalone runtime

Launch a saved project with:

```sh
SeedRuntime --project=MyGame/MyGame.seedproj
```

The runtime HUD displays health, objectives, dialogue, interaction prompts, and inventory. F5 saves an autosave slot and F9 loads it.
