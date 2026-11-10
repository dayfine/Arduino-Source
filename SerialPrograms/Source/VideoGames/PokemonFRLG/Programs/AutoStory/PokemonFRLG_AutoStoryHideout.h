/*  AutoStory Rocket Hideout Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The two story steps the Rocket Hideout needs that no other place does:
 *  the switch behind the Celadon Game Corner's poster, which opens the
 *  stairs down, and the lift's floor panel. Both are GAME steps named by
 *  their variant, as the draft plans write them (StoryData/Draft).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryHideout_H
#define PokemonAutomation_PokemonFRLG_AutoStoryHideout_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "CommonFramework/Language.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that pushes the switch behind the Game Corner's poster,
//  the player facing it (pokefirered CeladonCity_GameCorner_EventScript_
//  Poster): A, "Hey! A switch behind the POSTER!? Let's push it!", and the
//  stairs (15..17, 2..3) open for good. The Grunt in front of it must be
//  beaten first.
inline const std::string POSTER_SWITCH_VARIANT = "poster_switch";

//  The GAME step that rides the Rocket Hideout's lift with the LIFT KEY,
//  its args `from` and `to`, floors as the panel names them (B1F, B2F,
//  B4F). `from` is the floor the lift stands at, where the panel's cursor
//  starts. The player starts below `from`'s lift door, facing north, and
//  ends on `to`'s lift door (rocket_hideout_lift_doors()): the lift is
//  walked by a script (lift_in_legs(), lift_out_legs()), not a map.
inline constexpr std::string_view LIFT_FLOOR_VARIANT = "lift_floor";
//  Its args: the floors it rides from and to.
inline constexpr Pokemon::StoryArgSpec LIFT_FLOOR_ARGS[] = {
    {"from", Pokemon::StoryArgType::TEXT, true},
    {"to", Pokemon::StoryArgType::TEXT, true},
};

//  The lift panel's menu, top first (pokefirered
//  sMultichoiceList_RocketHideoutElevator): B1F, B2F, B4F, EXIT.
const std::vector<std::string>& rocket_hideout_lift_menu();

//  A ride: the menu rows the cursor starts on and is moved to.
struct LiftRide{
    size_t from = 0;
    size_t to = 0;
};
//  The ride a lift_floor step names, or nothing if it is not one, or names
//  a floor the panel hasn't.
std::optional<LiftRide> parse_lift_floor_step(const Pokemon::StoryStep& step);

//  Presses of the D-pad on a menu: which way, how many.
struct MenuPresses{
    Pokemon::Direction direction = Pokemon::Direction::SOUTH;
    size_t presses = 0;
};
//  The fewest presses that move the cursor from `ride.from` to `ride.to`
//  on a menu of `entries` rows. A multichoice of more than three rows
//  wraps round (pokefirered DrawVerticalMultichoiceMenu()).
MenuPresses lift_menu_presses(const LiftRide& ride, size_t entries);

//  Whether `step` is one of this file's steps.
bool is_hideout_step(const Pokemon::StoryStep& step);

//  On the console: A on the panel, the floor picked, the ride waited out
//  back to the overworld, the player still in the lift.
void pick_lift_floor(ConsoleHandle& console, ProControllerContext& context, const LiftRide& ride);
//  On the console, the whole ride: into the lift from below the door,
//  the floor picked, out onto the floor's lift door.
void ride_lift(ConsoleHandle& console, ProControllerContext& context, const LiftRide& ride);
//  On the console: A on the poster, its message closed back to the
//  overworld.
void push_poster_switch(ConsoleHandle& console, ProControllerContext& context);

//  On the console, a GAME step of this file's, a Pokemon Center's
//  (is_center_variant()) or an item from the bag (is_item_use_step());
//  nothing for another GAME step. The step ends on its pose_after, if it
//  has one (an Escape Rope's, a lift ride's, a Center's).
std::optional<Pokemon::Outcome> run_field_game_step(
    ConsoleHandle& console, ProControllerContext& context, Language language, const Pokemon::StoryStep& step
);


//  Registers the hideout steps' tests.
void add_tests_AutoStoryHideout(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
