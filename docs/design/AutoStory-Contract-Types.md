# Typed actions, parsing and shared report types

This refines [the interface examples](AutoStory-Interface-Examples.md). The
[standalone C++23 type sketch](examples/AutoStory_ContractSketch.h) defines every
supporting story/map value type used there, including action parameters,
requirements, pins, reports, state, checkpoints and movement assessments. It is
documentation scaffolding, outside the application build; it implements no game
behavior. Compile it with the command in its module comment.

## 1. Recommended polymorphism: typed data, a narrow binder, shared runner

Yes: `GameSpecificAction` can itself be a variant:

```cpp
using FrlgActions = std::variant<Frlg::CutObstacle, Frlg::ApproachTrainer>;
using FrlgAction = Action<FrlgActions>;
using FrlgPlan = StoryPlanData<FrlgActions>;

// A common action is a direct alternative; a game action is the nested variant.
FrlgAction heal = HealAction{"nurse", {"lead-1"}};
FrlgAction cut = FrlgActions{Frlg::CutObstacle{"example-tree", Direction::NORTH}};
```

`Action<T>` is a variant of the common actions plus `T`. A visitor handles the
outer variant, and a game-local visitor handles the inner variant. There is no
need to flatten variants or give every action a virtual base class. The concrete
FRLG variant cannot be inserted into `Action<Bdsp::GameSpecificAction>`.

This is compile-time polymorphism of the **data and binding**, not a requirement
to template the entire execution engine. Prefer this combination:

| Layer | Choice | Why |
|---|---|---|
| JSON storage | Versioned tagged records | Stable language-independent exchange format |
| Parsed game plan | `StoryPlanData<GameSpecificAction>` | Typed action parameters; no JSON payload in the execution model |
| Game executor | Concrete per-game overloads | Common operations and game actions call that game's routines |
| Binding to events | Small template visitor | Converts typed actions/checks into runner callables |
| Event/checkpoint/retry engine | Existing non-template runner | Game-independent ordering, reconciliation, recovery and saving |
| Program selection | Runtime factory/registered callable | User can select a game without erasing action types throughout the code |
| Battle backend | Runtime protocol/interface | The selected ruleset chooses a mechanics implementation |

For example, an action binder can erase only the execution callable's type:

```cpp
// Sketch: include the real Common/Cpp/MoveOnlyFunction.h in production.
using BoundAction = MoveOnlyFunction<ActionReport(const StoryState&)>;

template <typename T, typename Executor>
BoundAction bind_action(const Action<T>& action, Executor& executor){
    return [&action, &executor](const StoryState& state){
        return std::visit([&](const auto& value){
            return executor.execute(value, state);
        }, action);
    };
}
```

The FRLG executor supplies common-action overloads and an overload for its
`FrlgActions`. That overload visits the concrete game alternatives. Missing
required overloads are compile errors, not silent default implementations. If a
known action is unavailable in the selected profile, report UNSUPPORTED during
preflight. A full adapter should use constrained overloads/a concept to give a
clear diagnostic; an unconstrained catch-all must not accept every action.

The binder and its captured references must not outlive the immutable plan or
executor. A run owns both for its entire lifetime. A report adapter reconciles
`ActionReport` into the proposed richer story state and the existing runner's
`Outcome`; this migration is still needed, because current `RunState` has only
pose/flags. Keep descriptors for export alongside the bound callables; lambdas
are not a serializable plan. This is not a second independent retry loop.

A templated `Engine<T, Executor>` is also reasonable for a small prototype, but
is unnecessary if its loop only uses checks, actions and outcomes. It would
instantiate orchestration code for every game without adding useful type safety
inside that loop. Conversely, a universal virtual `GameExecutor` is useful if
executors really must be interchangeable behind one interface, but its arguments
must then be normalized/type-erased, or the interface must itself be specialized
per `T`. C++ cannot supply a virtual function template. Do not erase actions into
`void*`, `std::any` or JSON merely to make that interface universal.

## 2. JSON is a separate storage model

```text
JSON: type=game, schema=frlg/cut-obstacle/1, payload={...}
    -> FRLG codec: schema/version/game checks, required fields, value validation
    -> Frlg::CutObstacle
    -> Action<FrlgActions>
    -> bind to FrlgExecutor -> shared non-template event runner
```

A codec's boundary can be declared as:

```cpp
// JsonValue belongs to the storage/codec module, not the typed action header.
DecodeResult<FrlgPlan> decode_frlg_plan(const JsonValue& document);
```

The decoder uses the plan's registered game profile, then dispatches on schema
name **once at the boundary**. Execution dispatches on the C++ variant alternative.
A matching encoder exports the selected typed plan without serializing lambdas.
Reject unknown fields/versions in semantic payloads unless an explicit migration
handles them. Optional annotations are a separate metadata section.

There is no `JsonValue payload` in the typed action model. Raw JSON can remain in
the codec/diagnostic layer. The same separation applies to required map extensions
and battle manifests: stored names select typed adapters, not arbitrary code.
The JSON examples remain illustrative fragments, not importable fixtures.

## 3. What DataPins means

`DataPins` identifies the exact **input datasets** used to interpret a plan:

- `maps`: published map manifest/content, including verified game corrections.
- `reference_data`: trainer/species/move/item/progression data used by the plan.
- `battle_ruleset`: manifest selecting the mechanics profile, implementation and
  NPC AI/data dependencies for battle evaluation.

Each `ArtifactPin` has an artifact ID, immutable revision and SHA256 of the
artifact/manifest bytes. A manifest's referenced files must also be verified.
A moving branch name or a cached dataset with the same friendly name is not a
pin. Optional fields are allowed only when the plan does not use that input;
validation rejects a battle needing a missing ruleset or reference-data pack.

`PlanSource` separately identifies the producer plan/revision; `PolicyRef` pins
an executable policy definition, not just an A/B/C label. Build/backend code
identity belongs in the run's implementation provenance and in the ruleset's
resolved implementation identity. A dataset digest alone does not prove which
binary ran. `SaveIdentity` identifies the runtime snapshot a reload restores;
it is not a data-pack revision, and it must not be fabricated for an unknown save.

## 4. Requirement is a typed predicate plus evidence policy

The header defines `Predicate` alternatives such as `ItemAtLeast`, `MoveKnown`,
`HpAtLeast`, `PpAtLeast`, `LevelAtLeast`, `CapabilityAvailable`, `LocationWithin`,
`ScreenIs`, `FlagEquals`, `FacingObject` and `ObjectAbsent`. `Requirement` gives
that predicate a stable ID and specifies which evidence origins may establish it.
A vector of required predicates means AND; explicit AnyOf/Not/exact game-predicate
extensions can be added later when a real plan needs them. Do not encode a
predicate as an arbitrary expression string.

```cpp
Requirement bag_ready{
    .id = "two-potions",
    .predicate = ItemAtLeast{"potion", 2},
    .accepted_sources = {EvidenceOrigin::OBSERVED},
};
Requirement knows_cut{
    .id = "supporter-knows-cut",
    .predicate = MoveKnown{"support-1", "cut"},
    .accepted_sources = {EvidenceOrigin::OBSERVED},
};
```

For a quantity requirement `>= 2`, an observed interval `[2,4]` satisfies it,
`[0,1]` violates it, and `[1,3]` is UNKNOWN. An unreadable/missing count is UNKNOWN,
not zero or two. A requirement evaluator checks the fact type, accepted origin,
freshness, game and save identity before comparing values. No satisfying fact
means UNKNOWN; it does not imply the opposite fact.

A pose requirement must hold for all current candidate poses. Intersection with
the goal is insufficient; unknown facing cannot satisfy a specified facing.
A capability predicate is game-adapter logic backed by the relevant evidence;
knowing Cut alone may not prove authorization to use it in the field.

Preconditions and postconditions use the same structure. Hard UNKNOWN checks
stop or invoke explicitly permitted observation/preparation, not the guarded
operation. Soft preferences belong in planning objectives, not weakened hard
requirements. Replay may accept DERIVED/SAMPLED evidence under an explicit
replay profile; that evidence cannot satisfy an OBSERVED-only live check.

## 5. Observation, effects and failure are different records

| Type | Defined fields | Meaning |
|---|---|---|
| `Observation` | ID, typed `Fact` or `UnavailableReading`, `EvidenceSource` | A reading of state, including an unreadable field |
| `EffectEvidence` | ID, typed `Effect`, source, supporting observation IDs | Evidence of an actual or possible state change |
| `EvidenceSource` | Origin, producer, run, attempt, sequence, reference, optional save identity | Traceability and freshness; frame/trace IDs are references, not trusted poses |
| `ActionReport` | Status, observations, effects, optional failure | What one action established, even when interrupted or failed |
| `Failure` | Code, namespaced reason, message, optional event ID, evidence IDs | Why execution/evaluation could not proceed; no automatic retry authority |
| `CheckReport` | Requirement IDs with three-valued results/evidence IDs, fresh observations | The result of evaluating predicates |
| `ValidationReport` | Structured issues | Preflight acceptance/errors; acceptance is not gameplay success |
| `StoryState` | Reconciled game facts/pose, queue state, optional save snapshot | Current knowledge owned by the runner; no mutation through executor inputs |

A Potion example: `Observation(ItemCount("potion", [1,1]))` reads the remaining
stock. `EffectEvidence(InventoryDelta("potion", [-1,-1]))` records consumption
relative to a known prior state. These describe the same event in different
ways; reconciliation must not subtract twice. Effects/observations need stable,
run-unique IDs and a consistent causal order. Conflicts retain uncertainty or
fail; they do not select the most favorable reading.

When consumption may have occurred, `InventoryDelta` can be `[-1,0]`; a subsequent
reading can resolve it. The effect does not become confirmed merely because an
item-use command was sent. HP/PP changes record before/after bounds. A failed
postcondition still retains effects already evidenced. Derived/sampled simulation
effects use their proper provenance and cannot masquerade as camera observations.
`GameState.pose` is the reconciled spatial view of those facts, not a second
independently writable source of truth. Full event history is kept separately.

| `ActionStatus` | Contract |
|---|---|
| COMPLETED | Adapter completed the semantic action; runner still checks event postconditions |
| INTERRUPTED | An encounter/scene/other interruption occurred; effects and pose may be partial |
| FAILED | A supported action failed; structured `Failure` required |
| UNSUPPORTED | Missing game/profile/feature implementation; structured failure required; not a battle loss |
| INCOMPLETE | Completion cannot be established, e.g. ambiguous/unreadable evidence; structured explanation required |

COMPLETED has no failure. INTERRUPTED supplies a structured interruption reason;
FAILED/UNSUPPORTED/INCOMPLETE require failure records. The header is a value-type
sketch; factories/validators must enforce those cross-field invariants in the
implementation. Recovery decisions come from event/checkpoint policy and the
current evidence. A message string alone never grants an automatic reload/retry.

`RecoveryPolicy.max_attempts` and checkpoint `max_attempts` include the initial
attempt. One means no action/checkpoint retry. Separate shared interruption and
input budgets still need enforcement; entering a helper must not replenish them.
Queue PENDING/UNKNOWN forbids acting as if all movement inputs have drained.
Reload restores the identified snapshot while invalidating stale evidence and
preparation caches, except explicitly proven read-only facts for that same save.

## 6. Remaining map and battle boundary names

The header also defines `Destination`, `LocatedState`, `PoseBelief`, `MapPack`,
`TransitionId`, `MovementPrediction` and `AnchorAssessment`. `MapPack` is an opaque
validated handle here; actual grid/graph structures remain in the map adapters.
An anchor assessment specifies a safe/unsafe/unknown verdict, predicted arrival,
input cap and issues. Safe requires one arrival from all candidates, not merely
a blocked step. Prediction is not arrival confirmation.

For the Python battle sketch, these are the minimum domain stubs rather than
unexplained names. They deliberately leave full game mechanics inside a backend:

| Name | Minimal fields/operations |
|---|---|
| `BattleAction` | Tagged move/target, switch/member, item/target or other explicitly supported command |
| `BattleState` | Ruleset identity and backend-owned full state; never sent directly to player policy |
| `PolicyView` | Validated permitted observations and uncertainty for this side; no hidden simulator state |
| `DecisionRequest` | Unique ID, side, phase, `PolicyView`, legal `BattleAction` list |
| `PhaseResult` | Next `BattleState` and ordered semantic events/effects |
| `BattleOutcome` | Player win, loss, draw, escape or other supported terminal outcome; not an unsupported/phase-limit result |
| `NpcPolicy` | `choose(view, legal_actions, rng)` returning one legal action, using game-specific information access |
| Player policy | Same choice protocol, separate version and permitted observations |
| RNG bundle | Independent recorded streams for player policy, NPC policy and mechanics |
| Evaluation result | Completed outcome or typed unsupported/incomplete/budget failure, final state and trace |

The existing FRLG battle schema is the starting implementation input, not a
license to call every game's state a FRLG state. Fully typing the common battle
schema and validating its game extensions is a separate implementation slice;
these table stubs do not implement or certify any battle backend.
