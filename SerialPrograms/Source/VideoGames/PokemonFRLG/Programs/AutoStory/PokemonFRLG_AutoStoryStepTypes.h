/*  AutoStory Step Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's parsed story step data, S10 action parsers (used by #648
 *  Pokemon::read_story_checkpoint()), and FRLG-owned grid run state (S2:
 *  pose, gates, flags live in FRLG, not in core Pokemon::RunState).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStepTypes_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStepTypes_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Grid/Pokemon_GridStoryArgs.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Direction.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Grid.h"
#include "Pokemon/Navigation/Grid/Planning/Pokemon_GatedTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Story leg and item types from #653 grid-core.
using Pokemon::StoryLeg;
using Pokemon::StoryItem;

//  Typed value parsed for one FRLG step arg at plan load (S10).
using FrlgArgValue = std::variant<
    std::string,
    int64_t,
    bool,
    Pokemon::Direction,
    Pokemon::GridPose,
    std::vector<StoryLeg>,
    std::vector<StoryItem>,
    std::vector<std::string>,
    std::vector<std::vector<int64_t>>
>;

using FrlgArgMap = std::map<std::string, FrlgArgValue, std::less<>>;

//  One parsed step of an FRLG story checkpoint (S10/S11).
struct StoryStep{
    std::string name = {};
    std::string why = {};
    //  S11 action name ("route", "legs", "step", "scene", "battle", "mart", ...).
    std::string action = {};
    //  Optional movement/talk variant ("run", "bike", "taps", "trainer_line",
    //  "trainer_wait", "wanderer").
    std::string variant = {};
    //  Validated args parsed at plan load time by the action's S10 parser.
    FrlgArgMap args = {};
    std::optional<Pokemon::GridPose> pose_after = std::nullopt;
    std::string skip_if = {};
    std::optional<Pokemon::GateChange> gates_after = std::nullopt;
    std::vector<std::string> flags_after = {};
    bool once_per_save = false;
    std::optional<Pokemon::StoryCheckKind> check = std::nullopt;
};

//  Reads a GateChange object ({"opened": [...], "switches": {"id": bool}}).
Pokemon::GateChange read_gate_change(const JsonValue& value, std::string_view source);

//  Where the player stands and faces on one tile (FRLG table & routes).
struct StoryPose{
    std::string map = {};
    Pokemon::TilePos tile = {};
    std::optional<Pokemon::Direction> facing = std::nullopt;
};

//  FRLG-owned run state (S2: no pose/gates/flags in core Pokemon::RunState).
struct FrlgRunState{
    std::optional<Pokemon::GridPose> pose = std::nullopt;
    Pokemon::GateState gates = {};
    std::set<std::string> flags = {};
    //  Set by a step that succeeded without changing game state (used for
    //  once_per_save skipping on retry).
    bool changed_nothing = false;
};

//  Callback that executes one FRLG step against FRLG run state.
using FrlgStepAction = std::function<std::optional<Pokemon::StoryFailure>(const StoryStep& step, FrlgRunState& state)>;

//  Constructs a StoryFailure with optional detail and repeats flag (S15).
inline Pokemon::StoryFailure frlg_failure(
    std::string code, std::string detail = {}, bool repeats = false
){
    return Pokemon::StoryFailure{
        .code = std::move(code),
        .detail = std::move(detail),
        .repeats = repeats,
    };
}

//  Failure for an action a backend/binding does not implement.
Pokemon::StoryFailure frlg_unsupported_step(const StoryStep& step);

//  Converts between single-tile StoryPose and GridPose.
Pokemon::GridPose to_grid_pose(const StoryPose& pose);
std::optional<StoryPose> story_pose(const std::optional<Pokemon::GridPose>& pose);
std::optional<StoryPose> story_pose(const Pokemon::GridPose& pose);

//  Formats a GridPose for logs: "Map (x, y) facing north", or "unknown".
std::string format_grid_pose(const std::optional<Pokemon::GridPose>& pose);
std::string format_grid_pose(const Pokemon::GridPose& pose);

//  Gate rules derived from FRLG run flags (card_key_held).
Pokemon::GateRules frlg_gate_rules(const FrlgRunState& state);

//  Returns true if `action` is a built-in (non-game-action) step name.
bool is_core_action(std::string_view action);

//  All registered FRLG S11 action names (sorted).
const std::vector<std::string>& frlg_action_names();

//  Builds #648 Pokemon::StoryActions over every FRLG action name. Each parser
//  validates args at plan load time and forwards the parsed StoryStep to
//  `make_run` (or stores it for plan inspection).
Pokemon::StoryActions make_frlg_action_parsers(
    std::function<Pokemon::StoryStepRun(StoryStep)> make_run
);

//  Typed lookup of a required/defaulted arg in `args`. Throws InternalProgramError
//  if missing or wrong type.
template <typename Type>
const Type& frlg_arg(const FrlgArgMap& args, std::string_view key);

template <typename Type>
const Type& frlg_arg(const FrlgArgMap& args, std::string_view key){
    const auto it = args.find(key);
    if (it == args.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Missing FRLG step arg: " + std::string(key));
    }
    const Type* value = std::get_if<Type>(&it->second);
    if (value == nullptr){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Wrong type for FRLG step arg: " + std::string(key));
    }
    return *value;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
