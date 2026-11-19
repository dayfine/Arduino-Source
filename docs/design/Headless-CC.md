# Headless Mode for SerialPrograms (`--headless`)

Design note for running `SerialPrograms` without `MainWindow` or Qt widgets, anchored on **`lab/main`** (`356aa15e1`).

## 1. Motivation & Prior Work

Today, running any automation program in `SerialPrograms` enters `StartupMode::GUI` in `CommonFramework/Main.cpp`, which constructs `MainWindow`, shows and raises it (`w.show(); w.raise();`), builds the full `QWidget` tree (`ProgramSelect`, `ButtonDiagram`, `LoggerWindow`, video preview + overlays), and saves UI state on exit.

For multi-instance emulator workflows (such as parallel `mGBA` instances, see [Lab-mGBA-Console.md](https://github.com/dayfine/Arduino-Source/blob/agents/docs/design/Lab-mGBA-Console.md)) and automated lab runs:
- **No window / focus stealing:** Background workers should not pop up GUI windows or steal focus.
- **Lower overhead:** Skipping `QWidget` construction and live video preview/overlay rendering avoids unnecessary CPU/GPU work per instance.
- **Direct one-shot execution:** A runner can launch `SerialPrograms` for one program run with a profile and options file, wait for completion, and observe an exit code without needing `LabChannel` or an MCP server.

### Relation to Prior Work (`#206`/`#207` and `Agent-Integration.md`)
- **`#206` / `#207` (`use_qt_ui()`, `SerialProgramsTests` on `lab/main`):** Introduced a separate `QCoreApplication` executable for unit tests with its own user folder and no settings/device startup. Headless program execution instead uses a `--headless` flag in `Main.cpp` so it shares the single `SerialPrograms` binary, `LabStartupPolicy` (`--profile` + profile lease), and full device/video backends without duplicating a second application target.
- **`Agent-Integration.md` ([`lab/tooling`](https://github.com/dayfine/Arduino-Source/blob/lab/tooling/docs/design/Agent-Integration.md)) & `LabChannel` (`lab/main`):** `LabChannel` (`CommonFramework/Windows/LabChannel.{h,cpp}`) is already on `lab/main` and drives long-lived interactive sessions via `Lab/inbox` and `Lab/state.json`. One-shot `--headless --run` is complementary and has no `LabChannel` prerequisite; attaching `LabChannel` to `HeadlessHost` is deferred until a long-lived headless daemon worker is needed.

## 2. Existing Seams on `lab/main`

- **`StartupArguments` & `LabStartupPolicy` (`CommonFramework/Startup/`):** `parse_startup_arguments()` and `apply_lab_startup_policy()` validate flags before Qt, settings, or device discovery, and `reserve_lab_profile()` (`Main.cpp:399`) acquires an exclusive profile lease.
- **`PanelHolder` (`CommonFramework/Panels/PanelTools.h`):** Abstract interface (`report_new_panel_intent`, `load_panel`, `on_busy`, `on_idle`) implemented only by `MainWindow`.
- **`PanelDescriptor::make_panel()` & `PanelSession` (`CommonFramework/Panels/PanelSession.h`):** Instantiates a `PanelSession` with **no** widgets; `make_ui_component()` is only called in `MainWindow::load_panel()` (`MainWindow.cpp:427`).
- **`ProgramSession` (`CommonFramework/ProgramSession.h`):** Exposes `start_program()`, `stop_program()`, `current_state()`, and `Listener::state_change(ProgramState)` / `Listener::error(std::string)` without requiring UI widgets.
- **`PanelListDescriptor` (`CommonFramework/Panels/PanelList.h`):** Pure C++ catalog whose `panels()` builds `std::vector<PanelEntry>` on demand; only `PanelListWidget` and `ProgramSelect` (`PanelLists.h`) are `QWidget`s.

## 3. Design

### 3.1 Startup Arguments, Platform Scope, & Profile Safety (`StartupArguments.*`, `LabStartupPolicy.*`)

Extend `StartupMode` (`HEADLESS`) and `StartupPlan` on `lab/main`:
- **macOS-only in PR 2:** On `lab/main`, `--profile` and `reserve_lab_profile()` are macOS-only (`#if defined(__APPLE__)`, `LabDeviceRuntime.cpp:52`, `LabStartupPolicy.cpp`). Until cross-platform profile/device isolation exists, `--headless` is **rejected outside macOS** in `apply_lab_startup_policy()`.
- **`--headless` requires `--profile <name>`** (or `PA_LAB_PROFILE`): `apply_lab_startup_policy()` must be updated so it does not return early for `StartupMode::HEADLESS` (and `LabStartupPolicy_Tests` covers it). Rejecting `--headless` without a profile prevents opening the default profile's serial port (e.g. console A's port) when `make_panel()` + `load_json()` constructs `ControllerSession` and calls `open_connection()`.
- **Profile lease:** Goes through `reserve_lab_profile()` in `Main.cpp` before any port scan, camera probe, or panel creation. If the profile lease is already held, headless startup fails immediately with exit code `3` without touching any device (GUI path keeps its existing exit code `2` at `Main.cpp:402`).
- Rejected if combined with `--command-line-test-mode`.
- `--run <identifier>`: Program identifier (`PanelDescriptor::identifier()`, e.g. `"UnitTestRunner"` or `"Pokemon FireRed/LeafGreen:AutoStory"`). Required with `--headless` in PR 2.
- `--options <path>`: Optional JSON file applied via `PanelSession::load_json()` after `load_json_from_global()`, overriding options for this single run.
  - **No `ConsoleSetup` in `--options`:** `ConsoleProgramSession::load_json()` passes `ConsoleSetup` to `m_system.load_json()`, which reloads/reopens devices (`LabChannel.cpp:129-139` strips it for the same reason). In `--headless --run`, `--options` containing `ConsoleSetup` is rejected with exit code `4` before opening any device.

### 3.2 `PanelCatalog` & Shared Panel Loading

Currently `ProgramSelect` (`SerialPrograms/Source/PanelLists.h`, `PanelLists.cpp`) owns `std::vector<std::unique_ptr<PanelListDescriptor>> m_lists` and `select_program(identifier)`, while `PanelListWidget::handle_panel_clicked()` (`CommonFramework/Panels/UI/PanelListWidget.cpp:90`) stores `std::shared_ptr<const PanelDescriptor>`, calls `make_panel()`, loads JSON, calls `load_panel()`, and writes `PERSISTENT_SETTINGS().panels[JSON_PROGRAM_PANEL]`.

Extract widget-free free functions `make_all_panel_lists()` and `find_panel(lists, identifier)` into `Source/PanelCatalog.{h,cpp}` (in PR 1, no behavior change):
- **Ownership (no caching in PR 1):** `PanelListDescriptor::panels()` creates a fresh `std::vector<PanelEntry>` per call (`PanelList.h`). `find_panel(lists, identifier)` scans `panels()` per lookup (matching `ProgramSelect::select_program()` today) and returns the matching `PanelEntry` with its category index; the caller converts its `std::unique_ptr<PanelDescriptor>` into an owning `std::shared_ptr<const PanelDescriptor>`. `HeadlessHost` keeps that `shared_ptr` alongside `m_current_panel` so it outlives the `PanelSession`.
- **Shared load helper:** Extract the `report_new_panel_intent()` -> `make_panel()` -> `load_json()` -> `load_panel()` sequence out of `PanelListWidget::handle_panel_clicked()` into a shared helper with a `bool persist_selection` flag (`true` in GUI, `false` in headless) that reports errors via return/exception instead of opening `QMessageBox` directly.

### 3.3 `HeadlessHost` (Widgetless `PanelHolder`)

Add `HeadlessHost : public PanelHolder`:
- **Owns:**
  - the panel lists from `make_all_panel_lists()`
  - `std::shared_ptr<const PanelDescriptor> m_current_panel_descriptor`
  - `std::unique_ptr<PanelSession> m_current_panel`
  - (`ProgramSession::run_program()` already holds a `SleepSuppressScope` at `ProgramSession.cpp:323`, so `HeadlessHost` needs no `m_sleep_scope` and does not rely on `on_busy()`/`on_idle()`.)
- **Settings & state persistence (no writes in one-shot mode):**
  - `report_new_panel_intent()` and `load_panel()` **never** call `save_settings()` or write `ProgramCategory` / `ProgramPanel`.
  - `Main.cpp` skips `PERSISTENT_SETTINGS().write()` on exit when `--headless` is active.
  - `ControllerSettingsTable` (`NintendoSwitch_ControllerSettings.cpp:588`, which calls `save_settings()` on a connection thread when meeting a new controller) and `STATS_FILE` updates are guarded/skipped in `--headless` so concurrent workers never clobber saved GUI settings or race on stats files.

### 3.4 Qt Application, Dialogs, Services, and Video/Audio Capture

- **Keep `QApplication` (no `MainWindow`) in PR 2:** Session/connect-path code still opens `QMessageBox` (`SerialPABotBase2_Connection.cpp:140`, `ErrorReports.cpp:371` in `send_all_unsent_reports`, `NewVersionCheck.cpp:145`, `HardwareValidation_x86.tpp`, `SetupSettings.cpp:85`), and `set_permissions(QObject&)` uses `qApp`. Moving to `QCoreApplication` is deferred until those are decoupled. In headless mode:
  - Pass `application` (not `MainWindow`) to `set_permissions(application)` if camera/mic permissions are needed.
  - Skip `check_new_version()`, `send_all_unsent_reports()`, and Discord (`run_on_start`) / social SDK startup.
  - Convert `check_resource_folder()`, `check_hardware()`, and connection-dialog errors to log lines + non-zero exit codes (GUI path keeps its existing exit code `1` for startup checks).
  - Never construct `MainWindow` or any `QWidget`.
- **Video & audio capture:** Capture backends live in the sessions (`CameraQt6QVideoSink` holds `std::unique_ptr<QVideoSink>`; `VideoSource_MgbaLink` on `lab/tooling` polls `FRAME`) and audio feeds live in session state, so capture runs without any display widget.
- **macOS `open -n` constraint:** On macOS, camera capture permissions require launching via `open -n`, which does not return the child exit code to the caller. Direct `exec` with exit-code inspection applies to sources that do not need macOS camera permissions (such as `MgbaLink`); `open -n` capture-card runs report results via the profile's `SerialPrograms.log` (or `Lab/state` when `LabChannel` is enabled).

### 3.5 One-Shot Execution, Readiness, Exit Codes, and Concurrency

1. **Select & verify runnable:** `HeadlessHost` looks up `<id>` via `find_panel()`, instantiates `PanelSession`, applies `PERSISTENT_SETTINGS().panels[id]`, validates/applies `--options` (rejecting `ConsoleSetup` or parse errors with exit `4`), and casts with `dynamic_cast<ProgramSession*>` (`LabChannel.cpp:58`). If null (e.g. a settings panel), exit `4`.
2. **Per-device leases & readiness:**
   - Per-device leases (`acquire_lab_serial`, `acquire_lab_capture` from `lab-devices.json`) throw `UserSetupError` on the connect path; in headless mode, log the error and fail fast with exit `5` (no wait, no dialog).
   - Otherwise, if the panel has a console (`UnitTestRunner` has no console and skips this wait), wait up to a bounded timeout (e.g. 10 s) for the controller status (`"ready"` / `"none"` / `""`, factored from `LabChannel.h:88`, `LabRequest.h:70` without requiring `LabChannel`) to become `"ready"`, exiting `5` on timeout.
3. **Start & run:** Call `start_program()`. (When merged with `lab/tooling`'s `ProgramTransition.h`, pass `ActionSource::CLI`, never `ActionSource::UI`, so `LabHold` and `Lab/state` do not treat a headless start as a user click.)
4. **Completion & signals:**
   - `ProgramSession::Listener::state_change()` and `error()` are called from the program thread; the listener posts updates to the Qt event loop via `QMetaObject::invokeMethod(QCoreApplication::instance(), ...)` rather than acting in the callback.
   - **Exit code `1` source on `lab/main`:** `ProgramSession::run_program()` calls `report_error()` -> `Listener::error(message)` on every error branch. Headless defines exit `1` as `error()` received between `RUNNING` and `STOPPED` (ignoring the known `update_historical_stats_with_current()` `"Unable to save stats."` message, which is also avoided when `STATS_FILE` writes are skipped). Once merged with `lab/tooling`, `ActionSource::PROGRAM_ERROR` on `last_transition()` replaces this.
   - **POSIX signals (`SIGINT` / `SIGTERM`):** Use `QSocketNotifier` on a self-pipe to post `stop_program()` on the event loop and wait up to a bounded timeout (e.g. 5 s) for `STOPPED`; a second signal exits immediately (`130` for `SIGINT`, `143` for `SIGTERM`).
   - **Concurrency (`AGENTS.md`):** No new `std::thread`, `std::mutex`, or `std::async`; use `PokemonAutomation::Mutex` / `ConditionVariable`, `GlobalThreadPools`, and the Qt event loop.
5. **Flush before exit:** All log flushes (`global_file_logger().stop()` in `stop_application_services()`) complete before returning the exit code.

| Exit Code | Meaning |
|---|---|
| `0` | Program stopped cleanly after normal completion |
| `1` | Program error (`Listener::error()` during run; `ActionSource::PROGRAM_ERROR` on `lab/tooling`) |
| `2` | Invalid startup arguments or policy rejection (`dispatch_startup`) |
| `3` | Profile lease already held (`reserve_lab_profile`) or startup check failure |
| `4` | Unknown `--run` identifier, non-program panel, invalid `--options` JSON, or `ConsoleSetup` in `--options` |
| `5` | Per-device lease `UserSetupError`, controller readiness timeout, or `start_program()` refused |
| `130` / `143` | Stopped by `SIGINT` (`130`) or `SIGTERM` (`143`) |

## 4. Rollout Plan

1. **PR 1 — `PanelCatalog` extraction (no behavior change):**
   - Move the panel-list factories and the lookup out of `ProgramSelect` (`PanelLists.{h,cpp}`) into `make_all_panel_lists()` / `find_panel()` and share the panel-load helper with `PanelListWidget::handle_panel_clicked()`.
   - Acceptance: Existing GUI behavior and `StartupArguments_Tests` / `LabStartupPolicy_Tests` unchanged.
2. **PR 2 — `--headless --run` + `HeadlessHost` (macOS-only):**
   - Add `StartupMode::HEADLESS`, `--headless`, `--run`, `--options` to `StartupArguments` + `LabStartupPolicy` (macOS-only, requires `--profile`) with unit tests.
   - Implement `HeadlessHost`, readiness/error listeners, POSIX signal handling, and `Main.cpp` wiring (keeping `QApplication` with no `MainWindow`).
   - Add a `HeadlessHost` unit test using a fake `PanelDescriptor` / `ProgramSession`.
   - Acceptance: `SerialPrograms --headless --profile <name> --run "UnitTestRunner" --options <unit-test-filter.json>` (against `UNIT_TEST_RESOURCE_PATH()`) runs a filtered test subset with no window created, writes no GUI settings, and exits `0` on pass (`1` on test failure).
3. **Future (deferred):**
   - Non-macOS `--profile` / device isolation, Windows `SetConsoleCtrlHandler`, and flushing before `std::quick_exit(ret)` (`Main.cpp:331, 355`) when `PA_ENABLE_QT_ADOPTION_WORKAROUND` is active.
   - Attaching `LabChannel` to `HeadlessHost` for long-lived headless daemon workers.
   - Moving from `QApplication` to `QCoreApplication` once session-level `QMessageBox` calls are decoupled.
