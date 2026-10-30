/*  BDSP AutoStory - Route 202 to Sandgem's Pokemon Center, as events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A test checkpoint, and the first written with the story event model
 *  (Pokemon/Navigation/Pokemon_StoryEvents.h): from Route 202's save spot
 *  under the south ledge, south into Sandgem Town and in through the
 *  Pokemon Center's door. It neither heals nor saves, so the save stays on
 *  Route 202 and the run can be repeated.
 *
 *  The game steps are behind SandgemCenterActions, so the events and their
 *  recoveries can be run against a scripted fake.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_SandgemCenterEvents_H
#define PokemonAutomation_PokemonBDSP_AutoStory_SandgemCenterEvents_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Navigation/Pokemon_StoryEvents.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PokemonCenter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The game steps the checkpoint is made of.
class SandgemCenterActions{
public:
    virtual ~SandgemCenterActions() = default;

    //  The map named on the save screen's card, as read (see
    //  read_location_from_save_screen()); back on the overworld after.
    virtual std::string save_screen_location() = 0;

    //  Walks the story maps from any of `at` until every tile we may be on
    //  is `goal`, and sets `at` to those tiles. A walk onto another map must
    //  see its location banner on the way (walk_route()).
    virtual Pokemon::Outcome walk(
        std::vector<Pokemon::TilePos>& at,
        const std::function<bool(const Pokemon::TilePos&)>& goal,
        const std::string& label
    ) = 0;

    //  East until the trees stop us, then south until a wall does. Returns
    //  whether Sandgem Town's location banner showed on the way.
    virtual bool east_then_south_into_sandgem() = 0;

    //  The lit doorway nearest the middle of the screen, if any.
    virtual std::optional<LitDoorway> lit_doorway() = 0;

    //  Lines up with the lit doorway on screen and walks in (enter_lit_door()).
    virtual Pokemon::Outcome enter_lit_door() = 0;

    //  Whether the screen is not black (the door's fade has finished).
    virtual bool screen_shows() = 0;

    virtual bool dialogue_showing() = 0;
    virtual Pokemon::Outcome finish_dialogue() = 0;
};


//  Where the walk south stops in Sandgem Town, in world tiles of
//  story_maps(): Route 202's column 30 runs into Sandgem's column 29, and
//  walking south from Route 202 stops at (29,2), above the Poke Mart.
Pokemon::TilePos sandgem_entry_stop();

//  Below the Center's sign, three tiles east of its door: where 00.04's
//  heal walks to (r75), with the doorway in sight.
Pokemon::TilePos below_sandgem_center_sign();

//  Whether `doorway` is the Center's, seen from the row below its door: up
//  the screen by about a row, and no further across than the Center's sign.
//  The Mart's doorway, seven tiles east of the sign, is too far off the
//  middle to be found from there (find_lit_doorway()).
bool center_doorway_at_front_row(const LitDoorway& doorway);

//  Whether we are inside: the fade is over and no doorway shows straight
//  ahead at the front row, as it does to someone the door did not take in.
bool inside_past_door(bool screen_shows, const std::optional<LitDoorway>& doorway);


//  The checkpoint's events, run against `game`, which must outlive it.
//  Events:
//      1. confirm_on_route_202     the save screen reads Route 202
//      2. walk_to_sandgem          the pose is on Sandgem Town
//      3. walk_to_center_door      the Center's doorway at the front row
//      4. enter_center             the fade is done and the inside shows
Pokemon::Checkpoint route202_to_sandgem_center_checkpoint(SandgemCenterActions& game);

extern const char* const ROUTE202_TO_SANDGEM_CENTER_NAME;

//  DIAGNOSTIC ONLY: Remove before pushing to production.
//  Runs the checkpoint on the console, from Route 202's save spot. Fails
//  with a screenshot, naming the event and why, if it does not complete.
void diagnostic_route202_to_sandgem_center(SingleSwitchProgramEnvironment& env, ProControllerContext& context);



}
}
}
#endif
