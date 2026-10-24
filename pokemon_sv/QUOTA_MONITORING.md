# Codex quota checks

2026-10-24 PDT. The user asked Codex to monitor quota and pace ongoing work.

Read the locally signed-in Codex account's actual quota windows:

```sh
python3 /Users/difan/Projects/Arduino-Source-agents/tools/read_codex_quota.py
```

The helper uses the documented read-only
[`account/rateLimits/read` method](https://learn.chatgpt.com/docs/app-server#6-rate-limits-chatgpt).
It initializes a short-lived local app-server, reads quota windows, prints
only percentages/reset times and terminates that child process. It does not
start a model turn, consume a reset, change accounts or expose credentials.
It needs an installed Codex CLI and an existing compatible sign-in. This is
workflow orchestration, not a Python test harness for SerialPrograms.

Check before a large work chunk, after a substantial milestone and before
starting another long autonomous iteration. Report which bucket/window the
reading belongs to, its timestamp and reset time in the user's timezone.
When allowance is low, save a concrete checkpoint and pace remaining work
around the reset instead of launching another large chunk blindly. The
percentage is account-wide, can change with other sessions, and is not a
guarantee of how much a particular task will cost.

Source scope matters: this reads the local CLI account. Do not assume a
separately authenticated browser, API-key client or another account shares
that quota. If unavailable, say the quota is unknown; do not substitute token
counts, context-window occupancy, a goal budget or an old snapshot. Account
details and live quota snapshots are not committed to this repository.

Monitoring runs while a task is active; there is no installed background daemon
or guaranteed automatic resumption after a turn ends. The user can also inspect
limits with `/status` in Codex CLI or the usage dashboard, per the
[official usage documentation](https://learn.chatgpt.com/docs/pricing).
