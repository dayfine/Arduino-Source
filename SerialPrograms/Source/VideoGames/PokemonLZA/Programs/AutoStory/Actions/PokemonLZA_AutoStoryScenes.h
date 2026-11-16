/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Story steps that press through one particular screen of the story, on
 *  the engine's backend: Taunie's first map (03.02b) and the side street's
 *  ladder (03.03).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScenes_H
#define PokemonAutomation_PokemonLZA_AutoStoryScenes_H

#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Taunie's talk after her battle (03.02b), the "Map & Missions" card, the
//  first map and the rest of her talk, one press a second by what shows,
//  until the party bar shows with no text box on looks a second apart: a
//  tutorial card (TUTORIAL_CARD_READING) and text (a box or the advance
//  chevron) before the card get A, anything else B. A pages the card and
//  closes it on its last page, where B would turn back (console B
//  2026-11-16: A and B in turn held it on page 1 for 120 s); B closes the
//  map, where A would place a pin. The map is not looked for: MapDetector
//  needs the Travel Spots icon, which the first map lacks (console B,
//  2026-11-13). No presses once the party bar shows. Fails as
//  "map_talk_timeout" after 120 s (by the looks' time).
Pokemon::Outcome talk_through_map_screen(PlzaBackend& backend);

//  Facing the side street's wall (03.03): sidestep right to the ladder
//  (0.6 s), face it (150 ms), A at "A Climb Up", then hold up 5 s to the
//  top. Reads nothing: the rooftop scene after it is the check.
void climb_ladder(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
