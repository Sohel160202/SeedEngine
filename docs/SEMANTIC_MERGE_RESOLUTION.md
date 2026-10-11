# Semantic Merge Resolution

Seed's semantic merge is split into two deterministic stages:

1. SceneMerger::merge combines independent changes and reports conflicts.
2. SceneMergeResolver::resolve applies explicit Ours/Theirs choices to those conflicts.

## API

Include seed/scene/SceneMergeResolution.h and create a plan whose entries correspond to the conflict index:

    seed::SceneMergeResolutionPlan plan;
    plan.choices = {
        seed::SceneMergeResolutionChoice::Ours,
        seed::SceneMergeResolutionChoice::Theirs,
    };

    const auto result = seed::SceneMergeResolver::resolve(
        base_scene,
        ours_scene,
        theirs_scene,
        plan
    );

A missing plan entry remains unresolved. This makes partial UI workflows safe: closing a review panel cannot silently choose a side.

result.complete() is true only when every merge conflict has been resolved and no change-application error occurred.

## Conflict semantics

- Property, name, and component conflicts apply exactly one selected branch change.
- Entity-added-differently conflicts apply the selected branch addition.
- Entity-removed-vs-modified conflicts apply the selected branch's complete entity delta, so an entity is either removed or kept consistently.
- Independent changes remain auto-merged by SceneMerger.

The resolver is UI-agnostic. Seed Studio can use it for a conflict panel, while a future CLI or Git merge driver can use the same behavior.
