#pragma once

namespace seed {

// Marks an authored entity as gameplay state that belongs in Seed Save Game.
// The marker intentionally does not expose per-field serialization plumbing to
// beginners. SaveGameSystem owns the supported runtime-state semantics for Seed
// components and can grow property-level policy later without changing game code.
struct PersistentComponent {
    bool enabled{true};
};

} // namespace seed
