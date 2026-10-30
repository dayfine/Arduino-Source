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

    //  One tile `direction`: the one hold that is not a walk until stopped.
    //  The walk after it stops on something that tells whether it took.
    virtual Pokemon::Outcome step(Pokemon::Direction direction, const std::string& label) = 0;

    //  Walks `direction`, pressing A after each short stretch, for at most
    //  `tiles` tiles' time until someone answers; then one A per line, 1.5 s
    //  apart, while the box shows.
    virtual DialogueRead talk_walking(Pokemon::Direction direction, size_t tiles, const std::string& label) = 0;

    //  As 00.04 walks it after the Potions, from the trees east of the Mart
    //  (the front row's east end) to Route 202's save spot, and the save
    //  there, which must read Route 202.
    virtual Pokemon::Outcome walk_to_route_202_save_spot() = 0;
    virtual Pokemon::Outcome save_on_route_202() = 0;
};

//  West of Route 201's border sign, in world tiles of story_maps(): the
//  only tile it is read from facing east.
Pokemon::TilePos route201_sign_west();

//  The east end of the lady's row, against Sandgem's fence.
Pokemon::TilePos route201_row_3_east_end();

//  From below Sandgem's Pokemon Center sign (where the Center checkpoint
//  ends): to the Poke Mart lady and back to Route 202's save spot, where the
//  save it starts from stands, and save there, so it can be repeated. Every
//  leg but three one-tile steps walks until something stops it, as measured
//  on the console (2026-10-30, CFR "walk"):
//      1. on_doors_row                 below the Center's sign
//      2. west_to_lab_sign             west until the Lab's sign stops us
//      3. read_lab_sign                facing west, "Pokemon Research Lab"
//      4. west_along_the_path          south a tile, west to the grass's edge
//      5. north_to_the_trees           north until the trees stop us (2 rows)
//      6. talk_to_mart_lady            east until she stops us; she talks
//      7. back_to_route_201_sign       south a tile, east until the sign stops us
//      8. read_route_201_sign          facing east, "Route 201 -> Sandgem Town"
//      9. east_along_the_path          south a tile, east to the trees, north a row
//     10. walk_to_route_202_save_spot  the walk ended there (Segment 00's)
//  then the save, which must read Route 202.
Pokemon::Checkpoint sandgem_mart_lady_checkpoint(MartLadyActions& game);

extern const char* const SANDGEM_MART_LADY_NAME;

//  DIAGNOSTIC ONLY: Remove before pushing to production.
//  Runs the Mart lady checkpoint on the console, from in front of Sandgem's
//  Pokemon Center door.
void diagnostic_sandgem_mart_lady(SingleSwitchProgramEnvironment& env, ProControllerContext& context);


//  The game steps of 00.05's first two checkpoints, in Jubilife.
class JubilifeActions : public MartLadyActions{
public:
    //  From 00.05's save below the steps, against the wall east of them: a
    //  tile west and up the steps onto the main road.
    virtual Pokemon::Outcome climb_the_steps() = 0;
    //  From in front of a Pokemon Center, its lit doorway on screen: heals
    //  and walks out. Ends on the Center's doorstep.
    virtual Pokemon::Outcome heal_at_center() = 0;
    //  Faces `direction` without moving: a tap the game takes as a turn.
    virtual void turn(Pokemon::Direction direction) = 0;
    //  Half a tile `direction` (EAST or WEST), facing it first.
    virtual void nudge(Pokemon::Direction direction, const std::string& label) = 0;
    //  On the School's doorsteps' row, near its door: tiles across from the
    //  player to the door's column, read from the lit lamp east of it;
    //  nothing if no lamp is lit (by day).
    virtual std::optional<double> school_door_across() = 0;
    //  Moves across by `tiles` (positive: east).
    virtual void step_across(double tiles, const std::string& label) = 0;
    //  Whether the screen shows a building's inside.
    virtual bool indoors() = 0;
    //  Inside a building whose door is straight south: out through it. Fails
    //  if three walks south do not change the scene.
    virtual Pokemon::Outcome leave_south(const std::string& label) = 0;
    //  One tile `direction` from standing still, facing that way already.
    virtual Pokemon::Outcome tile(Pokemon::Direction direction, const std::string& label) = 0;
    //  Walks north; true if a door took us through.
    virtual bool through_door_north(const std::string& label) = 0;
};

//  00.05a: from 00.05's save below Jubilife's steps to the Pokemon Center,
//  healed, on its doorstep. Measured on the console (2026-10-30):
//      1. up_the_steps          a tile west, then north up the steps
//      2. west_to_the_fence     west until the fence's end stops us
//      3. north_to_the_sign     north until the sign by the west lamp stops us
//      4. east_to_the_house     east across the road until the house stops us
//      5. north_to_the_center   north until the Center's doorway stops us
//      6. heal                  in, the nurse, out onto the doorstep
//  No save: the save stays below the steps, so it can be repeated.
Pokemon::Checkpoint jubilife_center_checkpoint(JubilifeActions& game);
extern const char* const JUBILIFE_CENTER_NAME;

//  00.05b: from the Center's doorstep into the Trainers' School. Nothing on
//  the doorsteps' row stops a walk, so the School's door is counted: eleven
//  one-tile steps west (measured both ways), then north; half a tile either
//  side if the School's front stops us.
//      1. into_the_school       through the School's door
Pokemon::Checkpoint jubilife_school_checkpoint(JubilifeActions& game);
extern const char* const JUBILIFE_SCHOOL_NAME;

//  00.05c: in the Trainers' School, from its door: Barry, who gives the Town
//  Map, and back out onto the School's doorstep.
//      1. up_to_barry           north up the door's column to the back wall
//      2. talk_to_barry         face west: his scene, to "You obtained a Town Map!"
//      3. out_of_the_school     south down the door's column and out
Pokemon::Checkpoint jubilife_barry_checkpoint(JubilifeActions& game);
extern const char* const JUBILIFE_BARRY_NAME;

//  DIAGNOSTIC ONLY: Remove before pushing to production.
void diagnostic_jubilife_barry(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
//  DIAGNOSTIC ONLY: Remove before pushing to production.
void diagnostic_jubilife_center(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
void diagnostic_jubilife_school(SingleSwitchProgramEnvironment& env, ProControllerContext& context);



}
}
}
#endif
