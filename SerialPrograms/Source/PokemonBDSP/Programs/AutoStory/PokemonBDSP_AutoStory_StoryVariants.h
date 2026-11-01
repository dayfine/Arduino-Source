/*  BDSP AutoStory - Story step variants
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The ways BDSP's executor does a step kind, as types. The shared steps
 *  carry a variant as text (StoryStep::variant); the table writes it from
 *  these with variant_name() and the executor reads it back with
 *  parse_variant(), so a misspelt way fails the table's lint, not a run.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryVariants_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryVariants_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "PokemonBDSP_AutoStory_Battle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  ROUTE: the plan's costs (story_route_costs()).
enum class RouteCosts{
    ROUTE,      //  route_plan_costs()
    TOWN,       //  town_plan_costs()
    ROBUST,     //  the planner's defaults
};
//  LEGS: how each leg is walked.
enum class LegsWay{
    DPAD,               //  counted D-pad taps; a leg that moves less fails
    DPAD_ONE_SHORT_OK,  //  the same, a tile short allowed (a door lined up by sight after)
    TO_WALL,            //  walk until something stops it, for at most its tiles' time
    HOLD,               //  one stick hold of the leg's tiles' time
    HOLD_TAPS,          //  a stick hold of one tile's time per tile
};
//  MASH: which button goes through the lines.
enum class MashWay{
    B,          //  B through a dialogue that opens (or has)
    A,          //  A through it, for lines that give an item
    WAIT_OUT,   //  MASH_WAIT_OUT_PRESSES B presses on a clock, then the overworld must show
};
//  STEP: how the step is taken.
enum class StepWay{
    DPAD,       //  one D-pad tap the step's way, turning first
    OUT_SOUTH,  //  out of a building's door south (leave_door_south())
};
//  TALK: how the talk starts.
enum class TalkWay{
    AHEAD,      //  A at what we face
    WALKING,    //  walk the step's leg until someone stops us and talks
};

//  WAIT_OUT's B presses, 1.5 s apart: the 12 s the president takes to walk
//  off after the Poketch (00.05c, on the console 2026-10-30).
inline constexpr size_t MASH_WAIT_OUT_PRESSES = 8;

template <typename Way>
const char* variant_name(Way way);
//  The way `name` names; none for an unknown name. "" is the first way.
template <typename Way>
std::optional<Way> parse_variant(const std::string& name);
//  Every way of `Way`, in order.
template <typename Way>
std::vector<Way> all_variants();

//  Whether `variant` is a way BDSP's executor does `kind` itself, besides
//  the story's events.
bool built_in_variant(Pokemon::StoryStepKind kind, const std::string& variant);


}
}
}
#endif
