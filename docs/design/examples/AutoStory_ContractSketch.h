/* Documentation sketch: proposed value types, not an installed runtime API.
 * C++23 syntax check:
 *   clang++ -std=c++23 -x c++ -fsyntax-only docs/design/examples/AutoStory_ContractSketch.h
 * Production would reuse existing direction/pose/domain types and split game
 * types into their own headers. No game actions or simulation are implemented.
 */
#ifndef PokemonAutomation_AutoStory_ContractSketch_H
#define PokemonAutomation_AutoStory_ContractSketch_H
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace AutoStorySketch{

// Direction/facing vocabulary; reuse Pokemon::Direction in production.
enum class Direction{ NORTH, SOUTH, WEST, EAST };
// An exact tile in an explicitly named coordinate frame.
struct TileLocation{
    std::string map;
    std::string frame;
    int x = 0;
    int y = 0;
    std::optional<Direction> facing;
    std::optional<int> elevation;
};
// A game-local landmark, without pretending that it is a known grid tile.
struct LandmarkLocation{
    std::string region;
    std::string landmark;
    std::optional<double> heading_radians;
};
// Spatial predicates/actions can use either supported location representation.
using Destination = std::variant<TileLocation, LandmarkLocation>;
// Movement state additionally includes a game-validated traversal-mode ID.
struct LocatedState{ Destination location; std::string traversal_mode; };
// Possible positions; an empty set means no trusted localization, never arrival.
using PoseBelief = std::vector<LocatedState>;
// Validated input artifact, pinned by immutable revision AND content digest.
struct ArtifactPin{ std::string id; std::string revision; std::string sha256; };
// Inputs required by this plan; absent packs are legal only if unused.
struct DataPins{
    std::optional<ArtifactPin> maps;
    std::optional<ArtifactPin> reference_data;
    std::optional<ArtifactPin> battle_ruleset;
};
// Target edition/build/language and its registered execution adapter.
struct GameProfile{ std::string id; std::string version; std::string language; std::string adapter; };
// Executable policy identity; its validated parameters are selected by this pin.
struct PolicyRef{ std::string id; ArtifactPin definition; };
// Software identity of the save snapshot, not an assumed ROM-readable save ID.
struct SaveIdentity{ std::string run; std::uint64_t generation = 0; };
// Inclusive integer bounds; min <= max; equal endpoints represent an exact value.
struct Bounds{ std::int64_t min = 0; std::int64_t max = 0; };

// Inventory/money/stat predicates compare against a bound, not a guessed value.
struct ItemAtLeast{ std::string item; std::int64_t quantity = 0; };
// Current HP threshold for a stable party-member ID, not a slot/species identity.
struct HpAtLeast{ std::string member; std::int64_t hp = 0; };
// An attack's available PP, checked independently of knowing the move.
struct PpAtLeast{ std::string member; std::string move; std::int64_t pp = 0; };
// The named member knows this move.
struct MoveKnown{ std::string member; std::string move; };
// Minimum member level.
struct LevelAtLeast{ std::string member; int level = 1; };
// Exact named game/story flag value.
struct FlagEquals{ std::string flag; bool value = false; };
// A registered game adapter establishes the unlock/move/badge requirements.
struct CapabilityAvailable{ std::string capability; };
// The pose belief must satisfy the destination constraints, not merely intersect.
struct LocationWithin{ Destination destination; };
// Required semantic screen, e.g. "overworld" or "battle/move-menu".
struct ScreenIs{ std::string screen; };
// Confirmed facing toward an identified object in the current world state.
struct FacingObject{ std::string object; };
// A world object has been removed, not merely absent from one camera frame.
struct ObjectAbsent{ std::string object; };
// Money available before purchasing; unknown money does not satisfy it.
struct MoneyAtLeast{ std::int64_t amount = 0; };
// Closed, typed initial vocabulary; add a typed extension parameter if needed.
using Predicate = std::variant<ItemAtLeast, HpAtLeast, PpAtLeast, MoveKnown,
    LevelAtLeast, FlagEquals, CapabilityAvailable, LocationWithin, ScreenIs,
    FacingObject, ObjectAbsent, MoneyAtLeast>;
// Evidence origin distinguishes console observations from offline assumptions.
enum class EvidenceOrigin{ OBSERVED, DERIVED, SAMPLED, ASSUMED };
// A predicate plus an explicit acceptance policy; hard requirements are conjunctive.
struct Requirement{
    std::string id;
    Predicate predicate;
    std::vector<EvidenceOrigin> accepted_sources = {EvidenceOrigin::OBSERVED};
};
// A check can be unreadable/ambiguous without being known false.
enum class CheckResult{ SATISFIED, VIOLATED, UNKNOWN };
// Failure classification; event recovery policy, not this enum, owns retry decisions.
enum class FailureCode{
    INVALID_PLAN, PIN_MISMATCH, UNSUPPORTED, PRECONDITION, POSTCONDITION,
    TIMEOUT, LOCALIZATION, EXHAUSTED_BUDGET, EXECUTION
};
// Structured failure with a namespaced reason and source event for diagnostics.
struct Failure{
    FailureCode code = FailureCode::EXECUTION;
    std::string reason;
    std::string message;
    std::optional<std::string> event_id;
    std::vector<std::string> evidence_ids;
};
// A codec returns a typed value or a structured validation/unsupported failure.
template <typename T>
using DecodeResult = std::variant<T, Failure>;
// Producer/run/attempt/sequence identify freshness; reference identifies frame/trace.
struct EvidenceSource{
    EvidenceOrigin origin = EvidenceOrigin::OBSERVED;
    std::string producer;
    std::string run;
    std::string attempt;
    std::uint64_t sequence = 0;
    std::string reference;
    std::optional<SaveIdentity> save;
};
// An inventory reading, with exact or bounded quantity.
struct ItemCount{ std::string item; Bounds quantity; };
// Current and maximum HP reading.
struct MemberHp{ std::string member; Bounds current; Bounds maximum; };
// Available PP reading.
struct MovePp{ std::string member; std::string move; Bounds current; };
// Known or absent move, based on a sufficient party summary read.
struct MemberMove{ std::string member; std::string move; bool known = false; };
// Member level reading.
struct MemberLevel{ std::string member; Bounds level; };
// Confirmed flag state.
struct FlagValue{ std::string flag; bool value = false; };
// Established game capability state.
struct CapabilityValue{ std::string capability; bool available = false; };
// Spatial observation can retain several positions and unknown facing.
struct PoseReading{ PoseBelief possible; };
// Semantic screen observation.
struct ScreenReading{ std::string screen; };
// Object orientation/visibility is expressed as a semantic reading, not raw pixels.
struct ObjectReading{ std::string object; bool present = false; bool facing = false; };
// Money reading.
struct MoneyReading{ Bounds amount; };
// Validated facts suitable for the shared ledger's current vocabulary.
using Fact = std::variant<ItemCount, MemberHp, MovePp, MemberMove, MemberLevel,
    FlagValue, CapabilityValue, PoseReading, ScreenReading, ObjectReading, MoneyReading>;
// Unreadable or unavailable field; e.g. "bag/potion", "quantity OCR failed".
struct UnavailableReading{ std::string field; std::string reason; };
// One reading with provenance; it is not a command or a promised effect.
struct Observation{
    std::string id;
    std::variant<Fact, UnavailableReading> reading;
    EvidenceSource source;
};
// Bounded resource delta; [-1,0] means consumption may have occurred.
struct InventoryDelta{ std::string item; Bounds change; };
// Bounded money delta.
struct MoneyDelta{ Bounds change; };
// Bounded EXP award to a stable party-member ID.
struct ExperienceDelta{ std::string member; Bounds change; };
// HP transition; a Potion is not represented as an unconditional full rest.
struct HpChange{ std::string member; Bounds before; Bounds after; };
// PP transition for the named move.
struct PpChange{ std::string member; std::string move; Bounds before; Bounds after; };
// Learned move, with the actual replacement when one occurred.
struct MoveLearned{ std::string member; std::string move; std::optional<std::string> replaced; };
// Established story/world flag change.
struct FlagChanged{ std::string flag; bool value = false; };
// Position change/uncertainty; empty belief explicitly loses localization.
struct PoseChanged{ PoseBelief possible_after; };
// Initial effect vocabulary; additional party/status/evolution effects need schemas.
using Effect = std::variant<InventoryDelta, MoneyDelta, ExperienceDelta, HpChange,
    PpChange, MoveLearned, FlagChanged, PoseChanged>;
// Evidence supporting an actual/possible effect; observations may support several.
struct EffectEvidence{
    std::string id;
    Effect effect;
    EvidenceSource source;
    std::vector<std::string> supporting_observation_ids;
};
// Evidence is assessed, not converted to a bool with unknown == success.
struct RequirementCheck{
    std::string requirement_id;
    CheckResult result = CheckResult::UNKNOWN;
    std::vector<std::string> evidence_ids;
};
// A check report is assessed using current state, source policy and freshness.
struct CheckReport{ std::vector<RequirementCheck> checks; std::vector<Observation> observations; };
// Empty issues means preflight accepted; it does not mean gameplay succeeded.
struct ValidationReport{ std::vector<Failure> issues; };
// COMPLETED is the action report, not the event's postcondition verdict.
enum class ActionStatus{ COMPLETED, INTERRUPTED, FAILED, UNSUPPORTED, INCOMPLETE };
// Report partial effects even when the action fails or cannot establish completion.
struct ActionReport{
    ActionStatus status = ActionStatus::INCOMPLETE;
    std::vector<Observation> observations;
    std::vector<EffectEvidence> effects;
    std::optional<Failure> failure;
};
// Controller queue state is independent of screen/position certainty.
enum class QueueState{ DRAINED, PENDING, UNKNOWN };
// The reconciled current facts; this is a reduced ledger, not the entire event log.
struct GameState{ std::vector<Observation> current_facts; PoseBelief pose; };
// Snapshot restored by an explicit reload; clear stale observations/caches.
struct SaveSnapshot{ SaveIdentity id; GameState game; };
// Owned by the runner; executors receive a const view and return evidence.
struct StoryState{
    GameState game;
    QueueState queue = QueueState::UNKNOWN;
    std::optional<SaveSnapshot> save;
};
// Recovery actions are bounded, explicit and game-supported.
enum class RecoveryKind{ REOBSERVE, RELOCALIZE, RELOAD, STOP };
// Attempts includes the initial action; 1 means no action retry.
struct RecoveryPolicy{
    std::size_t max_attempts = 1;
    std::vector<RecoveryKind> escalation = {RecoveryKind::STOP};
};
// A travel goal and pinned route policy; no assumed arrival is stored here.
struct TravelAction{ Destination destination; PolicyRef route_policy; };
// Trainer identity and executable/versioned player policy, not merely A/B/C.
struct TrainerBattleAction{ std::string trainer; PolicyRef player_policy; };
// Heal at an already-established service; travel is a separate action.
struct HealAction{ std::string service; std::vector<std::string> members; };
// Quantity in a shop request or resource budget.
struct ItemAmount{ std::string item; std::int64_t quantity = 0; };
// Buy exactly the required request or report unmet/partial preparation.
struct ShopAction{ std::string shop; std::vector<ItemAmount> items; std::int64_t budget = 0; };
// Capture is an objective with its own bounded game policy.
struct CaptureAction{ std::string species; std::string member_id; PolicyRef policy; };
// Teach a move to an identified member, with an explicit replacement choice.
struct LearnMoveAction{ std::string member; std::string move; std::optional<std::string> replace; };
// GameSpecificAction may itself be a variant; this nested visitor needs no flattening.
template <typename GameSpecificAction>
using Action = std::variant<TravelAction, TrainerBattleAction, HealAction,
    ShopAction, CaptureAction, LearnMoveAction, GameSpecificAction>;
// Typed event data, independent of its storage codec and executor.
template <typename GameSpecificAction>
struct StoryEventData{
    std::string id;
    std::vector<Requirement> preconditions;
    Action<GameSpecificAction> action;
    std::vector<Requirement> postconditions;
    RecoveryPolicy recovery;
};
// Each game chooses a save placement policy; sharing must preserve that choice.
enum class SavePolicy{ NONE, BEFORE_FIRST_ATTEMPT, AFTER_SUCCESS };
// A typed checkpoint with explicit start/end guards and total attempt budget.
template <typename GameSpecificAction>
struct CheckpointData{
    std::string id;
    std::vector<Requirement> start;
    std::vector<StoryEventData<GameSpecificAction>> events;
    std::vector<Requirement> end;
    SavePolicy save_policy = SavePolicy::NONE;
    std::size_t max_attempts = 1;
};
// Producer identity pins the plan independently of the datasets it consumes.
struct PlanSource{ std::string id; std::string revision; };
// Selected typed plan; unknown/missing pins are rejected for operations needing them.
template <typename GameSpecificAction>
struct StoryPlanData{
    PlanSource source;
    GameProfile game;
    DataPins pins;
    std::vector<CheckpointData<GameSpecificAction>> checkpoints;
};
// Opaque validated map-pack handle; game adapters own decoded map structures.
struct MapPack{ ArtifactPin pin; std::vector<std::string> map_ids; };
// Transition identity within a pinned map pack.
using TransitionId = std::string;
// Prediction is separate from an observed/confirmed map transition.
struct MovementPrediction{
    std::vector<LocatedState> possible_after;
    std::optional<TransitionId> transition;
    std::optional<Failure> failure;
};
// Safe only when all candidates converge within the input bound, with hazards checked.
struct AnchorAssessment{
    CheckResult safe = CheckResult::UNKNOWN;
    std::optional<LocatedState> arrival;
    std::size_t max_inputs = 0;
    std::vector<Failure> issues;
};

namespace Frlg{
// These concrete actions live in the FRLG module in production.
struct CutObstacle{ std::string object; Direction direction = Direction::NORTH; };
// Trainer approach differs from the subsequent common TrainerBattleAction.
struct ApproachTrainer{ std::string trainer; };
// FRLG's typed action extension.
using GameSpecificAction = std::variant<CutObstacle, ApproachTrainer>;
} // namespace Frlg
namespace Bdsp{
// Example BDSP-specific hidden-move interaction.
struct HiddenMove{ std::string capability; std::string target; };
// BDSP's typed action extension; more concrete alternatives can be added.
using GameSpecificAction = std::variant<HiddenMove>;
} // namespace Bdsp
} // namespace AutoStorySketch
#endif
