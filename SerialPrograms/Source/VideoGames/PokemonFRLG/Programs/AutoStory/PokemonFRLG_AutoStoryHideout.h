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
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that pushes the switch behind the Game Corner's poster,
//  the player facing it (pokefirered CeladonCity_GameCorner_EventScript_
//  Poster): A, "Hey! A switch behind the POSTER!? Let's push it!", and the
//  stairs (15..17, 2..3) open for good. The Grunt in front of it must be
//  beaten first.
inline const std::string POSTER_SWITCH_VARIANT = "poster_switch";

//  The GAME step that rides a lift, the Rocket Hideout's with the LIFT KEY
//  or the Celadon Department Store's, its args `from` and `to`, floors as
//  the panel names them (B1F, B2F, B4F; 5F to 1F). `from` is the floor the
//  lift stands at, where the panel's cursor starts. The player starts
//  below `from`'s lift door, facing north, and ends a tile below `to`'s
//  (lift_exits()): the lift is walked by a script (lift_in_legs(),
//  lift_out_legs()), not a map.
inline const std::string LIFT_FLOOR_VARIANT = "lift_floor";


//  Whether `step` is one of this file's steps.
bool is_hideout_step(const Pokemon::StoryStep& step);
//  The GAME actions of this file, a Pokemon Center's and the bag's, with
//  their args: FRLG's table adds them (story_action_specs()).
std::vector<Pokemon::StoryActionSpec> field_action_specs();

//  On the console: A on the poster, its message closed back to the
//  overworld.
void push_poster_switch(ConsoleHandle& console, ProControllerContext& context);

//  On the console, a GAME step of this file's, a Pokemon Center's
//  (is_center_variant()), a field move (is_field_move_step()) or an item
//  from the bag (is_item_use_step());
//  nothing for another GAME step. The step ends on its pose_after, if it
//  has one (an Escape Rope's, a lift ride's, a Center's).
std::optional<Pokemon::Outcome> run_field_game_step(
    ConsoleHandle& console, ProControllerContext& context, Language language, PlayerGender player,
    const Pokemon::StoryStep& step
);


//  Registers the hideout steps' tests.
void add_tests_AutoStoryHideout(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
