/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Story steps that press through one particular screen of the story, on
 *  the engine's backend: Taunie's first map (03.02b) and the side street's
 *  ladder (03.03), and a story choice box ("choose_<index>").
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScenes_H
#define PokemonAutomation_PokemonLZA_AutoStoryScenes_H

#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  03.02b: Taunie's talk, the "Map & Missions" card, the first map and the
//  rest of her talk, until the party bar holds with no text box. The map
//  is not looked for: MapDetector needs the Travel Spots icon, which the
//  first map lacks. Fails as "map_talk_timeout" after 120 s.
std::optional<Pokemon::StoryFailure> talk_through_map_screen(PlzaBackend& backend);

//  03.03b, facing the side street's wall: steps along it until "Climb Up"
//  shows, A, then climbs. Fails as "ladder_not_found" when no step shows it.
std::optional<Pokemon::StoryFailure> climb_ladder(PlzaBackend& backend);

//  "choose_<index>" (0 the top): A through text until a choice box shows, then
//  moves to row `index` and presses A. Fails as "no_choice", "no_such_choice"
//  or "choice_not_reached".
std::optional<Pokemon::StoryFailure> choose_row(PlzaBackend& backend, int index);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
