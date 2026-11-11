# Update a development slot after a feature edit or amend

After committing in your feature worktree, update the configured dev integration
branch without changing directories:

```sh
python3 ~/Projects/wt/dev-feature-sync/lab/scripts/sync_dev.py B bdsp/my-fix
```

After an amend/rewrite, or when that merge conflicts:

```sh
python3 ~/Projects/wt/dev-feature-sync/lab/scripts/sync_dev.py B bdsp/my-fix --force
```

Once these tools merge into `agents`, the same script is available in an updated
agents checkout under `lab/scripts/sync_dev.py`. The path above works from the
review checkout immediately. `--config /path/to/dev-labs.json` selects another
reviewed slot configuration. A selects the Max Lair slot; B selects BDSP.

Quit the selected dev GUI/launcher before syncing. This command uses the same
console and build locks as `start_dev.py`, so it refuses to change sources while
the slot is active. It does not stop another process, build or launch anything,
open a device, push a branch, or change your feature checkout.

Then build/launch with the usual command:

```sh
python3 ~/Projects/wt/dev-instance-launcher/lab/scripts/start_dev.py B
```

## What force means

`git merge` has no amend-aware `--force` option. `-X theirs` only chooses incoming
conflict hunks; it can retain stale changes that the amended commit removed.
Blindly restoring all incoming files can also erase lab startup/device guards.

This helper instead:

1. Identifies the previously merged feature tip, using its recorded trailer or
   Git's ordinary two-parent merge message for the exact branch name.
2. Compares **previous feature tree → current feature tree**, including file
   additions, removals and mode changes. A changed integration file must still
   equal the old feature version, or already equal the new version. Otherwise
   the helper refuses to overwrite that integration customization.
3. If a matching merge is pending, backs up the original index, changed file
   bytes, merge heads, unmerged entries and diffs under
   `.git/dev-feature-sync-backups/<timestamp>/`, then aborts that merge. It
   reports the exact backup directory. A pending merge for another branch is
   left untouched. A feature amended again while its own merge is pending is
   identified by the merge message and its old incoming head is recorded too.
4. Replaces only the validated feature delta and creates a normal two-parent
   merge commit. The old integration remains the first parent; the new feature
   is the second. This records ancestry without retaining stale feature code.
   Unrelated lab files and history stay intact; there is no reset, rebase or
   force-push. Normal commit hooks still run.

If automatic prior-tip detection is unavailable, provide the exact old merged
feature commit explicitly:

```sh
python3 /path/to/sync_dev.py B bdsp/my-fix --force --previous <old-feature-SHA>
```

The old commit must be an ancestor of the integration branch. This is not an
escape hatch for overwriting lab-only edits. An ordinary dirty worktree is
refused, even with --force. A failed commit hook leaves the staged merge for
review; the helper does not bypass it. Backups preserve in-progress conflict
work but are not automatically restored over later changes.

The helper uses local refs and does not fetch. Update the feature's intended
parents separately. Native guard/lab-main ancestry is checked before a slot is
updated; the feature must contain code, not the agents/notes branch.

## Tests

```sh
python3 -m unittest discover -s lab/scripts -p 'test_*dev.py' -v
```

Tests use temporary Git repositories/worktrees, never a console. They cover
normal updates, repeated amends, deletions/additions, removing stale code,
linked-worktree conflict backups, incoming changes during a pending merge,
lab-file/mode preservation, dirty state, unrelated merges and commit-hook
failure. These are source-update tests, not proof that the new gameplay or
native build passes; run the relevant unit tests after syncing.
