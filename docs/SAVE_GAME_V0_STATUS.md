# Seed Save Game v0 Status

**Implementation complete on `feature/save-game-v0`; awaiting final cross-platform CI before merge.**

Implemented:

- `PersistentComponent` authored marker
- `+ Add Gameplay -> Persistent / Save Game`
- creator-facing Persistent inspector with explicit v0 state coverage
- `.seedscene` persistence of the Persistent declaration
- stable persistent-ID capture and restore
- Transform runtime state
- Health current value, restored against the current authored maximum
- Inventory item IDs and quantities
- versioned `.seedsave` files
- slot and game-schema metadata
- safe partial restore with warnings for missing entities/components
- file read/write API
- end-to-end semantic round-trip tests
- Seed Doctor warning when an enabled Persistent entity has no Save Game v0-supported state

Deferred to later Save Game phases:

- runtime-spawned persistent entity identity
- checkpoints and autosave
- async I/O
- schema migration handlers
- creator-selectable property-level persistence policy
