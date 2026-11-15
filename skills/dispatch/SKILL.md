---
name: dispatch
description: Runs the main session's lane board and loop for coding subagents on dayfine/Arduino-Source (and PokemonSimulation) - write the brief, claim branches so no two agents touch one stack, stage at most 3 agents, track a status table, and relay results to the user in a fixed shape. Use when starting, steering or reporting on background agents (autostory-code, pr-maintenance, reviewer, run-records), when planning merges up a PR stack, or when a task notification arrives.
---

# Dispatch

The main session owns console B and the plan; agents code. Standing rules live in the agent
definitions (#473) - never retype them in a brief.

## Before spawning

1. **Fit the work to an agent type**: code change → `autostory-code`; merge/conflicts/description → `pr-maintenance`;
   review → `reviewer`; notes, tooling, agents branch, design docs → `run-records`.
   A handful of tool calls or a known one-file fix: do it yourself instead.
2. **Claim branches** (every branch the agent will push to, including merge-forward targets):
   `sh ~/.claude/skills/dispatch/scripts/claims.sh claim <label> <branch>...`
   On `CONFLICT`, don't spawn: queue it behind the holder, or narrow the scope.
3. **Staging**: at most 3 agents running. Never give new work to an agent that is wrapping up -
   queue it as your own next item. A merge up a stack is one agent, bottom-up, parents before children.
   Work that depends on an open agent's branch waits for its notification.

## Brief (only these, in this order)

```
Goal: <one sentence, with the user's words if it's their request>
Branch/PR: <branch> (#N), tip <sha>; base <branch>
Scope: in - <list>; out - <branches/files not to touch, other agents' areas>
Ruled out / known: <what's already decided or tried>
Done when: <pushed + CI started + what to report>
(No attribution lines: no commit trailers or PR-body ending; user, 2026-11-14.)
```
Add a time limit only if it is not the 2 h default. Name skills (`tdd`, `diagnose`) only when
the brief needs something beyond the definition's default.

## Status table

Keep `~/BDSPLab/dispatch/status.md` current on every spawn, notification and merge; format in
[REFERENCE.md](REFERENCE.md#status-table).

## On a notification

1. Interim ("waiting on its own background work"): no relay, no action.
2. Final: `claims.sh release <label>`; update the table; check the claims of queued work and start
   the next item if its branches are free.
3. Verify what matters before relaying: the CI result if it's done, PR state, anything the agent says
   it could not do. Never claim CI is green from the agent's word alone.
4. Follow-ups the agent left (merge forward, a sibling branch) become queued items with claims, not
   messages to the finished agent.

## Relay to the user

```
<Agent> done: <one-line outcome>.
- What changed: <PR / tip / the 2-4 changes that matter>
- CI: <URL + verdict, or "running">
- Your call: <numbered decisions, each with my recommendation>   (omit if none)
- Next: <what I started or queued>
```
Agents' results are not visible to the user: relay what matters, drop internal IDs and file dumps.

## Tools

- `scripts/claims.sh claim|release|check|list` - branch ownership (`~/BDSPLab/dispatch/claims.tsv`).
- Board files live outside git in `~/BDSPLab/dispatch/` (`lanes.tsv`, `claims.tsv`, `status.md`). New board:
  copy `templates/lanes.tsv` (header only) there and add one row per lane. `scripts/lanes_test.sh` tests `lanes.sh`.

## Lanes (keep every lane moving)

Six lanes: FRLG, PLZA, seams (+PR maintenance), MCP, mGBA, BDSP. Board: `~/BDSPLab/dispatch/lanes.tsv`,
one row per lane (state, owner, since, next, wait_on). It holds only *now*; history stays in status.md / the checklist.

- `scripts/lanes.sh show` - the board with minutes in state.
- `scripts/lanes.sh stalled` - exit 1 and one line per lane needing action: IDLE, STALE (waiting-ci/console > 30 min),
  ASK (waiting-user > 2 h).
- `scripts/lanes.sh set LANE state|owner|next|wait_on VALUE` - setting state stamps `since`.

States: `moving` (agent or console working), `waiting-ci`, `waiting-console`, `waiting-user`, `idle`, `paused` (user's call only).

**Every state change updates the board in the same turn**: spawn → moving; agent done → waiting-ci with the run id
and the follow-up as `next`; CI read → idle (or moving if I act at once); user question → waiting-user.
A lane never goes `idle` without a `next`; if the queue is empty, `next` = "plan next stage" and the tick does that.

**Lane tick** (each 10-min pulse, and after every notification):
1. `lanes.sh stalled`. Exit 0 → one-sentence pulse, done.
2. STALE waiting-ci → read that CI once; act on the result (merge / fix agent) and set the new state.
3. IDLE lanes, in priority order FRLG > PLZA > seams > MCP > mGBA, while agents < 3: claim, spawn the `next` item,
   set moving. Console lanes (FRLG/PLZA runs) are mine, not an agent slot; one console train at a time.
4. ASK → repeat the open question to the user in one line (once per 2 h), keep the lane waiting-user.
5. Pulse sentence names what the tick started. Skip the tick while the user is driving B or mid-conversation.

### Loop

Install with `/loop 10m` and this prompt, verbatim:

> 10-min lane tick: if the user sent a message in the last 10 minutes or is driving console B, do nothing. Otherwise run `sh ~/BDSPLab/checkin.sh` and `sh ~/.claude/skills/dispatch/scripts/lanes.sh stalled`. If stalled exits 0: post ONE plain sentence (console B state + last checkpoint result, what's running, next step). If it lists lanes: follow the "Lane tick" section of the dispatch skill (read STALE CI once and act, spawn IDLE lanes' next item in priority order while agents < 3, repeat ASK questions once per 2 h), update lanes.tsv, then post one sentence naming what the tick started.

`~/BDSPLab/checkin.sh` is a copy of `lab/scripts/checkin.sh` on this branch.
