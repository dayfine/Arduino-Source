/*  PokemonLZA AutoStory - Species check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free choices behind catch_in_zone()'s end: how sure its
 *  own count of new species is, whether to read the Pokedex's count, and
 *  whether the zone's "register species" quest is done.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySpeciesCheck_H
#define PokemonAutomation_PokemonLZA_AutoStorySpeciesCheck_H

#include <optional>
#include <string_view>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What catch_in_zone()'s stops did: new species caught (`found_by_plate`
//  of them by NewSpeciesSign::PLATE_NOT_CAUGHT, the rest by the banner),
//  throws, and locks on that found only species already caught.
struct GrassCatches{
    int found = 0;
    int found_by_plate = 0;
    int throws = 0;
    int caught_seen = 0;

    GrassCatches& operator+=(const GrassCatches& other){
        found += other.found;
        found_by_plate += other.found_by_plate;
        throws += other.throws;
        caught_seen += other.caught_seen;
        return *this;
    }
};

//  How sure catch_in_zone()'s own count of new species is.
enum class SpeciesTally{
    //  All wanted, each by the Pokedex's "Registered!" banner.
    BANNERS,
    //  All wanted, some only by a plate read "not caught" before the throw;
    //  or none caught and none thrown at, only species already caught
    //  locking on (a rerun from a save after the catches): taken as done.
    INFERRED,
    //  Fewer than wanted (a banner may have been missed).
    SHORT,
};

//  How sure `patrol`'s count of new species is, `wanted` of them asked.
SpeciesTally species_tally(const GrassCatches& patrol, int wanted);

//  The tally, for the log.
std::string_view species_tally_text(SpeciesTally tally);

//  Whether to open the Pokedex and read its count: at THOROUGH and TESTING
//  checks always, at the others when the tally is not all banners.
bool read_dex_count(Pokemon::StoryCheckLevel checks, SpeciesTally tally);

//  Whether the zone's quest is done: the Pokedex's count, when read, has
//  `dex_needed`; unread, the tally is not SHORT.
bool enough_species(SpeciesTally tally, std::optional<int> dex_count, int dex_needed);

//  Whether the patrol is done by the overworld party bar: `filled_slots`
//  of it filled (PartyBarDetector; none: the bar was not read) and
//  `dex_species` species wanted. The plate keeps the throws to species
//  not caught, so every catch fills a slot with a new species and the
//  filled slots are the registered species (the starter and the catches:
//  3 in the tutorial's save). The "Registered!" banner is missed at times
//  (plate "not caught", a throw, no banner over 208 frames, yet Mareep in
//  the party: console B 2026-11-14 19:36), so the bar decides, not the
//  banner tally (the user: "we are obviously done when we have 5
//  different icons in the party").
bool party_has_species(std::optional<int> filled_slots, int dex_species);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
