/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Wild Zone 1's tutorial catches (03.07), from console B, 2026-11-10.
 *  Each throw is throw_ball()'s: ZL 0.5 s to lock on, then ZL + ZR 1.5 s
 *  (0.5 s only aims), then 8 s for the flight, the shakes and the banner.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatches_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatches_H

#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  03.07's first catch (Fletchling). A break-out brings two or three lines
//  from Taunie and control back; the catch brings her lines and then the
//  "Battling wild Pokemon" cards (console B, 2026-11-10). So: throw, A
//  through the lines (press_a_through_lines()), caught once a card shows.
//  A break-out into a wild battle gets the battle AI's attacks before the
//  next throw. Fails as "no_catch" after `max_throws` throws with no card.
Pokemon::Outcome catch_tutorial(StoryEnv& env, PlzaBackend& backend, int max_throws);

//  03.07's second catch (Bunnelby), after the cards: A past its "weaken
//  it" banner (D-pad up is ignored while it shows), D-pad up sends the
//  partner out, then rounds of attacks (3 that land) and a throw, until a
//  round after the first finds no target to lock on: caught. Fails as
//  "still_a_target" after `max_rounds`.
Pokemon::Outcome battle_then_catch(StoryEnv& env, PlzaBackend& backend, int max_rounds);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
