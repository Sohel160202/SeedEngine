# Revision-Based Semantic Merge

Seed Studio's Semantic Merge Review now operates on named Scene History revisions.

## Workflow

1. Save at least three authored revisions from Tools → Scene History.
2. Open Tools → Semantic Merge Review.
3. Select Base Revision, Ours Revision, and Theirs Revision.
4. Review semantic conflicts and choose Ours or Theirs for each.
5. Apply the merge.

Seed automatically saves the resolved result as a new revision named Merged revision before applying it to the active scene. The result remains available in Scene History and can be undone through Studio history.

The merge inputs are immutable revision snapshots. The current working scene is not used as an implicit merge side, which keeps the operation reproducible.
