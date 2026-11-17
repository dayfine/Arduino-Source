# Lesson: Lab App vs. Live App Controller Collisions and macOS Permission Isolation

**Date**: November 2026  
**Context**: Dual-console automation environment running live production (Console A) alongside continuous experimental story runs (Console B).  
**Affected PRs / Worktrees**: BDSP AutoStory (`bdsp/autostory-mine-floaroma-wip`, PR #226).  
**Classification**: Operational Safety & Process Lesson.

---

## 1. Summary of What Happened

During development and testing of BDSP AutoStory checkpoint legs on Console B, multiple collisions occurred where:
1. Console A's live session suddenly halted and dropped its microcontroller connection (`INFO_STREAM_NOT_READY`).
2. macOS System Settings (**Privacy & Security -> Camera**) ended up showing three identical entries all titled `SerialPrograms`.
3. Toggling camera permissions caused macOS to prompt "Quit & Reopen", which upon clicking reopened the app into Console A's profile, immediately knocking Console A offline again.
4. Video frames on Console B dropped to 0.00 FPS due to denied TCC permissions triggered by code-signature and bundle identifier mismatches.

This document records the exact technical root causes, the cascading failures between process management and macOS security, and the operational rules established to prevent recurrence.

---

## 2. Root Cause Analysis

### A. The Controller Takeover (Default Profile Fallthrough)

`SerialPrograms` determines hardware bindings (microcontroller serial port and video capture index) using its profile configuration:
* **Target Profile (`--profile lab-B`)**: Isolated to Console B's configuration directory (`~/Library/Application Support/SerialPrograms/Profiles/lab-B/`) and serial device (`cu.usbmodem...B`).
* **Default Profile (`--profile` omitted)**: Stored in `~/Library/Application Support/SerialPrograms/UserSettings/SerialPrograms-Settings.json`, which was pre-configured with **Console A's live production hardware** (`cu.usbmodem5C630032771`).

When an agent launched a test command directly:
```bash
./SerialPrograms.app/Contents/MacOS/SerialPrograms --test "bdsp"
```
or when macOS restarted the app from a system dialog, the executable was invoked **without** `--profile lab-B`.

Upon startup, `SerialPrograms` initialized its default settings and immediately sent:
```text
PABB2_CONNECTION_OPCODE_ASK_RESET
```
to Console A's serial port. This reset Console A's PABotBase hardware while Console A was actively playing. Console A's live controller stream immediately severed, aborting the active production run.

### B. The 3 Identical Permission Entries in System Settings

macOS TCC (Transparency, Consent, and Control) isolates camera capture permissions based on the application's unique bundle identifier (`CFBundleIdentifier`) and code signature. The lab environment had three bundles:

| Entry | Bundle Identifier | File System Location | Intended Console |
| :--- | :--- | :--- | :--- |
| **Production** | `PokemonAutomation.SerialPrograms` | `/Applications/SerialPrograms.app` | **Console A (Live)** |
| **Slot A Lab** | `com.dayfine.pokemonautomation.lab` | `~/BDSPLab/builds/slot-A/` | Console A (Test) |
| **Slot B Lab** | `com.dayfine.pokemonautomation.lab2` | `~/BDSPLab/builds/*-B/` | **Console B (Lab)** |

In [`make_slot_b.sh`](file:///Users/difan/BDSPLab/make_slot_b.sh), the packaging script attempted to set a friendly display name:
```bash
/usr/libexec/PlistBuddy -c 'Set :CFBundleDisplayName PokemonAutomation Lab 2' $A/Contents/Info.plist 2>/dev/null
```
**The Failure**: In Apple's `PlistBuddy`, the `Set` command fails silently if the specified key does not already exist in the plist. Because `CFBundleDisplayName` was not in the base template, the key was never written.

**The Result**: Without `CFBundleDisplayName`, macOS Privacy & Security fell back to the raw binary name (`SerialPrograms`) for all three bundles. The user saw **three identical entries named `SerialPrograms`** in System Settings with no indication of which toggle corresponded to Console A vs. Console B.

### C. The Permission Toggle and "Quit & Reopen" Trap

1. **Rebuilds and CDHash Changes**: While the code-signing certificate remained constant (`PokemonAutomation Local Development`), every C++ compilation generated a new binary with a new Code Directory Hash (`CDHash`).
2. **Permission Denied**: In attempting to identify the correct entry among the three identical toggles, switches were flipped. Once macOS flags a bundle identifier as Denied, Qt's camera permission handler (`SetupSettings.cpp`) evaluates to:
   ```text
   Qt::PermissionStatus::Denied -> "No camera permission found"
   ```
   Under macOS security policies, once an application is marked Denied, video capture frames are dropped silently (0.00 FPS), and the OS will not present another permission request prompt.
3. **The LaunchServices Trap**: When toggled back to ON in System Settings, macOS displayed a modal: *"Quit & Reopen"*.
   Clicking "Reopen" prompted LaunchServices to start the app bundle directly with **zero command-line arguments**. The app launched into the default profile, sending the reset command to Console A's port and hijacking the live controller once again.

---

## 3. The Rules (Lessons for Process & Architecture)

### Rule 1: Packaging Must Guarantee Unique Display Names
Packaging and deployment scripts (`make_slot_b.sh`, `bdsp_deploy.sh`) must never rely on `PlistBuddy Set` on unverified keys. They must explicitly delete (ignoring error) and `Add`:
```bash
/usr/libexec/PlistBuddy -c 'Delete :CFBundleDisplayName' "$PLIST" 2>/dev/null || true
/usr/libexec/PlistBuddy -c 'Add :CFBundleDisplayName string "PokemonAutomation Lab 2 (Console B)"' "$PLIST"
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$APP_BUNDLE"
```
This guarantees distinct names in macOS System Settings:
* Release App: `SerialPrograms`
* Slot A Lab: `PokemonAutomation Lab (Console A)`
* Slot B Lab: `PokemonAutomation Lab 2 (Console B)`

### Rule 2: Never Run Unwrapped Binaries or Click OS "Reopen"
* Never run `./SerialPrograms` or `./SerialPrograms.app/Contents/MacOS/SerialPrograms` directly without `--profile <name>`.
* If macOS presents a "Quit & Reopen" dialog after a permission toggle, click **Quit Only**. Never allow macOS to reopen the application, as it will launch without CLI flags and collide with Console A.
* Always launch lab instances through designated launch scripts (e.g. `open -n -a "$APP_BUNDLE" --args --profile lab-B`) or explicit test runners.

### Rule 3: Targeted TCC Reset
If TCC entries become stale or confused, do not guess and toggle entries in System Settings. Clear only the specific lab bundle ID from the terminal:
```bash
tccutil reset Camera com.dayfine.pokemonautomation.lab2
```
This leaves Console A's production permissions completely intact while resetting Console B to a clean slate.

### Rule 4: Default Profile Safety Isolation
The default profile (`UserSettings/SerialPrograms-Settings.json`) should not be treated as a catch-all. Production console controller ports should either require an explicit `--profile live-A` flag, or unparameterized headless/unit-test invocations must default to a dummy/mock controller rather than auto-connecting to the primary serial port.
