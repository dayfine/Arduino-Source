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


//  What a talk showed: whether a dialogue opened, whether it had closed
//  after, and what OCR read in its first box.
struct DialogueRead{
    bool opened = false;
    bool closed = false;
    std::string text;
};

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

    //  walk() with town_plan_costs(): short open legs allowed, so the plan
    //  can step a tile across and go straight through a gap.
    virtual Pokemon::Outcome walk_in_town(
        std::vector<Pokemon::TilePos>& at,
        const std::function<bool(const Pokemon::TilePos&)>& goal,
        const std::string& label
    ) = 0;

    //  Faces `facing` and presses A: whether a box opened at the bottom of
    //  the screen (a sign's may be green, not a dialogue's), what OCR reads
    //  in it, and whether B closed it.
    virtual DialogueRead read_sign(Pokemon::Direction facing, const std::string& label) = 0;

    //  The lit doorway nearest the middle of the screen, if any.
    virtual std::optional<LitDoorway> lit_doorway() = 0;

    //  From the doors' row: lines up with the lit doorway on screen, across,
    //  without going in (line_up_with_lit_door()).
    virtual Pokemon::Outcome line_up_with_door() = 0;

    //  Whether a Pokemon Center's inside shows (pokemon_center_inside()).
    virtual bool inside_pokemon_center() = 0;

    //  Anywhere in Sandgem Town: east until a wall stops us, then north
    //  until one does, which ends on the doors' row or above it, and never
    //  walks into a door.
    virtual Pokemon::Outcome east_then_north_in_sandgem() = 0;

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

//  On the doors' row, east of the Lab's sign, where a walk west along the
//  row stops (checked on the console).
Pokemon::TilePos east_of_lab_sign();

//  Whether `doorway` is straight ahead on the row below it: about a row up,
//  within half a tile across.
bool doorway_lined_up(const LitDoorway& doorway);

//  Whether a sign's OCR text has `key` (lower case), ignoring case.
bool sign_text_matches(const std::string& text, const std::string& key);

//  Whether `doorway` is the Center's, seen from the row below its door: up
//  the screen by about a row, and no further across than the Center's sign.
//  The Mart's doorway, seven tiles east of the sign, is too far off the
//  middle to be found from there (find_lit_doorway()).
bool center_doorway_at_front_row(const LitDoorway& doorway);



//  The checkpoint's events, run against `game`, which must outlive it. It
//  ends in front of the Center's door, not inside.
//  Events:
//      1. confirm_on_route_202     the save screen reads Route 202
//      2. walk_to_sandgem          the pose is on Sandgem Town
//      3. walk_to_center_sign      below the Center's sign (planned, town costs)
//      4. read_center_sign         it reads, facing north
//      4. at_center_door           the doorway straight ahead, a row up, outside
Pokemon::Checkpoint route202_to_sandgem_center_checkpoint(SandgemCenterActions& game);

extern const char* const ROUTE202_TO_SANDGEM_CENTER_NAME;

//  DIAGNOSTIC ONLY: Remove before pushing to production.
//  Runs the checkpoint on the console, from Route 202's save spot. Fails
//  with a screenshot, naming the event and why, if it does not complete.
void diagnostic_route202_to_sandgem_center(SingleSwitchProgramEnvironment& env, ProControllerContext& context);



//  The game steps of the Poke Mart lady checkpoint.
class MartLadyActions : public SandgemCenterActions{
public:
    //  Walks `direction` until something stops us, for at most `tiles`
    //  tiles' time. Fails on a transition or a dialogue.
    virtual Pokemon::Outcome walk_until_stopped(Pokemon::Direction direction, size_t tiles, const std::string& label) = 0;

    //  Faces `direction` and presses A. If a dialogue opens, reads its first
    //  box, then presses `next` once per line, 1.5 s apart, while it shows.
    virtual DialogueRead talk(Pokemon::Direction direction, Button next, const std::string& label) = 0;

    //  From the tile north of a sign, facing it: south a tile, two tiles
    //  `way` (WEST or EAST), and north a tile, to the sign's other side.
    virtual Pokemon::Outcome step_around_sign(Pokemon::Direction way) = 0;

    //  Walks `direction`, pressing A after each short stretch, for at most
    //  `tiles` tiles' time until someone answers; then one A per line, 1.5 s
    //  apart, while the box shows.
    virtual DialogueRead talk_walking(Pokemon::Direction direction, size_t tiles, const std::string& label) = 0;

    //  As 00.04 walks it after the Potions, from east of the Lab's sign on
    //  the doors' row to Route 202's save spot, and the save there, which
    //  must read Route 202.
    virtual Pokemon::Outcome walk_to_route_202_save_spot() = 0;
    virtual Pokemon::Outcome save_on_route_202() = 0;
};

//  West of Route 201's border sign, in world tiles of story_maps(): the
//  only tile it is read from facing east.
Pokemon::TilePos route201_sign_west();

//  The east end of the lady's row, against Sandgem's fence.
Pokemon::TilePos route201_row_3_east_end();

//  From in front of Sandgem's Pokemon Center door: to Route 201's border
//  sign and read it, talk to the Poke Mart lady, read the sign again, back
//  to the front of the Center's door, then to Route 202's save spot, where
//  the save it starts from stands, and save there, so it can be repeated.
//  Events:
//      1. at_center_door               the doorway straight ahead, a row up
//      2. walk_to_route_201_sign       the pose is the tile west of the sign
//      3. read_route_201_sign          the sign's dialogue opened and closed
//      4. talk_to_mart_lady            a dialogue opened and was closed
//      5. back_to_route_201_sign       the pose is the tile west of the sign
//      6. read_route_201_sign_again    the sign's dialogue opened and closed
//      7. back_to_center_door          the Center's doorway at the front row
//      8. at_center_door_again         the doorway straight ahead, a row up
//      9. walk_to_route_202_save_spot  the walk ended there (Segment 00's)
//  then the save, which must read Route 202.
Pokemon::Checkpoint sandgem_mart_lady_checkpoint(MartLadyActions& game);

extern const char* const SANDGEM_MART_LADY_NAME;

//  DIAGNOSTIC ONLY: Remove before pushing to production.
//  Runs the Mart lady checkpoint on the console, from in front of Sandgem's
//  Pokemon Center door; pins the clock first if `pin_clock`.
void diagnostic_sandgem_mart_lady(SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool pin_clock);



}
}
}
#endif
