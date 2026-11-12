/*  BDSP AutoStory - Story checkpoint
 *
// code-health: allow header_symbols -- the story table's row type, the run's options and the segment its rows make
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A row of BDSP's story table, what the run plays with (the starter, the
 *  player's name), the plan costs a ROUTE step's variant names, and the
 *  segment the rows make for the shared runner (Pokemon_StorySegments.h).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryCheckpoint_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryCheckpoint_H

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
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
enum class StoryFailure{
    //  Save before the first step; on a failure reset to that save and run
    //  the row again from its first step.
    RELOAD_AND_RETRY,
    //  No save and no retry: the failure ends the program (00.00, before
    //  the game has anything worth saving).
    STOP,
};

//  A row of the story table: the shared checkpoint, and what a failure does.
struct StoryCheckpoint : public Pokemon::StoryCheckpoint{
    StoryFailure on_failure = StoryFailure::RELOAD_AND_RETRY;
    //  The map the save screen's card names where the row starts ("Route
    //  203"). The save at its start is refused anywhere else and the program
    //  stops: r76/r77 started 01.06 a walk away from its start and saved over
    //  the good save there. Empty for a row that does not save first.
    std::string saves_in;
};

//  What the story's steps act on.
struct StoryEnv{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    StarterChoice starter = StarterChoice::CHIMCHAR;
    std::string player_name;
};


//  The variant of a ROUTE step whose target is the next scene's trigger:
//  the scene starts there and stops the walk, as a wall would (00.04's
//  Mom's call on (6, 9)). It plans with the route costs.
inline constexpr std::string_view ROUTE_ONTO_TRIGGER = "trigger";

//  The variant of a ROUTE step that, once at its target, runs its one leg
//  ("legs") until a scene's dialogue starts. It plans with the route costs.
inline constexpr std::string_view ROUTE_INTO_SCENE = "into_scene";

//  The plan costs a ROUTE step's variant names: "", ROUTE_ONTO_TRIGGER and
//  ROUTE_INTO_SCENE the route costs, "town" town_plan_costs(), "robust" the planner's
//  defaults, "line" waypoint_plan_costs() (a stretch of a line a human
//  walked). Nothing for another name.
std::optional<RobustPlanCosts> story_route_costs(const std::string& variant);


//  The arg of a row's start save (a SAVE step) that names the map its card
//  must show: the row's saves_in.
inline constexpr std::string_view SAVES_IN_ARG = "saves_in";

//  `rows` (kept alive by the caller) as one segment. A row that reloads on
//  failure saves first, unless `saved` says it has in this run: a retry
//  starts from that save. `executor` and `saved` must outlive the segment.
Pokemon::Segment make_story_segment(
    const std::vector<StoryCheckpoint>& rows, Pokemon::StoryStepExecutor& executor,
    std::set<std::string>& saved
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
