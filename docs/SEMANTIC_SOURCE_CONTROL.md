# Seed Semantic Source Control

Seed scenes are source-controlled by meaning, not by serialized line numbers.

## End-to-end workflow

1. `SceneDiffer` compares authored scene state using persistent entity IDs, component names, and properties.
2. `SceneMerger` performs a three-way merge from a common base plus ours and theirs.
3. Independent semantic changes are composed automatically.
4. Competing changes become structured `SceneMergeConflict` records.
5. A merge-resolution session chooses ours, theirs, or an explicit manual value for every conflict.
6. Seed Studio presents the same session visually as Base / Ours / Theirs.
7. The Seed CLI will expose the same engine APIs through `seed diff` and `seed merge`.
8. Git integration will use the CLI as a custom merge driver. Unresolved sessions can be handed to Seed Studio rather than exposing serialized scene conflicts.

## Design rule

Semantic comparison, merge, and resolution belong to SeedEngine. Studio and CLI are clients of those APIs. This keeps Play Changes Review, source control, prefab merging, and future semantic history on one model.

## v2 target

- resolution API with unresolved/resolved state
- ours/theirs/manual conflict choices
- deterministic conflict identifiers
- expanded structural and property conflict tests
- Studio merge preview and conflict resolver
- CLI semantic diff/merge
- portable merge-session representation for Git handoff

## Git target flow

`git merge` -> Seed merge driver -> automatic semantic merge -> if conflicts remain, merge session -> Seed Studio resolver -> resolved scene -> Git continues.

A developer should see `Player > Transform > position` and Base/Ours/Theirs values, never a meaningless JSON line conflict.
