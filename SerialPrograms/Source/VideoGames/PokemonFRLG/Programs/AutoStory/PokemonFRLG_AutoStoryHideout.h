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

//  The GAME step that rides the Rocket Hideout's lift, the player facing
//  its panel with the LIFT KEY: "lift_floor <from> <to>", floors as the
//  panel names them (B1F, B2F, B4F). `from` is the floor the lift stands
//  at, where the panel's cursor starts.
inline constexpr std::string_view LIFT_FLOOR_PREFIX = "lift_floor ";

//  The lift panel's menu, top first (pokefirered
//  sMultichoiceList_RocketHideoutElevator): B1F, B2F, B4F, EXIT.
const std::vector<std::string>& rocket_hideout_lift_menu();

//  A ride: the menu rows the cursor starts on and is moved to.
struct LiftRide{
    size_t from = 0;
    size_t to = 0;
};
//  The variant for a ride from floor `from` to `to`.
std::string lift_floor_variant(std::string_view from, std::string_view to);
//  The ride a "lift_floor <from> <to>" variant names, or nothing if it is
//  not one, or names a floor the panel hasn't.
std::optional<LiftRide> parse_lift_floor_variant(std::string_view variant);

//  Presses of the D-pad on a menu: which way, how many.
struct MenuPresses{
    Pokemon::Direction direction = Pokemon::Direction::SOUTH;
    size_t presses = 0;
};
//  The fewest presses that move the cursor from `ride.from` to `ride.to`
//  on a menu of `entries` rows. A multichoice of more than three rows
//  wraps round (pokefirered DrawVerticalMultichoiceMenu()).
MenuPresses lift_menu_presses(const LiftRide& ride, size_t entries);

//  Whether `variant` is one of this file's steps.
bool is_hideout_variant(std::string_view variant);

//  On the console: A on the panel, the floor picked, the ride waited out
//  back to the overworld, the player still in the lift.
void ride_lift(ConsoleHandle& console, ProControllerContext& context, const LiftRide& ride);
//  On the console: A on the poster, its message closed back to the
//  overworld.
void push_poster_switch(ConsoleHandle& console, ProControllerContext& context);

//  On the console, a GAME step of this file's or an item from the bag
//  (is_item_use_variant()); nothing for another GAME step. The hideout's
//  steps leave the player where they stand; an item used ends on the
//  step's pose_after (an Escape Rope's), if it has one.
std::optional<Pokemon::Outcome> run_field_game_step(
    ConsoleHandle& console, ProControllerContext& context, Language language, const Pokemon::StoryStep& step
);


//  Registers the hideout steps' tests.
void add_tests_AutoStoryHideout(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
