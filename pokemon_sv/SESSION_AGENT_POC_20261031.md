# ProgramSession agent proof of concept

User requested a minimal draft after comparing lab tooling with upstream #1519.

- Fork draft: https://github.com/dayfine/Arduino-Source/pull/116
- Branch: `prototype/program-session-agent`, commit `c92772397`.
- Base: `lab/tooling` (#107), so review only the prototype diff.
- Real adapter: four JSON methods through `session_agent <JSON>` in the lab channel.
- Shared dispatcher with an in-memory native terminal demo; no hardware involved.
- Prototype README describes the question, commands, outcomes and remaining gaps.
- Actual start/stop errors now propagate through TrackableProgram and ProgramTracker.
- Local native demo exercised rejection, acceptance, STOPPING/completion, unloaded
  IDs, reserved IDs and invalid ID types. Full-flags syntax checks passed for
  LabChannel, ProgramSession, ProgramTracker and the live adapter. Code-health
  against origin/lab/tooling and CRLF-aware whitespace checks passed.
- CI dispatched: Mac 41280320249, Windows Clang 41280322339, Ubuntu 41280324392.
  Still running/queued when the draft was created; no pass claimed.
- No app deployment or console interaction. Concurrent session destruction and
  per-run errors/history remain explicit production gaps.
- One command: `sh devtools/prototypes/session-agent/run-demo.sh` from the branch.
- PR branch handed to Claude per existing ownership rules.
