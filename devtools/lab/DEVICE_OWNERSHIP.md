# Lab device ownership

This is the second preparation step after #236. macOS GUI lab builds reserve
the selected profile before Qt/settings startup and check an explicit allowlist
before constructing a native serial connection, QCamera or audio input.
Without a matching policy, hardware opens are refused. An empty policy permits
the GUI for setup, but permits no devices. Native tests do not reserve live
profiles or enumerate hardware.

## Configure the allowlist

Create `lab-devices.json` at the profile root, alongside `UserSettings/`, using
reviewed identities for that console. Example with deliberately fictitious IDs:

```json
{
  "schema": 1,
  "profile": "lab-B",
  "serial": ["10c4:ea60:EXAMPLE-USB-SERIAL-B"],
  "video": ["4558414d504c452d564944454f2d42"],
  "audio": ["4558414d504c452d415544494f2d42"]
}
```

All five fields are required; unknown fields, duplicates within a device list,
invalid JSON, wrong types, empty identities, unsupported schema and mismatched
profile names are refused. Empty lists are valid. A successfully loaded policy
is immutable for that process: restart the app after changing it. No default
profile, display alias, first-device fallback or wildcard is used.

Serial keys are lowercase four-digit USB vendor/product IDs followed by the exact
USB serial number: `VID:PID:SERIAL`. The runtime resolves a fresh USB enumeration
immediately before opening, rather than trusting the UI's cached port record.
`/dev/tty.*` and `/dev/cu.*` aliases share a key. Two distinct current ports
reporting the same USB identity are rejected, as are adapters lacking a serial.
If a port path is reused by a different adapter, its new identity must independently
be allowlisted. VID/PID/serial cannot distinguish hardware that impersonates an
identity; this protects against accidental assignment errors, not hostile USB devices.

Video/audio entries are the lowercase hexadecimal encoding of the exact byte IDs
reported by `QCameraDevice::id()` / `QAudioDevice::id()`. The settings UI stores
the corresponding original device ID; it does not use the hex string as a device
selector. Do not use capture descriptions, friendly names or list positions.
Audio output to speakers is outside this input-device policy.

This PR does not generate a policy from the currently selected panel: that could
approve the very wrong-console binding we are preventing. Identity collection
and confirming A/B assignments remain a deliberate setup step. Existing profiles,
local deployment scripts, certificates and permissions are not modified.

## Ownership and lifetime

All guarded apps for a user use the same local directory:

```
~/Library/Application Support/PokemonAutomation/LabDeviceLeases/
```

Lock keys hash device kind plus stable identity, independently of profile,
executable name and bundle ID. Different consoles can run concurrently;
two profiles pointing at the same input contend for one lease. A separate
profile lease prevents duplicate GUI instances from loading/writing the same
profile. Video and audio are separate input endpoints so a capture card's
camera and microphone can be used together.

The implementation uses Qt's existing `QLockFile` backend, with nonblocking
acquisition and age-based expiration disabled. A live owner is never evicted
because a run has lasted a long time. Qt detects a dead process and recovers its
stale lock; the application never forcibly deletes another owner's lock.
Do not put the lease directory on a network filesystem or remove locks manually.
See [Qt's locking contract](https://doc.qt.io/qt-6/qlockfile.html) for platform
and cooperative-locking limits.

The serial lease survives connection setup, command traffic and teardown until
the physical port closes. Failed setup closes resources before releasing it.
The camera lease survives Qt's synchronous destruction of the camera on its event
thread. The audio lease survives stopping and destroying its native input.
Reconfiguration closes the previous input before opening its replacement.
Capture refusals are logged and return no active input without propagating an
exception through a Qt callback. Serial refusals appear as a failed connection.

## Scope

Enforcement is for macOS GUI lab builds in this stack. GUI-free CoreLib and
Windows/Linux application device behavior remain outside this enforcement;
the lease engine itself has portable native tests. The shared audio replacement
ordering also applies on other GUI platforms.

Every app touching the test devices must use the guarded build. An old/default
binary that does not participate in this locking protocol cannot be excluded by
an advisory lease. Keep it disconnected from lab devices. This is not a system
sandbox, and neither file hashes nor the allowlist replace physical-console
assignment review. #236's package metadata still says
`hardware_ownership_enforced: false`; that older preparation marker is not a
runtime capability check or evidence that a deployed binary contains this PR.
Use the deployed source revision and this stack's CI evidence.

## Native tests

The focused target compiles production policy/lease code, without the GUI,
controller drivers, real settings or capture hardware:

```sh
cmake -S SerialPrograms/Source/CommonFramework/Devices -B /path/to/test-build
cmake --build /path/to/test-build -j4
ctest --test-dir /path/to/test-build --output-on-failure
```

The same cases are registered in the application's native test database. The
small runner is also the subprocess used by cross-process cases and is built
beside the app executable. Tests cover policy rejection before scripted opens,
USB identity/alias resolution, duplicate USB identities, same/different devices,
duplicate profiles, release/reconnect, exception unwinding, live-process exclusion
and recovery after killing an owned test child. The aged-live-lock timestamp
fixture runs on POSIX; Windows disallows editing that live lock file.

These tests validate real process-lock behavior and scripted opens. They do not
validate drivers' physical close completion, USB replug behavior, macOS capture
identity persistence or real permission dialogs. Live validation still needs a
separately authorized, carefully assigned two-console run.
