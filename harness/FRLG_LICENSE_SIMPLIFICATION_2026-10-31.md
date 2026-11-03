# FRLG license wait simplification

User requested reducing fork PR #59 now that online-license handling is optional.
Updated its head from `298e92be6` to `6b4ba3706` using an isolated worktree on
`frlg/license-simplify`; no other agent's worktree was edited.

PR diff: 3 files, 57 insertions / 14 deletions, formerly 5 files,
161 insertions / 9 deletions. Removed public GameLaunch/LaunchWait types,
choose_launch_wait(), automatic Home/license detection, remembered modes and
69 lines of tests for the removed selection policy. A private helper preserves
the fixed-delay A press when the checkbox is off, and waits for startup black
(up to 30 seconds) when on. Existing retry loop handles timeout. No new bare
Boolean parameter or cross-module API was introduced.

Moved existing settings option registration to a private add_options() method
so the constructor no longer grows beyond its code-health baseline. Both the
new helper and registration method are within code-health limits. Checked
against origin/main: zero failures. CRLF-aware whitespace check passed.

Dispatched build/native-test workflows for the exact commit:
- Mac: https://github.com/dayfine/Arduino-Source/actions/runs/41242989506
- Windows clang: https://github.com/dayfine/Arduino-Source/actions/runs/41242992074
- Ubuntu: https://github.com/dayfine/Arduino-Source/actions/runs/41242994388

CI results pending at this checkpoint. No local build or console run was done
for the simplification; previous always-on launch validation is not validation
of this reduced implementation. PR description was shortened and updated to
state that limitation.
