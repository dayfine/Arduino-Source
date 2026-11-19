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

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What catch_in_zone()'s stops did; `caught_seen`: locks on that found only
//  species already caught.
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
    BANNERS,
    //  All wanted, some only by a plate read "not caught"; or none thrown at,
    //  only species already caught locking on (a rerun after the catches).
    INFERRED,
    //  Fewer than wanted (a banner may have been missed).
    SHORT,
};

//  `wanted`: new species asked.
SpeciesTally species_tally(const GrassCatches& patrol, int wanted);

//  For the log.
std::string_view species_tally_text(SpeciesTally tally);

//  The Pokedex's count, when read, has `dex_needed`; unread, the tally is
//  not SHORT.
bool enough_species(SpeciesTally tally, std::optional<int> dex_count, int dex_needed);

//  The throws go only at species not caught, so the filled slots are the
//  registered species. The banner is missed at times, so the bar decides.
bool party_has_species(std::optional<int> filled_slots, int dex_species);

//  The party bar's filled slots once one count has held for 3 s, reading a
//  look at a time with `read_look` (none: no bar on that look). A catch's
//  new member joins the bar seconds after its banner: 03.08 read 4 of 5
//  slots and failed, and its retry at once read 5 (console B, 2026-11-18).
//  None when no look shows the bar in 2 s; the last count after 8 s.
std::optional<int> settled_party_slots(const std::function<std::optional<int>()>& read_look);

//  The party bar's verdict first, as it decides when read; the banner
//  tally's verdict only when the bar was not read.
std::string catch_summary_text(const GrassCatches& patrol, std::optional<int> filled_slots, int new_species, int party_species);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
