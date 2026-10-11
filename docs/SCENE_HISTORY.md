# Scene History

Seed stores authored-scene revisions inside the project at:

    .seed/revisions/

Each revision contains:

- an immutable .seedscene snapshot;
- a small JSON metadata file with its ID, label, and entity count.

Runtime SaveGame slots remain separate. Scene revisions are editor history, not player save data.

## Studio workflow

Use Tools → Save Scene Revision to create a named snapshot.

Use Tools → Scene History to:

- review available revisions;
- restore a previous authored scene;
- keep the restore as one undoable Studio operation.

Restoring a revision does not overwrite the current scene file automatically. Save the project normally after reviewing the restored scene.

## Engine API

The SceneRevisionStore service provides:

- create_revision(scene, label);
- list();
- load_revision(scene, revision_id).

Revision IDs are generated as revision-000001, revision-000002, and so on. Labels are sanitized before metadata is written, and revision IDs are validated before loading so a project history cannot escape its own revision directory.
