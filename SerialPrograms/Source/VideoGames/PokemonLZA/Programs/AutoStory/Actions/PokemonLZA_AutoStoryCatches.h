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


//  03.07's first catch (Fletchling): caught once a "Battling wild Pokemon"
//  card shows. Fails as "no_catch" after `max_throws` throws with no card.
Pokemon::Outcome catch_tutorial(StoryEnv& env, PlzaBackend& backend, int max_throws);

//  03.07's second catch (Bunnelby): rounds of attacks and a throw until a
//  round after the first finds no target to lock on. Fails as
//  "still_a_target" after `max_rounds`.
Pokemon::Outcome battle_then_catch(StoryEnv& env, PlzaBackend& backend, int max_rounds);

//  `party_species`: the party bar's filled slots once `new_species` are
//  caught.
struct ZoneCatchGoal{
    int new_species = 0;
    int party_species = 0;
    int max_throws = 0;
};

//  03.08's catches for the "register five species" quest. From Taunie's
//  rocks to the 100 spot (fails there, before any catch, as
//  "rocks_not_left", "leg_off_target" or "map_pin_not_read"), then patrols
//  CATCH_SPOTS, throwing only at species not caught, until the party bar
//  shows `goal.party_species`; unread, the tally of banners and plates
//  decides. Fails as "short_of_species", still in the zone.
Pokemon::Outcome catch_in_zone(StoryEnv& env, PlzaBackend& backend, const ZoneCatchGoal& goal);

//  Back onto the road at the 60 spot by a map leg: the marker is past the
//  far gate behind the zone's fence, so a walk at the marker wanders off.
//  Ends with the camera on the marker. Fails as walk_map_legs() does.
Pokemon::Outcome walk_back_to_zone_road(StoryEnv& env, PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
