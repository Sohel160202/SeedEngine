# Seed Save Game v0

Seed Save Game is a native engine system, not a project tutorial.

## Creator model

Creators mark authored entities as **Persistent**. Seed uses the entity's stable persistent ID and serializes supported gameplay state automatically.

Version 0 intentionally starts small and deterministic:

- Transform state
- Health state
- Inventory contents
- explicit slot metadata
- versioned `.seedsave` files
- safe restore by persistent entity ID
- unknown/missing entities are reported rather than guessed

The long-term model expands to component/property persistence declarations, checkpoints, autosave, async I/O, runtime-spawned entity identity, and schema migrations.

## Principles

1. Saving should not require custom serialization code for ordinary Seed gameplay.
2. Authored defaults remain in `.seedscene`; runtime progress belongs in `.seedsave`.
3. Save data is semantic and versioned, never a raw memory dump.
4. Stable persistent IDs are the bridge between authored entities and saved runtime state.
5. Restore reports partial/missing state instead of silently corrupting the world.

## File direction

```json
{
  "seed_format": "seedsave",
  "format_version": 1,
  "slot": "autosave",
  "entities": {
    "player-id": {
      "transform": {},
      "health": {},
      "inventory": {}
    }
  }
}
```

This document defines the initial contract while implementation lands incrementally.
