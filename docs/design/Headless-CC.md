# Headless Mode for SerialPrograms (`--headless`)

Design note for running `SerialPrograms` without `MainWindow` or Qt widgets, anchored on **`lab/main`** (`356aa15e1`).

## 1. Motivation & Prior Work

Today, running any automation program in `SerialPrograms` enters `StartupMode::GUI` in `CommonFramework/Main.cpp`, which constructs `MainWindow`, shows and raises it (`w.show(); w.raise();`), builds the full `QWidget` tree (`ProgramSelect`, `ButtonDiagram`, `LoggerWindow`, video preview + overlays), and saves UI state on exit.

For multi-instance emulator workflows (such as parallel `mGBA` instances, see [Lab-mGBA-Console.md](https://github.com/dayfine/Arduino-Source/blob/agents/docs/design/Lab-mGBA-Console.md)) and automated lab runs:
- **No window / focus stealing:** Background workers should not pop up GUI windows or steal focus.
- **Lower overhead:** Skipping `QWidget` construction and live video preview/overlay rendering avoids unnecessary CPU/GPU work per instance.
- **Direct one-shot execution:** A runner can launch `SerialPrograms` for one program run with a profile and options file, wait for completion, and observe an exit code without needing `LabChannel` (`lab/tooling`) or an MCP server.

### Relation to Prior Work (`#206`/`#207` and `Agent-Integration.md`)
- **`#206` / `#207` (`use_qt_ui()`, `SerialProgramsTests` on `lab/main`):** Introduced a separate `QCoreApplication` executable for unit tests with its own user folder and no settings/device startup. Headless program execution instead uses a `--headless` flag in `Main.cpp` so it shares the single `SerialPrograms` binary, `LabStartupPolicy` (`--profile` + profile lease), and full device/video backends without duplicating a second application target.
- **`docs/design/Agent-Integration.md` (`LabChannel` on `lab/tooling`):** `LabChannel` drives long-lived interactive sessions via `Lab/inbox` and `Lab/state.json`. One-shot `--headless --run` is complementary and has no `LabChannel` prerequisite; `LabChannel` attachment to `HeadlessHost` is deferred until a long-lived headless daemon worker is needed.

## 2. Existing Seams on `lab/main`

- **`StartupArguments` & `LabStartupPolicy` (`CommonFramework/Startup/`):** `parse_startup_arguments()` and `apply_lab_startup_policy()` validate flags before Qt, settings, or device discovery, and `reserve_lab_profile()` (`Main.cpp:399`) acquires an exclusive profile lease.
- **`PanelHolder` (`CommonFramework/Panels/PanelTools.h`):** Abstract interface (`report_new_panel_intent`, `load_panel`, `on_busy`, `on_idle`) implemented only by `MainWindow`.
- **`PanelDescriptor::make_panel()` & `PanelSession` (`CommonFramework/Panels/PanelSession.h`):** Instantiates a `PanelSession` with **no** widgets; `make_ui_component()` is only called in `MainWindow::load_panel()` (`MainWindow.cpp:427`).
- **`ProgramSession` (`CommonFramework/ProgramSession.h`):** Exposes `start_program()`, `stop_program()`, `current_state()`, and `Listener::state_change(ProgramState)` without requiring UI widgets.
- **`PanelListDescriptor` (`CommonFramework/Panels/PanelList.h`):** Pure C++ catalog of `PanelEntry` items; only `PanelListWidget` and `ProgramSelect` (`PanelLists.h`) are `QWidget`s.

## 3. Design

### 3.1 Startup Arguments & Profile Safety (`StartupArguments.*`, `LabStartupPolicy.*`)

Extend `StartupMode` / `StartupPlan` on `lab/main`:
- `--headless`: Runs without `MainWindow` or any `QWidget`s.
  - **Requires `--profile <name>`** (or `PA_LAB_PROFILE`): `apply_lab_startup_policy()` rejects `--headless` without a profile. This prevents a headless run from opening the default profile's serial port (e.g. console A's port) when `make_panel()` + `load_json_from_global()` constructs `ControllerSession` and calls `open_connection()`.
  - Goes through `reserve_lab_profile()` in `Main.cpp` before any port scan, camera probe, or panel creation. If the profile lease is already held, startup fails immediately with exit code `3` without touching any device.
  - Rejected if combined with `--command-line-test-mode`.
- `--run <identifier>`: Program identifier (`PanelDescriptor::identifier()`, e.g. `"UnitTestRunner"` or `"Pokemon FireRed/LeafGreen:AutoStory"`).
  - Required with `--headless` in PR 2 (since `LabChannel` is deferred).
- `--options <path>`: Optional JSON file passed to `PanelSession::load_json()` after `load_json_from_global()`, overriding options for this single run.

### 3.2 `PanelCatalog` & Shared Panel Loading

Currently `ProgramSelect` (`SerialPrograms/Source/PanelLists.h`, `PanelLists.cpp`) owns `std::vector<std::unique_ptr<PanelListDescriptor>> m_lists` and `select_program(identifier)`, while `PanelListWidget::handle_panel_clicked()` (`CommonFramework/Panels/UI/PanelListWidget.cpp:90`) constructs `std::shared_ptr<const PanelDescriptor>`, calls `make_panel()`, loads JSON, calls `load_panel()`, and writes `PERSISTENT_SETTINGS().panels[JSON_PROGRAM_PANEL]`.

Extract a widget-free `PanelCatalog` (in its own PR, no behavior change):
- **Ownership:** `PanelCatalog` owns the `PanelListDescriptor`s for the lifetime of the host (outliving `PanelSession`, which holds `const PanelDescriptor&`). `find_panel(identifier)` returns a `std::shared_ptr<const PanelDescriptor>` (using `std::shared_ptr`'s aliasing constructor against the catalog/list lifetime) so it can be passed directly to `PanelHolder::load_panel()`.
- **Shared load helper:** Extract the `report_new_panel_intent()` -> `make_panel()` -> `load_json()` -> `load_panel()` sequence out of `PanelListWidget::handle_panel_clicked()` into a shared helper that takes a `bool persist_selection` flag (true in GUI, false in headless) and returns/throws errors instead of opening `QMessageBox` directly.

### 3.3 `HeadlessHost` (Widgetless `PanelHolder`)

Add `HeadlessHost : public PanelHolder`:
- **Owns:**
  - `PanelCatalog&` (outlives `HeadlessHost`)
  - `std::shared_ptr<const PanelDescriptor> m_current_panel_descriptor`
  - `std::unique_ptr<PanelSession> m_current_panel`
  - `std::unique_ptr<SleepSuppressScope> m_sleep_scope` (held for the duration of the run; `on_busy()`/`on_idle()` are only called by widgets through `global_panel_holder()`, so `HeadlessHost` does not rely on them)
- **Settings persistence (no writes in one-shot mode):**
  - `report_new_panel_intent()` and `load_panel()` **never** call `save_settings()` or write `ProgramCategory` / `ProgramPanel`.
  - `Main.cpp` skips `PERSISTENT_SETTINGS().write()` on exit when `--headless` is active.
  - Thus `--options <path>` is strictly a one-run override, and concurrent runs cannot clobber saved GUI settings.

### 3.4 Qt Application, Dialogs, and Video/Audio Capture

- **Keep `QApplication` (no `MainWindow`) in PR 2:** Session/connect-path code still opens `QMessageBox` in several places (`SerialPABotBase2_Connection.cpp:140`, `ErrorReports.cpp:371` in `send_all_unsent_reports`, `NewVersionCheck.cpp:145`, `HardwareValidation_x86.tpp`, `SetupSettings.cpp:85`), and `set_permissions(QObject&)` uses `qApp`. Moving to `QCoreApplication` is deferred until those are decoupled. In headless mode:
  - Pass `application` (not `MainWindow`) to `set_permissions(application)` if camera/mic permissions are needed, or skip when no camera is used.
  - Skip `check_new_version()` and `send_all_unsent_reports()` interactive prompts; convert `check_resource_folder()`, `check_hardware()`, and connection-dialog errors to log lines + non-zero exit codes.
  - Never construct `MainWindow` or any `QWidget`.
- **Video & audio capture:** Capture backends live in the sessions (`CameraQt6QVideoSink` holds `std::unique_ptr<QVideoSink>`; `VideoSource_MgbaLink` on `lab/tooling` polls `FRAME`) and audio feeds live in session state, so capture runs without any display widget.
- **macOS `open -n` constraint:** On macOS, camera capture permissions require launching via `open -n`, which does not return the child process exit code to the caller. Direct `exec` with exit-code inspection applies to controller/video sources that do not need macOS camera permissions (such as `MgbaLink`, or Linux/Windows runs); `open -n` capture-card runs report results via log/state files.

### 3.5 One-Shot Execution, Readiness, Exit Codes, and Concurrency

1. **Select & configure:** `HeadlessHost` looks up `<id>` in `PanelCatalog` (exit `4` if unknown), creates `PanelSession`, loads `PERSISTENT_SETTINGS().panels[id]`, and if `--options <path>` is given, parses and applies `load_json()` (exit `4` on read/parse error).
2. **Wait for controller readiness:** Instead of a fixed sleep, observe `ProgramSession::Listener::state_change(ProgramState)` (posted to the Qt event loop via `QMetaObject::invokeMethod(QCoreApplication::instance(), ...)`, never acting synchronously inside the program-side callback). If `current_state() == ProgramState::NOT_READY`, wait up to a bounded timeout (default e.g. 10 s) for `NOT_READY` to clear; on timeout, exit `5`.
3. **Start & run:** Call `start_program()`. (When merged with `lab/tooling`'s `ProgramTransition.h`, pass `ActionSource::CLI`, never `ActionSource::UI`, so `LabHold` and `Lab/state` do not treat a headless start as a user click.)
4. **Completion & signals:**
   - `state_change(ProgramState::STOPPED)` posts completion to the Qt event loop and calls `QCoreApplication::exit(code)`.
   - **Signals:** POSIX (`SIGINT`/`SIGTERM`) uses `QSocketNotifier` on a self-pipe; Windows uses `SetConsoleCtrlHandler` setting an atomic flag / posting via `QMetaObject::invokeMethod` from the OS handler thread. Both call `stop_program()`, wait for `STOPPED`, and exit `130`.
   - **Concurrency (`AGENTS.md`):** No new `std::thread`, `std::mutex`, or `std::async`; use `PokemonAutomation::Mutex` / `ConditionVariable`, `GlobalThreadPools`, and the Qt event loop.
5. **Exit path & Windows `quick_exit`:** On Windows (`PA_ENABLE_QT_ADOPTION_WORKAROUND`), `Main.cpp` terminates with `std::quick_exit(ret)` (`Main.cpp:331, 355`) rather than joining leaked threads. All disk flushes (`global_file_logger().stop()`, any state output) must complete **before** `std::quick_exit(ret)`, and `ret` is the exit code below:

| Exit Code | Meaning |
|---|---|
| `0` | Program stopped cleanly after normal completion |
| `1` | Program error / unhandled exception (on `lab/tooling`: `ActionSource::PROGRAM_ERROR`) |
| `2` | Invalid startup arguments or policy rejection (`dispatch_startup`) |
| `3` | Profile lease already held (`reserve_lab_profile`) or startup check failure |
| `4` | Unknown `--run` identifier or invalid `--options` JSON |
| `5` | Controller readiness timeout or `start_program()` refused |
| `130` | Stopped by `SIGINT` / `SIGTERM` |

## 4. Rollout Plan

1. **PR 1 — `PanelCatalog` extraction (no behavior change):**
   - Extract `PanelCatalog` from `ProgramSelect` (`PanelLists.{h,cpp}`) and share the panel-load helper with `PanelListWidget::handle_panel_clicked()`.
   - Acceptance: Existing GUI behavior and `StartupArguments_Tests` / `LabStartupPolicy_Tests` unchanged.
2. **PR 2 — `--headless --run` + `HeadlessHost`:**
   - Add `--headless`, `--run`, `--options` to `StartupArguments` + `LabStartupPolicy` (`--headless` requires `--profile`) with unit tests.
   - Implement `HeadlessHost`, signal handling, exit-code mapping, and `Main.cpp` wiring (keeping `QApplication` with no `MainWindow`).
   - Add a `HeadlessHost` unit test using a fake `PanelDescriptor` / `PanelSession`.
   - Acceptance: `SerialPrograms --headless --profile <name> --run "UnitTestRunner"` runs to completion with no window created, writes no GUI settings, and exits `0`.
3. **Future (only if needed):**
   - Attach `LabChannel` (`lab/tooling`) to `HeadlessHost` for long-lived headless daemon workers.
   - Investigate `QCoreApplication` once session-level `QMessageBox` calls are decoupled.
