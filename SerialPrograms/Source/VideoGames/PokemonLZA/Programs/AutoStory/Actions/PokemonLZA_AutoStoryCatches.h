/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Wild Zone 1's catches: 03.07's tutorial, from console B, 2026-11-10,
 *  and 03.08's two more species, from console B, 2026-11-13 and 14.
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

//  What catch_in_zone() is after: the overworld party bar showing
//  `party_species` filled slots, in `max_throws` throws at most.
struct ZoneCatchGoal{
    int party_species = 0;
    int max_throws = 0;
};

//  03.08's catches for the "register five species" quest (3/5 after the
//  tutorial): back and forth between the user's two catch spots on Wild
//  Zone 1's road, by the objective marker's distance (100: Scatterbug on
//  the plaza's road; 60: Mareep by the far gate), 10 visits and 10 minutes
//  at most. At each stop a wild battle that is on is fought: one of a
//  species already caught (its lock-on plate shows the caught mark,
//  lock_target_caught()) is fought off, others weakened and caught.
//  Else ZL is held until a plate shows, the right stick flicked to the
//  next target while the plate shows the caught mark, and the ball thrown
//  at the first one not caught, the camera turned right between looks (4
//  at most). The party bar's filled slots (PartyBarDetector), read before
//  the first stop and after each stop that threw, end the patrol at
//  `goal.party_species`: the throws go only at species not caught, so
//  each catch is a new species (the Pokedex's "Registered!" banner is
//  missed at times; console B 2026-11-14). Fails as "short_of_species",
//  still in the zone.
Pokemon::Outcome catch_in_zone(StoryEnv& env, PlzaBackend& backend, const ZoneCatchGoal& goal);

//  After 03.08's catches: back onto the road at the catch spots nearer
//  the far gate than the player, or at 60 when at or past it, by the
//  objective marker (walk_toward_marker(), walk_back_from_marker()). From
//  60 the far gate's pillars are straight down the road; the marker is not
//  on the gate but past it and to its left, behind the zone's fence, so a
//  walk at the marker was held up at 36 and wandered off (console B,
//  2026-11-13 and 2026-11-14). Fails as "road_stop_not_reached".
Pokemon::Outcome walk_back_to_zone_road(StoryEnv& env, PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
