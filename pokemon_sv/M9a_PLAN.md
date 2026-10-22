# M9a plan: faster, bounded Auto Battle quests

Status: plan, 2026-10-21. No code yet. It builds on the evidence in [M9.md](M9.md)
and on what the Material Farmer already does well. Paths are relative to
`SerialPrograms/Source/VideoGames/PokemonSV`.

## Problem

An Auto Battle 10 quest took 4–12 minutes. An Auto Battle 30 took 17.5 minutes
with a correct Lv.100 lead. Most of that time is not battling.

## Where the time goes

`quest_auto_battle()` (`Programs/Farming/PokemonSV_BlueberryQuests.cpp`) loops
until the kill target is reached. Each pass:

1. **Fly to Chargestone Cavern** (`central_to_chargestone()`), wait 3 s for
   spawns, and walk a fixed path into the battle court.
2. **Clear with Let's Go** (`use_lets_go_to_clear_in_front()` in
   `Programs/ShinyHunting/PokemonSV_LetsGoTools.cpp`). It sends the lead out,
   then returns once no kill has been heard for 6 s.
3. **Return to Central Plaza** (`return_to_plaza()` in
   `Programs/PokemonSV_Terarium.cpp`): open the map and fly.
4. **Heal**: walk to the nurse, mash A, then mash B until the overworld.
5. **Fly from Central Plaza to Central Plaza again**, to put the player back on
   the fly point after walking to the nurse. `central_to_chargestone()` moves
   the map cursor by a fixed offset from the player's position, so it needs
   that.

Measured on one pass (bbq-m5 log, 2026-10-21 12:42:00 → 12:43:41):

| Step | Seconds | Needed? |
| --- | ---: | --- |
| Clear (including the final 6 s with no kill) | ~26 | Yes: this is the battling |
| Return to Central Plaza | ~28 | Partly: leaving the cave respawns the Pokémon |
| Walk to the nurse and heal | ~14 | Only when HP is low |
| Fly Central Plaza → Central Plaza | ~14 | Only because of the walk to the nurse |
| Fly to Chargestone plus the 3 s spawn wait | ~19.5 | Yes, for the next pass |
| **Total** | **~100** | |

Codex's notes measured the return, heal and re-fly together at 55.8–56.6 s per
pass. Passes gained 0–4 detected kills, typically 1–2, so an Auto Battle 10 is
5–7 passes.

The kill target is only checked at the top of the loop. The pass that reaches it
still finishes clearing, then returns, heals and re-flies before the loop exits.
That cost 71 s after the tenth kill in one example. There is no limit on passes
or time: if kills stop registering (wrong lead, no spawns, missing audio), the
loop never ends, and the farmer's retry and fallback handling (M4/M6) can't
step in.

## What the Material Farmer already does

`run_material_farmer()` (`Programs/Farming/PokemonSV_MaterialFarmerTools.cpp`)
uses the same Let's Go core (`OverworldSensors`, `LetsGoEncounterBotTracker`,
`use_lets_go_to_clear_in_front()`), but:

- **It heals only when needed.** `LetsGoHpWatcher` reads the lead's HP from the
  Let's Go overlay during the clear, and `check_hp()` heals only below
  `AUTO_HEAL_PERCENT`, using `auto_heal_from_menu_or_overworld()` from the menu
  rather than walking to a nurse.
- **It runs from incidental battles.** Each trip is wrapped in
  `run_from_battles_and_back_to_pokecenter()`.
- **It caps errors.** It recovers from up to 15 consecutive failures by
  resetting the game.

The kill count is live. The kill-sound detector's callback increments
`LetsGoEncounterBotStats::m_kills` as each kill is heard (`LetsGoTools.cpp:164`),
so a caller can see the target being reached during a clear.

## Plan: four PRs, each with tests

Each PR keeps the round and quest interfaces unchanged: `quest_auto_battle()`
still returns `void`, and success is still decided by the board check in
`process_and_do_quest()`. An early return is simply a failed attempt that M4/M6
handle.

### PR 1: seam and pass log (no behavior change)

- Move the loop's decisions into
  `run_auto_battle(Logger&, BBQAutoBattleActions&, const BBQAutoBattlePolicy&)`,
  with the game steps behind an interface:
  - `fly_to_battle_area()`: fly to Chargestone, wait for spawns, walk in;
  - `clear(remaining)`: returns the kills heard and the last known HP;
  - `return_to_plaza()`, `heal()`.

  The console version uses today's calls, buttons and timings.
- Log one line per pass: pass number, kills this pass, total, elapsed time, and
  the reason for the next step. That gives the per-pass data M9 asks for.
- Tests use a scripted fake that reports kills and HP per pass and charges
  each step its measured seconds (from the table above). That pins today's
  behavior, as the FRLG #1663 tests did: Auto Battle 10 at 2 kills per pass
  costs 5 passes, and the final pass still heals.
- Size: about +120 to +160 lines of code and +150 to +200 of tests.

### PR 2: stop at the target

- Add an opt-in early stop to `use_lets_go_to_clear_in_front()`: an optional
  predicate checked while waiting, which ends the clear once it returns true.
  Its default is none, so the other three callers (Scatterbug, the Area Zero
  platform hunt and the Material Farmer) are unchanged.
- In `run_auto_battle()`, stop clearing once the kills reach the target, and
  skip the heal and re-fly on that pass. Still return to Central Plaza, because
  the next quest starts from there.
- Saves about 30–35 s per quest (the 6 s tail, heal and re-fly). It also stops
  the over-clearing seen in the logs (34 kills for an Auto Battle 30).
- Size: about +30 to +50 lines of code (including the helper change) and +40 to
  +60 of tests.

### PR 3: heal only when HP is low (a behavior change, in its own PR)

- Run `LetsGoHpWatcher` during the clear. Skip the heal and re-fly when the
  lead's last known HP is at or above a threshold.
- Unknown HP still heals, which is today's behavior.
- Saves about 28 s on each pass that skips the heal: about 2 minutes per
  Auto Battle 10 (about 5 passes) and about 4–5 minutes per Auto Battle 30
  (the 17.5-minute run was about 10 passes), with a strong lead.
- **Decision needed:** a new BBQ option `Auto Battle: heal below HP %`.
  - Default 100 keeps today's behavior, but then nobody gets the speedup
    without changing a setting.
  - Default 50, matching the Material Farmer's style, makes it a behavior
    change by default, so the PR title must say so and maintainers decide.
- Size: about +40 to +60 lines of code (option included) and +50 to +70 of
  tests.

### PR 4: bound the attempt

- Stop the attempt after N passes in a row with no kill, or after a total time
  limit. It then returns to Central Plaza and leaves the outcome to the board
  check and M4/M6.
- Starting values, to tune from logs: 3 empty passes in a row, and a total of
  about 3 × the slowest observed time (Auto Battle 10: about 35 min; Auto
  Battle 30: about 60 min). Log which limit ended the attempt.
- Size: about +30 to +50 lines of code and +60 to +80 of tests.

## Experiments, only after PR 1's pass log and a recorded cave visit

These need live evidence before any code:

- **E1, cheaper spawn reset:** fly from Chargestone straight back to Chargestone,
  skipping Central Plaza. Does that respawn the Pokémon? The map cursor would
  start on Chargestone, so the fixed-offset cursor move would change.
  Potential saving: about 30 s per pass.
- **E2, more kills per visit:** after a clear, walk to a second spot in the
  cave and clear again before leaving, as the Material Farmer does with
  several forward moves per trip. This needs a recorded visit showing where
  the spawns are.
- **E3, why passes get 0 kills:** is it no spawns in front, targets out of
  reach, a missed kill sound, or the wrong lead (the M8 party bug confounded
  the earlier traces)? Record one full visit with the pass log before tuning
  anything.

## Expected effect

For an Auto Battle 10 at 2 kills per pass with a strong lead, the model in
PR 1's tests gives:

- **Today:** 5 passes × ~100 s, about 8.3 minutes.
- **After PRs 2 and 3:** about 6 minutes.
- **After E1, if it works:** about 4 minutes.

These are model numbers from one measured pass. The PR 1 pass log, under
comparable conditions (same lead, full HP), is how to confirm them.

## Validation

- **Native tests in every PR.** The fake's step costs make the time savings
  visible in the tests, as with the FRLG `ExitWildBattle` idle-seconds checks.
- **Live runs for PRs 2 and 3, with a correct Lv.100 lead:** an Auto Battle 10
  and an Auto Battle 30. Record per-pass kills, HP and time, the number of heals
  skipped, the total duration, and the board check. Compare with the M9
  baselines: Auto Battle 10 at 227–249 s with the correct lead, and Auto Battle
  30 at 17m33s.
- **PR 4:** force a no-progress case live (for example, a lead that can't
  battle) only if it is safe. Otherwise rely on the native tests.

## Open questions for the user

1. The heal threshold default for PR 3 (see above).
2. Whether E1 and E2 are worth live time before M8a, since M8a affects every
   battle route through the lead.
3. Who implements: by [AGENTS.md](../AGENTS.md), Codex implements and validates
   and Claude owns the PRs. This plan is written so either can start PR 1.
