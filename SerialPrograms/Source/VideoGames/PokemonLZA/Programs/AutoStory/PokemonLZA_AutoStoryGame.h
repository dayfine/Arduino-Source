/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's actions for the shared story engine
 *  (Pokemon/AutoStory/Engine/Pokemon_StoryGame.h): the specs a checkpoint's
 *  JSON is read against.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryGame_H

#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  PLZA's own actions and its replacements of built-in ones (same kind and
//  name), in the order the game registers them:
//
//      ROUTE   "fly"       fly to the target, whose map is a fast travel
//                          place's slug ("vert-pokemon-center")
//      TALK    "talk"      A at what the player faces
//      ANSWER  "answer"    the first choice for yes, the second for no
//      MASH    "mash"      B through the text until the overworld is back
//      MASH    "a"         A through it
//      HEAL    "heal"      the nurse (not implemented)
//      SHOP    "shop"      the counter (not implemented)
//      GAME    a move of story_moves(); "approach_starter"; "choose_0" /
//              "choose_1"; "story_battle"; "new_game_language", "pick_look",
//              "type_player_name"; "mash_to_move_hints", "text_to_choice";
//              "story_battle_start", "talk_through_map_screen",
//              "talk_to_pokedex_screen"; "climb_ladder"; or an action of
//              story_actions_not_implemented()
//
//  BATTLE is the built-in "fight" (the backend's battle) and SAVE the
//  engine's. Tile walks (LEGS, STEP) and the built-in "route" mean nothing
//  in Lumiose: they are not replaced, and fail by name.
std::vector<Pokemon::StoryActionSpec> plza_story_action_specs();

//  Every action a PLZA checkpoint may name: the built-ins with
//  plza_story_action_specs() on top, as the engine registers them. Its JSON
//  is read against these.
const std::vector<Pokemon::StoryActionSpec>& plza_story_specs();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
