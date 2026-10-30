# AutoStory: checkpoints as data (event model)

Status: design proposal, 2026-10-29. No code yet. Builds on
[AutoStory-Design.md](AutoStory-Design.md) (walker, planner, screen machine),
[AutoStory-Navigation-Stack.md](AutoStory-Navigation-Stack.md) (shared core) and
[AutoStory-Lessons.md](AutoStory-Lessons.md) (what broke). Examples:
[autostory-examples/](autostory-examples/).

**Problem.** A checkpoint today is a C++ function: walks, presses and waits in a row, with
checks added where a run once failed. Three failures keep coming back (Lessons 1, 5, 7, 11–13):

1. **The pose drifts unconfirmed.** Counted taps or holds lose a tile; nothing reads the
   position back until the next action fails (Mom, r108b; the mine stairs, r91–r93).
2. **Talks are assumed.** "A beside X" starts nothing, or starts the wrong talk (Roark's rock,
   r105), or the NPC is not ready yet (Roark before Barry, r101).
3. **Dialogues end by count.** Mashing for N seconds or N presses answers a Yes/No wrongly
   (nurse, r85) or stops mid-scene (r86, 01.02's walk-along).

The fix in each case was the same shape: *state what must hold, act, check what the game
shows, recover by a named rule*. This doc makes that shape the unit of authoring.

## 1. Prior art

| Source | Core abstraction | Failure, "where am I" | Borrow |
|---|---|---|---|
| Upstream SV AutoStory (`PokemonSV/Programs/AutoStory/`) | checkpoint = class wrapping `checkpoint_NN()`, a lambda run by `checkpoint_reattempt_loop` | any exception → `reset_game()` (reload save), retry; report at 10, give up at 100; no per-step preconditions ("exactly correct starting position") | save-backed retry; **avoid** the opaque lambda. No other game upstream has AutoStory (LZA has navigation helpers only) |
| [pokebot-gen3](https://github.com/40Cakes/pokebot-gen3) | `BotMode` generators + event hooks; `map_path.py` plans waypoints | per-waypoint timeout, re-read position (RAM), replan | "arrived?" check per step, replan from the actual position; we get it from anchors, not RAM |
| [pret/pokefirered](https://github.com/pret/pokefirered) | `map.json`: objects, warps, coord events, signs; `scripts.inc`: flags, text | n/a (data) | generate FRLG NPC tiles, trigger tiles, flags and expected text (already our FRLG map source) |
| [RouteOne](https://github.com/HRoll/poke-router) / [RouteTwo](https://github.com/Dabomstew/pokemon-routetwo) | speedrun route = one line per trainer/event, for damage/exp | none | a route as a plain list of events |
| [Poke-Controller-Modified](https://github.com/Moi-poke/Poke-Controller-Modified) | Python `do()` of press/wait/template match | none: fixed sequences (our failure mode) | nothing; a warning |
| [Honorbuddy profiles](https://github.com/BosslandGmbH/Honorbuddy-Profiles) ([behaviours](https://github.com/BosslandGmbH/Honorbuddy-Quest-Behaviors)) | XML `QuestOrder`: `PickUp`, `If`/`While`, `TurnIn`, `CustomBehavior InteractWith` | every step guarded by quest-log state, so a restart skips finished steps | **closest match**: route as data, verbs as code, a guard per step for resume |
| [OpenKore eventMacro](https://openkore.com/wiki/eventMacro) | automacros fire when state + event conditions hold | handlers queue behind the AI | interrupts as declared condition → handler pairs |
| [BehaviorTree.CPP v4](https://www.behaviortree.dev/docs/guides/pre_post_conditions/) | XML trees, blackboard; `_skipIf`/`_successIf`/`_failureIf`/`_while`, `_onSuccess`/`_post` | conditions checked at node start (`_while` each tick) | the field names: `skip_if`, `requires`, `expect` |
| [BTs in Robotics and AI](https://arxiv.org/abs/1709.00084) (Colledanchise, Ögren) | PPA: Fallback(postcondition?, Sequence(precondition, action)) | idempotent: re-running a done step is a no-op | "effect already observed → skip" makes resume a re-check |
| [Nav2 BT navigator](https://docs.nav2.org/rolling/getting_started/nav2_behavior_trees/detailed_behavior_tree_walkthrough/detailed_behavior_tree_walkthrough/), AMCL | `RecoveryNode` (6 retries) around navigate; recoveries clear/spin/wait/back up by error code; pose = belief corrected by landmarks | escalating recoveries chosen by failure type | named, escalating recoveries (wait → re-anchor → stop); counted steps are dead reckoning until a landmark confirms |
| [GOAP](https://www.gamedevs.org/uploads/three-states-plan-ai-of-fear.pdf) (Orkin), [Fluid HTN](https://github.com/ptrefall/fluid-hierarchical-task-network), [SHOP2](https://www.cs.umd.edu/projects/shop/) | actions with preconditions/effects + planner; HTN: compound tasks with guarded methods | replan from the current world state | precondition/effect sets; HTN "methods" = alternative approaches to one talk. Not the planner |
| [Screenplay](https://serenity-bdd.github.io/docs/screenplay/screenplay_fundamentals), [Page Object](https://martinfowler.com/bliki/PageObject.html) | actors perform tasks of interactions, ask **questions**; one object per page | state is read explicitly by questions | doing vs observing split: every effect is a question; one detector set per screen |
| [Playwright](https://playwright.dev/docs/actionability) ([assertions](https://playwright.dev/docs/test-assertions)) | auto-wait until actionable; assertions retry until true or timeout | never sleep; the timeout fails | "advance until overworld is stable", never by count |

What we take, in short: **pre/post-conditions on every step with "skip if already true"**
(BT PPA, BT.CPP v4 `_skipIf`/`_successIf`, GOAP), so resuming is re-checking, not
re-counting; **wait for an observable, never sleep** (Playwright web-first assertions,
Screenplay "Questions"); **re-localise on landmarks, then replan** (Nav2 recovery,
robotics landmark localisation), not replay; **profiles as data** (Honorbuddy quest
profiles, pokebot modes) with a small fixed vocabulary interpreted by code.

What we do not take: a general planner (GOAP/HTN search over actions). The story is a
fixed order; the only search we need is the tile planner we already have. Nor a full
behaviour-tree runtime: our trees are one level deep (sequence of events, each with a
fallback list), which plain structs express.

## 2. The model

```
Checkpoint  = start (save + pose) , interrupts[] , events[] , end (expect + save?)
Event       = skip_if? , requires? , do: Action , expect: Effect[] , verify? ,
              pose_after? , recover: Rule[]
Action      = route_to | anchor | walk | talk | dialogue | battle | menu | save | sequence
Effect      = a question the screen answers: screen, pose, banner, bag, tile_free, dialogue_opened
Interrupt   = detect (during which actions, which stop) , handle: Action , pose_after , then
```

### 2.1 State: what we can observe

| Part | Read from | Notes |
|---|---|---|
| Screen | ScreenMachine detectors: overworld, dialogue, battle, menu, bag, black | two frames agree; "stable_ms" = overworld held that long |
| Pose | map + set of possible tiles + facing + `sure` (tile, column, row, set) | tracked by the walker, **corrected only by anchors** |
| Inventory | Bag OCR (item names, counts) | checked at events that change it |
| Flags | the save (what it was made after) + this run's confirmed effects | never "assumed done"; see §2.6 |

**Rule (from Navigation-Stack §3):** the screen is read, the pose is tracked. An event whose
precision needs `sure: tile` must be preceded by an anchor that delivers it.

### 2.2 Confirming the pose: anchors

An anchor is an action whose stop *fixes* coordinates, whatever happened before it:

| Anchor | Pins | Example |
|---|---|---|
| `wall` walk until blocked (held past arrival) | one axis | Route 201 corner (42,3): north then west |
| `npc_block` walk until a known-still NPC stops us | one axis + adjacency | Barry in the Trainers' School |
| `warp` / transition + banner OCR (fuzzy) | map, entry tile | Sandgem banner, house door |
| `dialogue` the NPC who answered | adjacency to that NPC | Mom's row is the row that answered |
| `landmark` image match | tile | BDSP intro landmarks (to be retired) |
| `facing_reader` sprite | facing | FRLG |

Every `route_to` already plans to end legs on walls (planner extra cost for open stops); the
event model adds `confirm: [anchor…]` so an event *checks* the tile before the action that
needs it, and `pose_after` so an event that learned something (the row that answered) writes
it back. Grass hides `blocked`; there, `walk_into` with A on every step does the stopping.

### 2.3 Talks: approach rules, then proof

`talk = {approach, press, opens_within_ms, attempts}`, then `expect: dialogue_opened`, and for
story NPCs a `verify` on the **story effect**, since an opened dialogue is not proof (rock, r105).

| Approach | Use when | Example |
|---|---|---|
| `face` dir from a confirmed tile | NPC still, our tile anchored | Roark from below, Barry at the gym door |
| `walk_into` dir until blocked/dialogue, A each step | NPC wanders along a known line | Poké Mart lady from her row's corner |
| `probe` face dir + A, step perpendicular, up to N | one axis known, other not | Mom: column from the door, row by probing |
| `probe_sides` A towards N/S/E/W | walk stopped next to someone | fallback after `walk_into` |

`opens_within_ms` is per NPC (story NPCs zoom first: 8 s for Roark, r98). The first press is
A; advancing is B where the next A would re-open the talk (Pokétch president), A where B would
answer No (nurse, r85).

### 2.4 Dialogues end on the screen, never on a count

`dialogue = {advance: A|B, line_gap_ms, answers[], until: Effect, max_s}`:

- `until` is usually `{screen: overworld, stable_ms: 4000}` (a trainer's quiet gap before the
  battle, r83) or `{screen: battle}`; `max_s` is a cap that fails, not a plan.
- `answers` match a prompt by OCR (`{"prompt": "heal", "choose": "Yes"}`); no prompt is
  answered by position in a press count.
- A scene with an animation in the middle (healing jingle, r86; walk-along, 01.02) is just
  `stable_ms` long enough, or a second `dialogue` whose `skip_if` is "already overworld".

### 2.5 Interrupts

Declared once per checkpoint (or shared sets), active during walking actions. Each says how
it is **detected** (the walker's stop: dialogue, battle flash, message, card), how it is
**handled** (an Action), what it does to the **pose** (`unchanged`, `sight_line` = the
leg's tiles inside the trainer's sight, `elapsed_time` = inexact), and what happens **then**
(`rewalk_leg` for wall legs, `replan_current_event`, `relocalise`). Never chain two inexact
poses (AutoStory.md §5.2): after one, the next precise action needs an anchor.

Known kinds: `wild_battle_flee`, `trainer_battle` (expected on `legs_crossing_sight`),
`talk_only`, `wanderer_block` (wait, rewalk), `tutorial_card` (B), `repel_wore_off`,
`scene_dialogue`.

### 2.6 Failure, retry and resuming mid-checkpoint

- **Each recover rule names a failure** (`blocked_unexplained`, `probe_exhausted`,
  `expect_failed`, `verify_failed`, `pose_set_too_wide`…) and a bounded action. Anything not
  named is `stop`: screenshot, close the game, report (Lessons 3). Transient causes (a
  wanderer, a wild battle eating a walk's time) get retries; deterministic ones never do.
- **Resume = re-check, not replay.** On reload, the runner walks the event list and skips
  every event whose `skip_if` holds on the screen (Potions in the Bag → skip the lady). Events
  without an observable `skip_if` are re-run from the save. That is BT.CPP's `_skipIf` and
  GOAP's "effects already satisfied".
- **Flags.** A flag (`barry_sent_us_to_mine`) is set only by a confirmed effect, lives in the
  run, and is lost on reload. So: either put a save after the event that sets it, or make the
  flag re-derivable (a `skip_if` that observes it). The schema check rejects a `requires: flag`
  with neither.
- **Mid-checkpoint saves** are events (`save` action) and split the checkpoint for resume.

### 2.7 C++ sketch (plain structs, free functions)

```cpp
namespace PokemonAutomation::AutoStoryEvents{

enum class Sure{TILE, COLUMN, ROW, SET, UNKNOWN};
struct Pose{ std::string map; std::vector<Pokemon::TilePos> tiles; std::optional<Pokemon::Direction> facing; Sure sure; };

//  A question the screen answers. Kind + arguments, as parsed from JSON.
struct Effect{ std::string kind; JsonObject args; };     // screen, pose, banner, bag, tile_free, dialogue_opened, flag

struct Action{                                          // one of; `steps` for "sequence"
    std::string kind;                                   // route_to, anchor, walk, talk, dialogue, battle, menu, save, ...
    JsonObject args;
    std::vector<Action> steps;
};
struct Rule{ std::string on; Action then; size_t retries = 0; };

struct Event{
    std::string id, why;
    std::vector<Effect> skip_if, requires, expect, verify;
    Action action;
    std::vector<Action> confirm;                        // anchors run before `action`
    std::optional<Pose> pose_after;
    std::vector<Rule> recover;
    std::optional<std::string> sets_flag;
};
struct Interrupt{ std::string id; Effect detect; Action handle; std::string pose_after, then; };
struct Checkpoint{ std::string id; Pose start; std::vector<Effect> start_requires;
                   std::vector<Interrupt> interrupts; std::vector<Event> events;
                   std::vector<Effect> end_expect; bool save_at_end; };

//  What a game supplies. Each entry is registered by `kind`; the runner never knows BDSP vs FRLG.
struct RunState{ Pose pose; std::set<std::string> flags; };
struct GameBindings{
    std::map<std::string, MoveOnlyFunction<bool(RunState&, const JsonObject&)>> questions;      // Effect kinds
    std::map<std::string, MoveOnlyFunction<Outcome(RunState&, const Action&)>> actions;        // Action kinds
    std::map<std::string, MoveOnlyFunction<Outcome(RunState&, const Interrupt&)>> interrupts;
};

Checkpoint parse_checkpoint(const JsonValue& json);                   // + schema checks (flags, ids, kinds)
std::vector<std::string> lint_checkpoint(const Checkpoint&, const GameBindings&);   // offline test
void run_checkpoint(RunState& state, const Checkpoint&, GameBindings& game, StepTrace& trace);

}
```

`Outcome` is `{ok | failure name, pose update}`; `run_checkpoint` maps a failure name to the
event's `recover` rules, else stops. Actions call the existing pieces (`walk_route`,
`walk_dir`, `talk_to_adjacent_npc`, `finish_dialogue`, `fight_trainer`, `bag_has_item`).
Each event logs as a `StepLog` step with its `why`, so the overlay shows the checkpoint's
steps for free. Offline tests run checkpoints against fake bindings (scripted screen answers),
like the leg walker's tests.

**JSON vs C++ literals.** JSON keeps checkpoints reviewable and lets the Lab Console run one
event at a time (stepping through new ground, Lessons 10, becomes "run event 3"). Upstream
ships no data files for programs today; a fallback is the same structs as C++ aggregate
literals in `Data/`, generated or hand-written. Decide before the first PR (§4).

## 3. Worked examples

All in [autostory-examples/](autostory-examples/), from the code on `bdsp/floaroma-mvp` and
`frlg/brock-mvp` and the Lessons runs. Abridged:

**Mom at home (00.04)** — `bdsp-00.04-mom.json`. The door anchors the column, not the row.
`talk.approach = probe{face E, A, step N, max 4}`; `pose_after` records Mom's row. Advance with
B until overworld is stable; `expect` the Parcel in Key Items (proposed check: today the code
takes the talk as proof).

**Poké Mart lady (Route 201)** — `bdsp-route201-mart-lady.json`. `skip_if` Potions in the
Bag (resume). `route_to` the row's west corner, `confirm` it with wall anchors north and
west, then `walk_into{E, A each step}` since grass hides `blocked` and she wanders.
`expect` ≥ 10 Potions; else stop ("this save already had hers").

**Roark (01.03)** — `bdsp-01.03-roark.json`. An explicit `talk_barry_gym_door` event `sets`
`barry_sent_us_to_mine`; `talk_roark` `requires` it. Walls pin the mine's stairs; `wanderer`
and `stopped_on_sign` are named recoveries. Below Roark is reached by four anchors (wall,
rock, bottom wall, two steps). `verify: tile_free roark` catches the rock's line (r105).

**Route 202 trainers** — `bdsp-route202-trainer-interrupt.json`. One event (`route_to`
the Jubilife exit); trainers are an interrupt with `expected_on_legs`, a dialogue → battle
→ Potion handler, `pose_after: sight_line`, `then: replan_current_event`. `talk_only` keeps
the 4 s quiet-gap rule (r83).

**FRLG Mewtwo (post-game)** — `frlg-cerulean-cave-mewtwo.json`. `start.requires` the
`after_hall_of_fame` flag (from the save; it also removes the cave guard from the map), Surf,
and a Max Repel. One planned route across warps and water (D-pad walks are exact; warps and
the facing reader confirm). Repel-wore-off and wild battles are interrupts.
The checkpoint ends on `screen: battle` and hands off to a catch policy.

## 4. Open questions

1. **Data format upstream.** JSON files under `Resources/` (loaded at run time) or C++
   literals? JSON allows Lab Console stepping and no-rebuild edits; literals keep type checks
   and match upstream's habits. A middle way: JSON in the fork's lab, literals generated for PRs.
2. **How generic are `args`?** A typed `std::variant` per action kind catches typos at
   parse time; `JsonObject` keeps the core small. Start typed for the five common kinds
   (`route_to`, `anchor`, `talk`, `dialogue`, `battle`), `JsonObject` for the rest.
3. **Named tiles vs coordinates.** Examples use names (`POTION_GIVER_ROW_WEST_END`) that live
   in the map data; the linter must resolve each against `MapSet`/FRLG map points.
4. **Pose sets in FRLG.** FRLG walks exactly, so `sure` is always `tile` there except after a
   wild battle mid-leg; is the set machinery worth sharing, or BDSP-only?
5. **Flags across reloads.** Is "save after any flag-setting event" acceptable on cost (each
   save ~10 s), or do we want per-flag observers (e.g. Roark gone from his tile)?
6. **Converting #1717's segments.** Butters' recorded segments are timed holds; converting
   them is a rewrite per checkpoint, not a mechanical port. Start with 00.04 and 01.03,
   which are already anchor-based.
7. **Screen machine vs event runner.** Menu-like stretches (intro, keyboard) stay
   `ScreenMachine` routines invoked as one `menu`/`scene` action. Should dialogues also be
   `ScreenMachine` states (answers as states) rather than a bespoke `dialogue` action?
