# Start a development instance

Once a slot is configured, run this from any directory:

```sh
python3 ~/Projects/Arduino-Source-agents/lab/scripts/start_dev.py A
```

Use `B` for the other console. Keep the terminal open. Quit the GUI normally to
release its console lock; Ctrl-C requests termination and waits for the child
before releasing ownership. This opens the application, not an automation program.
Existing saved panels may reconnect their assigned devices under #237's guards.

To see the resolved configuration without fetching, locking, building or launching:

```sh
python3 ~/Projects/Arduino-Source-agents/lab/scripts/start_dev.py A --plan
```

The launcher lives on the `agents` tools branch; it orchestrates a separate code
checkout. Do not merge `agents` into a code branch. Before this tools PR merges,
use the script from its review worktree instead of the path above.

## Configure once

1. Use a **dedicated** code worktree and local or remote `lab/...` integration branch for
   each slot. Do not point at an agent's active checkout. The integration branch
   merges `lab/main`, the device-ownership guard (#237), and the feature under
   test. Upstream-bound feature branches never merge lab tooling.

   For example, to prepare a new Max Lair development branch (review/resolve any
   merge conflict normally before continuing):

   ```sh
   git -C ~/Projects/Arduino-Source fetch origin
   git -C ~/Projects/Arduino-Source worktree add -b lab/dev-maxlair ~/Projects/wt/dev-maxlair origin/lab/main
   git -C ~/Projects/wt/dev-maxlair merge origin/lab/device-ownership
   git -C ~/Projects/wt/dev-maxlair merge origin/swsh/maxlair-boss-tests
   ```

   The launcher switches to the configured branch and fetches origin. If a matching
   remote lab branch exists, it fast-forwards; a local integration branch needs
   no publication. It preserves local integration commits.
   It never stashes, resets, rebases, resolves conflicts, merges feature changes,
   commits or pushes. It refuses dirty worktrees, branches checked out elsewhere,
   divergent updates, missing lab-main ancestry, and missing device guards.
   Refresh integration merges yourself when parents change. The guard check is
   pinned to #237's known runtime-adapter revision `45afc7eb048b638f34a62e79c8bbf7d65a377e1e`;
   a future history that removes this ancestry needs a reviewed pin update.

2. Prepare `lab-A` / `lab-B` profiles using [#236's preference-only seeding](https://github.com/dayfine/Arduino-Source/blob/lab/profile-preparation/devtools/lab/INSTANCE_PREPARATION.md).
   Review their `lab-devices.json` using [#237's device ownership instructions](https://github.com/dayfine/Arduino-Source/blob/lab/device-ownership/devtools/lab/DEVICE_OWNERSHIP.md).
   Empty allowlists permit GUI setup without device opens. The launcher does not
   copy default settings, discover/approve device identities, or configure panels.
   Confirm physical A/B assignments before enabling devices. Older unguarded
   applications must remain disconnected from those devices.

3. Keep the existing persistent signing certificate and normal Qt/OpenCV/CMake
   setup. See [local signing](../pokemon_sv/lab/MACOS_LOCAL_SIGNING.md).
   No certificate, permissions or TCC changes are made by this launcher. A fresh
   build needs the normal development dependencies (including Qt SerialPort),
   SDK/OpenCV paths and Packages resources. Configure any machine-specific cache
   values in `cmake_args`. Existing build directories must already match both the
   selected source and profile; an unbound/default build is refused rather than
   repurposed. Use separate build directories outside each source checkout.

4. Save `~/.config/pokemonautomation/dev-labs.json`. Paths below are examples;
   choose your dedicated directories. A one-slot file is valid: omit B until it
   has been prepared.

   ```json
   {
     "schema": 1,
     "slots": {
       "A": {
         "worktree": "~/Projects/wt/dev-maxlair",
         "branch": "lab/dev-maxlair",
         "build_dir": "~/Projects/dev-builds/slot-A",
         "stage_root": "~/Projects/dev-deployments/slot-A",
         "bundle_id": "com.dayfine.pokemonautomation.labA",
         "signing_identity": "PokemonAutomation Local Development",
         "cmake_args": ["-DCMAKE_BUILD_TYPE=RelWithDebInfo"]
       },
       "B": {
         "worktree": "~/Projects/wt/dev-bdsp",
         "branch": "lab/dev-bdsp",
         "build_dir": "~/Projects/dev-builds/slot-B",
         "stage_root": "~/Projects/dev-deployments/slot-B",
         "bundle_id": "com.dayfine.pokemonautomation.labB",
         "signing_identity": "PokemonAutomation Local Development",
         "cmake_args": ["-DCMAKE_BUILD_TYPE=RelWithDebInfo"]
       }
     }
   }
   ```

   The console selects the profile (`A` → `lab-A`, `B` → `lab-B`). Profiles,
   bundle IDs and directories cannot be shared between configured slots.
   `cmake_args` accepts cache `-DKEY=VALUE` entries, not arbitrary shell commands
   or alternate CMake source/preset/script/profile overrides.

Optional shell convenience after setup:

```sh
alias dev-a='python3 ~/Projects/Arduino-Source-agents/lab/scripts/start_dev.py A'
alias dev-b='python3 ~/Projects/Arduino-Source-agents/lab/scripts/start_dev.py B'
```

## What the command does

1. Acquires the existing console lock protocol at `~/.lab/locks/A` (or B), then
   the launcher-wide build lock. A busy console/build fails immediately.
2. Checks the worktree is clean; fetches origin, switches the named lab branch,
   and fast-forwards it when a matching remote exists; checks its lab/device-guard ancestry and profile policy.
3. Configures the profile-bound build and builds with **four workers**. Configure
   and build output remains in the terminal. It refuses source changes during
   building and a linked receipt from a different source revision.
4. Calls the existing `lab_instance.py stage`/`verify` tools to create and sign
   a new deployment. Copies the build's `Resources`, `Firmware` and `Scripts`
   beside the app, matching macOS `Main.cpp`'s working-directory behavior. Missing
   runtime directories prevent launch. Existing deployments are never overwritten.
5. Releases the build lock, then starts the actual executable as a supervised
   foreground child with `--profile lab-A`/`lab-B`. The console lock remains
   held until the GUI process exits. The app's native profile/device leases are
   a separate layer and enforce ownership before actual device opens.

The console lock interoperates with `lab/control/lablock.sh`, `lc.sh` and CFR
helpers. It is not a replacement for the native device leases. Other local build
commands do not participate in the build mutex: do not run them concurrently.
Another console can start its build after this launcher finishes staging, while
this GUI remains open. The launcher does not select a program, press Start,
stop another instance, reset a console or modify saves.

Each run stages a new immutable directory under `stage_root`, with the source
revision in its name and the deployment manifest alongside it. Bundle identity
and certificate remain stable, but paths change; this version does not promise
macOS permission prompts will never recur. Old deployments are retained for
inspection; remove unused ones only after confirming their app has exited.

If a source-only change did not relink and its receipt is stale, relink through
the normal CMake build (for example, a deliberate clean rebuild) before retrying.
Do not edit a receipt to make it pass. If a console lock remains after an abrupt
launcher crash, inspect it with the existing `lablock.sh -c A status`; the launcher
never deletes somebody else's lock or automatically evicts an existing owner.

## Offline verification

```sh
python3 -m unittest discover -s lab/scripts -p test_start_dev.py -v
```

Tests cover config/slot isolation, real temporary Git checkout and fast-forward,
clean-source checks, console/build exclusion, compatibility with the existing
shell lock, failure ordering, runtime data staging and interruption with delayed
child teardown. They use temporary repositories/processes and injected build/
signing/GUI operations. They do not build the app, launch a live profile, sign
with the real certificate or validate physical capture/controller behavior.
