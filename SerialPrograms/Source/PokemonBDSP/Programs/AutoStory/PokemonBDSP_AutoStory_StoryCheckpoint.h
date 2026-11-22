/*  BDSP AutoStory - Story checkpoint
 *
// code-health: allow header_symbols -- the story table's row type, typed step args, run state and route cost lookup
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A row of BDSP's story table, typed S10 step args, BDSP's own grid pose
 *  state (S2), what the run plays with (the starter, the player's name), and
 *  the plan costs a route step's variant names.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryCheckpoint_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryCheckpoint_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/AutoStory/Grid/Pokemon_GridStoryArgs.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RobustRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Starter Pokemon options for BDSP story automation.
enum class StarterChoice{
    TURTWIG,
    CHIMCHAR,
    PIPLUP,
};

//  What a row does when one of its steps fails.
enum class StoryFailurePolicy{
    //  Reload the row's start save (saved at the end of the row before it, E9)
    //  and run the row again from its first step.
    RELOAD_AND_RETRY,
    //  No save and no retry: the failure ends the program (00.00, before
    //  the game has anything worth saving).
    STOP,
};

//  Typed S10 args for each BDSP story action.
struct PlainStepArgs{};

struct RouteStepArgs{
    Pokemon::GridPose target = {};
    bool fight_wild = false;
    std::string variant = {};
    std::vector<Pokemon::StoryLeg> legs = {};
};

struct LegsStepArgs{
    std::vector<Pokemon::StoryLeg> legs = {};
    bool fight_wild = false;
    std::string variant = {};
};

struct StepMoveArgs{
    Pokemon::Direction direction = Pokemon::Direction::NORTH;
    std::string variant = {};
    std::vector<Pokemon::StoryLeg> legs = {};
};

struct SceneStepArgs{
    bool press_a = false;
};

struct BattleStepArgs{
    bool may_lose = false;
    std::string variant = {};
};

struct AnswerStepArgs{
    bool yes = false;
};

struct TrainStepArgs{
    int64_t battles = 0;
};

struct SaveStepArgs{
    std::string saves_in = {};
};

struct NewGameStepArgs{
    bool skip_if_loaded = false;
};

struct ShopStepArgs{
    std::vector<Pokemon::StoryItem> items = {};
};

using BdspStepArgs = std::variant<
    PlainStepArgs,
    RouteStepArgs,
    LegsStepArgs,
    StepMoveArgs,
    SceneStepArgs,
    BattleStepArgs,
    AnswerStepArgs,
    TrainStepArgs,
    SaveStepArgs,
    NewGameStepArgs,
    ShopStepArgs
>;

//  One parsed step of a BDSP story checkpoint.
struct BdspStep{
    std::string name = {};
    std::string why = {};
    std::string action = {};
    BdspStepArgs args = PlainStepArgs{};
    std::optional<Pokemon::GridPose> pose_after = std::nullopt;
};

//  A row of BDSP's story table: its steps, start pose, and failure/save config.
struct StoryCheckpoint{
    std::string id = {};
    std::string title = {};
    std::optional<Pokemon::GridPose> start = std::nullopt;
    std::vector<BdspStep> steps = {};
    StoryFailurePolicy on_failure = StoryFailurePolicy::RELOAD_AND_RETRY;
    //  The map the save screen's card names where the row starts ("Route
    //  203"). Under E9, the save at the end of row N checks row N+1's
    //  saves_in and stops the program if the card names another map.
    //  Empty for a row that does not save/retry.
    std::string saves_in = {};
    bool dev_only = false;
};

//  BDSP's own run state (S2: no pose in the core engine's RunState).
struct BdspRunState{
    Pokemon::GridPose pose = {};
    bool save_loaded = false;
};

//  Formats a GridPose as "Map (x, y) facing north", or "unknown" when empty.
std::string format_grid_pose(const Pokemon::GridPose& pose);

//  What the story's steps act on.
struct StoryEnv{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    StarterChoice starter = StarterChoice::CHIMCHAR;
    std::string player_name = {};
    //  The new game's language and look.
    IntroPicks intro_picks = {};
    //  Runs Starter RNG at 00.02's starter pick; empty when the option is off.
    std::function<std::optional<Pokemon::StoryFailure>()> run_starter_rng = {};
};


//  The variant of a route step whose target is the next scene's trigger:
//  the scene starts there and stops the walk, as a wall would (00.04's
//  Mom's call on (6, 9)). It plans with the route costs.
inline constexpr std::string_view ROUTE_ONTO_TRIGGER = "trigger";

//  The variant of a route step that, once at its target, runs its one leg
//  ("legs") until a scene's dialogue starts. It plans with the route costs.
inline constexpr std::string_view ROUTE_INTO_SCENE = "into_scene";

//  The plan costs a route step's variant names: "", ROUTE_ONTO_TRIGGER and
//  ROUTE_INTO_SCENE the route costs, "town" town_plan_costs(), "robust" the planner's
//  defaults, "line" waypoint_plan_costs() (a stretch of a line a human
//  walked). Nothing for another name.
std::optional<RobustPlanCosts> story_route_costs(const std::string& variant);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
