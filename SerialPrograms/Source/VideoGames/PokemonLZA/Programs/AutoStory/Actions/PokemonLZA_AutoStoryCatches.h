/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
  *  Wild Zone 1's catches: 03.07's tutorial and 03.08's two more species.
 *  Each throw is throw_ball()'s: ZL 0.5 s to lock on, then ZL + ZR 1.5 s
 *  (0.5 s only aims), then 8 s for the flight, the shakes and the banner.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatches_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatches_H

#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryConsoleBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  03.07's first catch (Fletchling). A break-out brings two or three lines
//  from Taunie and control back; the catch brings her lines and then the
//  "Battling wild Pokemon" cards. So: throw, A
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

//  What catch_in_zone() is after: `new_species` new species caught, so
//  the party bar shows `party_species` filled slots, in `max_throws`
//  throws at most.
struct ZoneCatchGoal{
    int new_species = 0;
    int party_species = 0;
    int max_throws = 0;
};

//  03.08's catches for the "register five species" quest (3/5 after the
//  tutorial), in Wild Zone 1. From Taunie's rocks, where the start save stands, first
//  to the road's spot at 100: ZoneStart's measured leg left off the rocks
//  (TAUNIE_ROCKS_ROUTE), the start checked by the map pin (a marker read
//  only when the pin is unread), then map legs
//  (walk_map_legs(), ROCKS_TO_SPOT_100); it fails there, before any
//  catch, as "rocks_not_left", "leg_off_target" or "map_pin_not_read".
//  Then it patrols the user's two catch spots (CATCH_SPOTS): 100
//  (Scatterbug on the plaza's road) and 60 (Mareep by the far gate), back
//  and forth by map legs to their waypoints, 10 visits and 10 minutes at
//  most, turning back at once at a spot where a new species was caught
//  (spot_visit()); a leg that fails ends it with that failure. At each
//  stop, after a marker read logged as a cross-check, it fights a wild
//  battle that is on (one of a species already caught is fought off),
//  else locks on as upstream's InPlaceCatcher does: rounds of a camera
//  turn right, D-pad up, ZL mashed and held, until the battle menu's move
//  symbols show (MoveEffectivenessSymbolWatcher; none in 8 rounds: on to
//  the next stop). Then, ZL held, it flicks through
//  the targets' plates and throws at the nearest not caught
//  (lock_target_caught()), stepping toward it first while its plate reads
//  far (step_toward_target()). After a miss at a plate that locks on
//  again it steps toward it the same way and throws again, 3 throws at
//  one lock at most, then moves on to the next stop; it weakens a target
//  that broke out into a battle (after_throw()). The
//  party bar's filled slots (PartyBarDetector), read before the first
//  stop and after each stop that threw, end it at `goal.party_species`:
//  the throws go only at species not caught, so each catch is a new
//  species. Unread, the tally of "Registered!" banners
//  (DexRegisteredDetector) and plates read "not caught" before a throw
//  that left no battle and no plate decides (species_tally()). Fails as
//  "short_of_species", still in the zone.
Pokemon::Outcome catch_in_zone(StoryEnv& env, PlzaBackend& backend, const ZoneCatchGoal& goal);

//  After 03.08's catches: back onto the road at the 60 spot by a map leg
//  (walk_map_legs(), ZONE1_SPOT_60), no walk when the last catch left
//  the pin in the spot's stop area already. From 60 the far gate's pillars are
//  straight down the road; the marker is not on the gate but past it and
//  to its left, behind the zone's fence, so a walk at the marker was held
//  up at 36 and wandered off. Ends
//  with the camera on the marker. Fails as walk_map_legs() does.
Pokemon::Outcome walk_back_to_zone_road(StoryEnv& env, PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
