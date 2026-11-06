# AutoStory interface sketches and example data

Companion to [the cross-game architecture review](AutoStory-Shared-Architecture.md).
These are **proposed API/data examples**, not implemented public interfaces or
validated game fixtures. Supporting types and routine error handling are omitted
from the code sketches. JSON versions, extension names and sample IDs below are
illustrative. No existing runtime loader accepts these documents yet.

The useful division is shared orchestration plus game adapters. Battle mechanics
and map authoring/planning live in PokemonSimulation; hardware actions and screen
observations live in Arduino. Use the same selected plan, policy IDs and semantic
contracts on both sides. Sharing a contract does not require identical C++ and
Python class hierarchies.

## 1. Story events: common data with typed game extensions

The core should know about travel, battle, heal, shop, capture, learning and
checkpoint operations. A game extension carries a namespaced schema and payload.
Its registered game adapter validates that payload and decodes it into a concrete
game-specific type. The shared engine never includes FRLG/BDSP/SV action headers.

C++ shape; `Destination`, `Requirement` and `Observation` are tagged records,
not arbitrary callbacks serialized into JSON:

```cpp
// Registered game-specific action, e.g. "frlg/cut-obstacle/1".
struct GameAction{
    std::string schema;
    JsonValue payload;
};

// Common actions; definitions carry each action's required parameters.
using StoryAction = std::variant<
    TravelAction, TrainerBattleAction, HealAction, ShopAction,
    CaptureAction, LearnMoveAction, GameAction
>;

// Data only: predicates have IDs and arguments, not captured C++ lambdas.
struct StoryEventData{
    std::string id;
    std::vector<Requirement> preconditions;
    StoryAction action;
    std::vector<Requirement> postconditions;
    RecoveryPolicy recovery;
};

// Exactly one game/version and data profile apply to a selected plan.
struct StoryPlanData{
    GameProfile game;
    DataPins pins;
    std::vector<CheckpointData> checkpoints;
};
```

Reuse/extend the existing
`StoryStep` -> `Event` conversion and `run_checkpoint()`/`run_segment()` ownership,
rather than introducing a competing second retry engine.

Example FRLG checkpoint fragment: synthetic tree/object IDs and conditions, not
a sourced route. Traversal authorization includes whatever move/badge/unlock
requirements this game needs; a missing requirement cannot be inferred from the
presence of an action in the plan.

```json
{
  "format": "autostory-plan/1",
  "game": {"id": "firered", "adapter": "frlg/1"},
  "checkpoint": {
    "id": "example-tree-crossing",
    "save_policy": "after_success",
    "events": [
      {
        "id": "cut-tree",
        "preconditions": [
          {"type": "capability.available", "capability": "field.cut"},
          {"type": "world.facing_object", "object": "example-tree"}
        ],
        "action": {
          "type": "game",
          "schema": "frlg/cut-obstacle/1",
          "payload": {"object": "example-tree", "direction": "north"}
        },
        "postconditions": [
          {"type": "world.object_absent", "object": "example-tree"}
        ],
        "recovery": {"max_attempts": 2, "on_uncertain_effect": "reobserve_or_stop"}
      }
    ]
  }
}
```

This is a fragment: a complete export also needs source/data/map pins, a starting
state, a save-generation identity and the full checkpoint contract. Apply a
registered schema for every action and predicate. The extension's game family
must match the selected game, with an explicit allowlist for shared families
such as FRLG. Unknown action versions, predicates or required extensions reject
the plan during preflight, before any controller inputs or saves.

An optional metadata annotation can be ignored; an executable action, collision
extension or required precondition cannot. `GAME` is an extension point, not
permission for the offline replay to assume that an opaque scene succeeded.

### One runner, several executors

```cpp
// Success is not the default when a check is unreadable.
enum class CheckResult{ SATISFIED, VIOLATED, UNKNOWN };

// Report partial effects and interruptions as well as completed actions.
struct ActionReport{
    ActionStatus status;
    std::vector<Observation> observations;
    std::vector<EffectEvidence> effects;
    std::optional<Failure> failure;
};

// Game-owned actions and observation; orchestration stays in the runner.
class GameExecutor{
public:
    virtual ~GameExecutor() = default;
    virtual ValidationReport validate(const StoryPlanData& plan) const = 0;
    virtual CheckReport observe(std::span<const Requirement> requirements) = 0;
    virtual ActionReport execute(const StoryAction& action, const StoryState& before) = 0;
};
```

`FrlgExecutor`, `BdspExecutor` and `SvExecutor` each implement these operations by
calling their own existing movement, menu, battle and detector routines. A visitor
dispatches the common action variant; the game registry dispatches a `GameAction`:

```cpp
// Inside FrlgExecutor's game-action dispatch, after schema/payload validation.
if (action.schema == "frlg/cut-obstacle/1"){
    const CutObstacle request = decode_cut_obstacle(action.payload);
    return cut_obstacle_with_screen_confirmation(m_io, request);
}
return unsupported_action(action.schema);
```

This is a game-local branch, not a growing cross-game switch in the runner.
Decode into `CutObstacle` only after validation; a generic `JsonValue` does not
make the payload untyped. Most actions should become common semantic operations
once two games need them; one-off puzzles/scenes can remain extensions.

| Selected game | Common travel implementation | Example required extension |
|---|---|---|
| FRLG | Plan/walk tile-map legs with elevation and warp rules | `frlg/cut-obstacle/1` |
| BDSP | Robust tile/belief route plus screen-confirmed transitions | `bdsp/use-hidden-move/1` |
| SV | Fly point/landmark/heading navigation | `sv/ride-action/1` |
| LGPE | Measured collision/transition adapter; Joy-Con operations | `lgpe/catch-throw/1` |
| Ruby/Sapphire | R/S tile-step/warp adapter | `rs/bike-action/1` |

All five extension names describe proposed dispatch, not existing implementations.
Travel's destination is also tagged: `tile` (named map, frame, coordinates,
elevation/facing) or `landmark` (region, landmark, heading/traversal constraints).
SV must not invent a tile coordinate to satisfy a tile-only API.

The shared runner's sequence is:

1. Preflight the complete selected plan against executor capabilities and pins.
2. Observe required preconditions; VIOLATED or UNKNOWN stops a hard requirement.
3. Mark certainty/input state invalid as appropriate before starting movement.
4. Execute through the game adapter; reconcile evidenced effects and observations,
   including those from a failed or interrupted action.
5. Observe postconditions. A completed button sequence is not proof of its effect.
6. If needed, spend the runner's bounded recovery budget and choose a supported
   reobservation/localization/reload path. Resume only with adequate evidence.
7. Save only under the checkpoint policy after its guards succeed.

A failed postcondition does not undo a consumed Potion or a partial move. Retry
only after reconciling effects; reset rolls back to an identified save snapshot.
The offline executor supplies derived/sampled effect evidence, labeled as such;
it cannot manufacture a hardware observation. Unsupported detailed replay can
retain an opaque action and report incomplete coverage, not a simulated success.

## 2. Battles: load a ruleset, keep player policy and NPC AI separate

A battle ruleset consists of both **data and mechanics code**. JSON can select a
registered implementation and pinned data; it cannot implement arbitrary new
mechanics by changing a generation number. Prefer an adapter around a suitable
existing library to another handwritten damage engine.

Illustrative manifest; the backend names and package identities are placeholders:

```json
{
  "format": "battle-ruleset/1",
  "id": "firered-gen3/1",
  "game": "firered",
  "mechanics": {"backend": "registered-frlg-backend/1", "profile": "firered"},
  "data": {"pack": "firered-reference", "revision": "example-immutable-revision"},
  "npc_ai": "frlg-trainer-ai/1",
  "formats": ["singles"],
  "required_capabilities": ["damage", "accuracy", "status", "stat-stages", "items"]
}
```

The real loader must require actual immutable revisions/checksums; the placeholder
above must be rejected by production validation. Support is a claim to test:
load-time capability checks are supplemented by entry validation (moves, abilities,
items, format and game-specific mechanics) and mechanics regression cases.

Python interfaces belong in PokemonSimulation. This sketch leaves `BattleState`,
`BattleAction`, `PolicyView` and related domain types to the battle schema:

```python
class MechanicsBackend(Protocol):
    def require_capabilities(self, capabilities) -> None: ...
    def validate_entry(self, entry) -> None: ...
    def initialize(self, entry) -> BattleState: ...
    def requests(self, state) -> Sequence[DecisionRequest]: ...
    def advance(self, state, choices, rng) -> PhaseResult: ...
    def outcome(self, state) -> Optional[BattleOutcome]: ...


@dataclass(frozen=True)
class LoadedRuleset:
    id: str
    mechanics: MechanicsBackend
    npc_policy: NpcPolicy
    data_revision: str


def load_ruleset(manifest, registry):
    validate_manifest_and_data_pins(manifest)
    factory = registry.require(manifest["mechanics"]["backend"])
    mechanics = factory.load(manifest["mechanics"]["profile"], manifest["data"])
    mechanics.require_capabilities(manifest["required_capabilities"])
    npc_policy = registry.load_npc_policy(manifest["npc_ai"], manifest["data"])
    return LoadedRuleset(manifest["id"], mechanics, npc_policy,
                         manifest["data"]["revision"])
```

Production code raises typed unsupported/data errors and reports them as
incomplete evaluation, not a loss or a win. The backend owns battle initialization
and decision phases, including automatic effects and forced replacements.
`DecisionRequest` contains a side, phase, permitted policy view and legal actions.

The common simulation loop is deliberately small:

```python
def simulate(entry, ruleset, player_policy, rng, max_phases):
    ruleset.mechanics.validate_entry(entry)
    state = ruleset.mechanics.initialize(entry)
    policies = {"player": player_policy, "foe": ruleset.npc_policy}
    policy_rng = {"player": rng.player_policy, "foe": rng.npc_policy}
    trace = []
    for _ in range(max_phases):
        outcome = ruleset.mechanics.outcome(state)
        if outcome is not None:
            return completed(outcome, state, trace)
        choices = {}
        for request in ruleset.mechanics.requests(state):
            choice = policies[request.side].choose(
                request.view, request.legal_actions, policy_rng[request.side])
            require_legal(choice, request.legal_actions)
            choices[request.id] = choice
        result = ruleset.mechanics.advance(state, choices, rng.mechanics)
        trace.append(result.events)
        state = result.state
    outcome = ruleset.mechanics.outcome(state)
    return completed(outcome, state, trace) if outcome is not None else phase_limit(state, trace)
```

Each policy sees only the information its game permits; the player does not get
hidden opponent IVs or a future action from the simulator. `NpcPolicy` includes
trainer-specific settings and may encode game-specific access to player state.
Separate, recorded RNG streams avoid accidentally coupling policy sampling to
damage rolls. Record seed, initial state, rules/data/policy versions and events.
A phase limit is inconclusive. A phase may request normal actions or forced
replacements, or advance automatic effects without a choice. All requests are
built before exposing any selected action to a policy. The backend validates
supported request sides/formats and applies game-specific ordering; this is not
a universal “one move per side per turn” assumption.

| Ruleset selection | What may be reused | What must be supplied/verified |
|---|---|---|
| `firered-gen3/1` | Existing FRLG simulator/library adapters | Exact supported moves/items/AI and player policy |
| `ruby-gen3/1`, `sapphire-gen3/1` | Compatible Gen 3 mechanics primitives | R/S data, AI, edition differences and parity tests |
| `bdsp/1` | Engine orchestration, compatible library mechanics | BDSP-specific profile/data/AI; not a DP Gen 4 substitution |
| `lgpe/1` | Battle action vocabulary and orchestration | LGPE stats/partner mechanics and its own data/AI |
| `sv/1` | Battle action vocabulary and orchestration | SV mechanics and scripted/phase-based story encounters |

In particular, a backend must not claim a general `items` capability if it only
models Potion: entry validation must reject an unsupported requested item.
A/B/C player strategies are independent of this selection: attacks, recovery,
complex setup/team planning. They still require game-supported legal actions.
NPC AI never becomes our B/C policy. Capture/progression are adjacent adapters,
especially in LGPE; this trainer-battle loop does not stand in for a catching game.

The native counterpart is a battle observation -> policy action -> controller
loop. It shares policy/state/action contracts and parity fixtures with offline
evaluation, not the simulator's hidden truth or speculative damage rolls.
Keep live menu/cursor/target selection and confirmation game-specific.

## 3. Maps: common envelope, required game semantics

Retain today's published `MapData` schema and pins. The shape below illustrates a
future envelope/migration, not a replacement consumers may import immediately.
It uses a **synthetic two-room board**; positions do not describe a real map.

```json
{
  "format": "navigation-map/next",
  "game": "firered",
  "id": "example-room-a",
  "frame": {"id": "runtime-local", "origin": [0, 0]},
  "source": {"pack": "synthetic-example", "revision": "example-only"},
  "tiles": ["#####", "#...#", "#.S.#", "#...#", "##.##"],
  "points": {"start": [2, 1], "exit": [2, 4]},
  "transitions": [
    {
      "id": "south-exit",
      "entry": {"tile": [2, 4], "activation": "walk-south"},
      "arrival": {"map": "example-room-b", "point": "entrance", "facing": "south"},
      "required_capabilities": [],
      "evidence": "synthetic"
    }
  ],
  "extensions": {
    "frlg/movement/1": {"default_elevation": 3, "allow_running": false}
  }
}
```

The common layer owns identity, frames, source pins, dimensions, named points,
objects, directed connections/transitions, conditions and extension validation.
The adapter decides what a press does, what activation means and whether a stop
is a stable anchor. Transition metadata must be resolved into the actual final
arrival, including any automatic walk-out; raw destination tiles are not enough.
Unknown destinations remain forbidden/incomplete, not confirmed arrivals.

Examples of additional required extension payloads (separate examples, not fields
that all maps must have):

| Game | Example payload | Meaning |
|---|---|---|
| BDSP | `"bdsp/world-placement/1": {"matrix": "example-outdoors", "offset": [32, 64]}` | Explicit local/world placement; runtime motion uncertainty remains a policy input |
| Ruby/Sapphire | `"rs/movement/1": {"default_elevation": 3, "bike_mode": "none"}` | R/S adapter validates these values and terrain behavior; not inherited FRLG rules |
| LGPE | `"lgpe/movement/1": {"collision_profile": "example-measured-profile"}` | Requires measured/verified target-game collision and arrivals |

A shared movement-model interface can be small:

```cpp
// A press may yield several possible states, a transition or an explicit failure.
struct MovementPrediction{
    std::vector<LocatedState> possible_after;
    std::optional<TransitionId> transition;
    std::optional<Failure> failure;
};

// Algorithms depend on these semantics; implementations own the game's rules.
class MovementModel{
public:
    virtual ~MovementModel() = default;
    virtual MovementPrediction press(const LocatedState& from, Direction direction) const = 0;
    virtual AnchorAssessment assess_anchor(const PoseBelief& from, Direction direction) const = 0;
    virtual ValidationReport validate(const MapPack& maps) const = 0;
};
```

This is the same dependency direction as existing `plan_legs()` and
`find_wall_anchors()` policy templates; use those existing extension seams rather
than replacing them with virtual calls merely to match this sketch.
`press()` predicts possibilities, while the executor supplies observations.
A blocked press retains its possible pose; an unsupported rule reports failure
instead of returning an empty belief that could be mistaken for completion.
`assess_anchor()` must check every hypothesis for NPCs, triggers, signs, doors,
unsafe edges and required input bounds. Its answer is conditional on static and
dynamic world state; “blocked” alone is insufficient.

SV can share the envelope's identity/provenance and transition/event IDs while
using a different tagged geometry: a landmark graph with traversal constraints,
not a fabricated `tiles` array. It should advertise graph routing and visual
localization capabilities; a grid planner must reject that representation.
Do not force both into a lossy universal collision grid. Crops and world offsets
must be explicit invertible transforms, so a correction cannot masquerade as a
one/two-tile movement error.

## 4. Examples to make executable first

| Example | Input | Required expectation |
|---|---|---|
| Extension dispatch | FRLG cut event passed to BDSP executor | Unsupported during preflight; zero controller actions/saves |
| Requirement uncertainty | Mandatory Potion count unreadable | UNKNOWN; no implicit full bag or successful preparation |
| Partial action | Potion consumed but postcondition not confirmed | Record consumption; reobserve before retrying, do not double-consume blindly |
| Shared runner | Same abstract heal-then-battle plan on two scripted game seams | Same event ordering/guards; different game action calls |
| Ruleset loading | BDSP entry with only an FRLG backend registered | Unsupported; no probability estimate |
| Ruleset legality | Supported backend, unsupported move/item/doubles | Reject the entry, not a fallback move or singles battle |
| Mechanics parity | Same pinned entry, seed and ruleset | Reproducible effect trace; compare supported real mechanics cases separately |
| Forest recovery | Existing one/two-tile shortfall/sign scenario | Sign is not a vertical wall anchor; native repair rejoins at the expected pose |
| BDSP transition | Verified house/mine entry/arrival fixture | Preserve uncertain poses until map/arrival evidence confirms them |
| Geometry mismatch | SV landmark graph supplied to grid search | Explicit unsupported geometry |

Start with documented schemas and native executor seams, then one end-to-end
FRLG/BDSP checkpoint export and independent replay. Keep native registered tests
beside the implementation and Python evaluator tests beside theirs. Full capture,
mechanics and geometry plugins for the other games remain future slices, each
with a real-data and live-controller acceptance gate.
