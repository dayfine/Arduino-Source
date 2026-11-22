# Startup isolation: review and upstream path

A launch intended only to run tests must not restore a saved controller panel.
A lab relaunch must not silently select the live app's default settings.

The PR #226 lesson at revision `01e6659116f520b51c35ee956aa6a222dfa864f8`
records a `--test bdsp` invocation and profile-free relaunch concerns. That command
is not supported: previously, unrecognized arguments could fall through to GUI
startup. This is a startup problem, independent of AutoStory game logic.

## Review separately

1. [PR #234](https://github.com/dayfine/Arduino-Source/pull/234), based on fork
   `main`, only extracts service shutdown and worker shutdown from `main()`.
   The statements and their order are unchanged, including the Windows Qt
   quick-exit branch. It has no lab or gameplay parents and is independently
   suitable for an upstream handoff after validation.
2. [PR #233](https://github.com/dayfine/Arduino-Source/pull/233), targeting
   `lab/main`, contains the startup behavior change and merges #234's exact
   commit without rewriting the previously published history. Its base-relative
   diff will still include the mechanical helper extraction until #234 reaches
   `lab/main`; review that extraction in #234 rather than as new behavior here.

To see only behavior changes to Main.cpp after the mechanical prerequisite:

```sh
git diff origin/startup/shutdown-refactor..origin/lab/startup-isolation -- SerialPrograms/Source/CommonFramework/Main.cpp
```

## General mechanism and lab policy

| Component | Contract | Upstream treatment |
|---|---|---|
| `StartupArguments.h/.cpp` | Pure parser and exclusive GUI/test dispatch; an ordinary no-argument launch remains GUI; documented Qt options are preserved. | Reusable candidate, with its own argument tests. |
| `LabStartupPolicy.h/.cpp` | Require an explicit macOS lab profile or select the compiled identity; reject a conflict. | Keep on lab branches. Never impose this default on normal releases. |
| `Main.cpp` wiring | Parse, apply lab policy, then enter exactly one startup path. Explicit tests skip saved settings, GUI restoration and startup device discovery. | Upstream wiring must omit the lab adapter. |
| `Filesystem::set_startup_profile` | Store an already-validated identity before paths are cached. | Review separately with existing upstream profile behavior and platform support. |
| `PA_LAB_PROFILE` CMake option | Bind the lab binary to one profile even when the OS relaunches it without arguments. | Keep lab-only. |

The general parser and lab policy have separate native test tables. The lab
regressions also inject entry-point callbacks to verify rejected launches enter
neither path and explicit tests never enter the GUI boundary. macOS CI checks
actual invalid executable invocations. These tests do not exercise hardware or
prove every registered test body is device-free.

Examples:

- `SerialPrograms --test bdsp`: reject with status 2 before Qt/settings/devices.
- Ordinary upstream-style parsing with no arguments: select GUI.
- Unbound macOS lab binary with no arguments: lab policy rejects it.
- Binary built with `PA_LAB_PROFILE=lab-B`, relaunched without arguments: GUI
  selects `Profiles/lab-B`.
- The same binary with `--profile lab-A`: reject before saved settings load.
- `--command-line-test-mode`: enter the isolated test path, even for a bound lab
  build; no console profile may be supplied with the tests.

## Reuse the pending headless runner upstream

[PokemonAutomation/Arduino-Source#1845](https://github.com/PokemonAutomation/Arduino-Source/pull/1845)
already provides `SerialProgramsTests` on fork branch `upstream/headless-tests`
(inspected at `006d547f9f5baa2d1e064755b6e45817a6b20aa4`). Its executable owns
QCoreApplication, test selection, logging and shutdown. Do not submit #233's
interim `run_test_application()` as another upstream implementation of that
lifecycle.

The proposed upstream sequence is:

1. Review/land the existing headless-runner work and independently review #234's
   mechanical extraction. Neither depends on lab policy.
2. Prepare a clean fork draft for general argument validation on those upstream
   prerequisites. Preserve no-argument GUI behavior and documented Qt arguments;
   run parser regressions on all platforms. Resolve compatibility for the legacy
   GUI test-mode flag using the existing runner rather than copying its lifecycle.
3. Keep the lab adapter, compiled profile and lab deployment instructions in
   `lab/main`. Consume the accepted general mechanism by merging main as usual.

No pending headless-runner changes are imported into #233, and this document is
not authorization to publish or modify an upstream PR. The clean upstream
argument-validation follow-up has not been opened yet.

## Remaining boundary

Settings isolation cannot stop two different profiles from naming the same
physical serial port. A future device lease must be enforced before a port is
opened, including the first connection/reset. Shell and channel ownership alone
do not protect that boundary. No deployment, permission reset or console run is
part of these PRs.
