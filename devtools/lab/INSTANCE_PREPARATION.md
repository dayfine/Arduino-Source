# Preparing two lab instances

Use a dedicated profile and app identity for each console. Default remains the
normal-use profile. This tooling prepares files offline; it never launches an
app, sends a command, changes permissions, or opens a controller or capture device.

## Seed profiles without copying live state

Work from an offline snapshot containing `UserSettings/SerialPrograms-Settings.json`, taken while the
source app is closed. Keep that snapshot as the backup; do not seed from a file
being rewritten by a running app. For example, using a scratch root first:

```sh
python3 devtools/lab/lab_instance.py seed \
  --source /offline/default-snapshot --root /offline/application-support --profile lab-A
python3 devtools/lab/lab_instance.py seed \
  --source /offline/default-snapshot --root /offline/application-support --profile lab-B
```

Each destination is `ROOT/Profiles/lab-NAME`. Existing destinations, including
empty directories, are refused. The source is never written. The new directory
is private to its owner. Only theme, window geometry and console-settings collapse
preferences are retained; panels are empty. Saved controller/camera/audio bindings,
device aliases, program options, selected panels, developer credentials, Discord
settings, test settings, statistics/temp paths, logs and channel queues are omitted.
Output-window startup and Rich Presence are disabled. Unknown future settings are
omitted. This is preference seeding, not a full profile clone. Application defaults
still apply to omitted settings; this does not promise a network-free GUI launch.

After reviewing the scratch output, the same command can seed under
`~/Library/Application Support/SerialPrograms`. Do not rename the executable:
its basename participates in the application's settings path. Do not configure
real devices until the assigned console and device identities have been checked.
`lab-profile.json` deliberately records `hardware_configured: false`; it is a
preparation note, not a runtime device lease.

## Bind and stage each build

The lab integration branch merges `lab/main` and the feature under test (MaxLair,
for example). The upstream feature branch never merges lab tooling. Build each
slot with its matching profile, using the existing build/signing setup:

```sh
cmake -S SerialPrograms -B /build/slot-A -DPA_LAB_PROFILE=lab-A
# Build via the existing lab procedure; use a clean committed source tree.
```

macOS linking writes `SerialPrograms.lab-build.json` beside the executable. It
records the compiled profile, executable SHA-256, source revision and whether the
source checkout was clean at link time. Unbound linking removes an old receipt.
This is accidental-mixup detection, not a signed source/build attestation; it
records the checkout observed at link time, and cannot prove when every object
was compiled. Use a clean committed checkout and the normal dependency-tracked
build. Do not edit or regenerate receipts manually to accept a changed binary.

```sh
python3 devtools/lab/lab_instance.py preflight \
  --app /build/slot-A/SerialPrograms.app --profile lab-A
python3 devtools/lab/lab_instance.py stage \
  --app /build/slot-A/SerialPrograms.app --out /staging/slot-A \
  --profile lab-A --console A --bundle-id com.dayfine.pokemonautomation.labA \
  --signing-identity 'PokemonAutomation Local Development'
```

Stage requires a matching linked-build receipt and unused output directory. It
copies the app, sets a distinct lab identifier/display name, signs and verifies
it, then writes `lab-deployment.json` outside the bundle with final executable
and plist hashes. The source app is unchanged. Failure removes only the new
staging directory. No active deployment is replaced and no permissions are reset.
Keep the manifest next to the staged app. The build receipt inside the signed app
describes the pre-packaging artifact; use `verify` for the final signed package.

Prepare B with `lab-B`, console `B`, a different bundle ID and
`--peer /staging/slot-A/lab-deployment.json`. Peer manifests reject reuse of a
profile, console or bundle ID. Before deployment, cross-check both packages:

```sh
python3 devtools/lab/lab_instance.py verify \
  --manifest /staging/slot-A/lab-deployment.json --profile lab-A \
  --peer /staging/slot-B/lab-deployment.json
python3 devtools/lab/lab_instance.py verify \
  --manifest /staging/slot-B/lab-deployment.json --profile lab-B \
  --peer /staging/slot-A/lab-deployment.json
```

Use this staging entry point instead of editing/signing bundles with the older
local deploy scripts. Those external scripts have not been modified automatically
and can bypass this check. No production/default profile is accepted by this
tool: preparation requires a `lab-` prefix. The existing startup policy additionally
rejects a profile conflicting with the compiled binding, before loading settings.

## Remaining device boundary

Two profiles do not reserve two physical devices. A misconfigured profile can
still open another console's serial port, which can disturb that console before
commands are sent. Shell locks and channel ownership do not prevent that.
Before concurrent hardware validation, add an explicit per-slot device allowlist
and exclusive lease checked before serial/capture open, with native tests for
wrong-device access and competing owners. These packages explicitly report
`hardware_ownership_enforced: false`. They are not authorization to start a run.

## Offline verification

```sh
python3 -m unittest discover -s devtools/lab -p test_lab_instance.py -v
```

Tests use temporary settings, fake executable files and an injected signer. They
exercise the real receipt CMake script without configuring the full application.
They cover sanitization, existing destinations, default/path names, symlink escape,
missing/mismatched/stale/dirty receipts, slot collisions, post-signing hashes,
modified packages and signing failure. They do not validate real certificates,
macOS permission persistence or physical-device exclusion. CI runs on macOS/Linux.
