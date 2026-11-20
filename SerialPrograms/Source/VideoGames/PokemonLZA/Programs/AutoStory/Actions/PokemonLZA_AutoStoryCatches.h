/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Console-backed Wild Zone 1 tutorial and species-registration catch actions.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatches_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatches_H

#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryConsoleBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Executes the first tutorial catch until the post-catch tutorial card appears.
//  Fails as "no_catch" after `max_throws` throws without completion.
std::optional<Pokemon::StoryFailure> catch_tutorial(StoryEnv& env, PlzaBackend& backend, int max_throws);

//  Weakens and throws at the second tutorial target until no lock-on target remains.
//  Fails as "still_a_target" after `max_rounds` rounds.
std::optional<Pokemon::StoryFailure> battle_then_catch(StoryEnv& env, PlzaBackend& backend, int max_rounds);

//  Target counts and throw budget for catch_in_zone().
struct ZoneCatchGoal{
    int new_species = 0;
    int party_species = 0;
    int max_throws = 0;
};

//  Patrols Wild Zone 1 to catch uncaught species until `goal` is met.
//  Fails as "rocks_not_left", "leg_off_target", "map_pin_not_read", or "short_of_species".
std::optional<Pokemon::StoryFailure> catch_in_zone(StoryEnv& env, PlzaBackend& backend, const ZoneCatchGoal& goal);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
