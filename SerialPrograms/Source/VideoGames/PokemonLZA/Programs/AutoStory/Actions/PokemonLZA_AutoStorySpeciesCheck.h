/*  PokemonLZA AutoStory - Species check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure species-count tallying, party-bar settling, and catch-summary formatting.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySpeciesCheck_H
#define PokemonAutomation_PokemonLZA_AutoStorySpeciesCheck_H

#include <optional>
#include <string>
#include "Common/Cpp/MoveOnlyFunction.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Aggregated catch patrol statistics across stops.
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

//  Confidence level of catch_in_zone()'s species count.
enum class SpeciesTally{
    BANNERS,
    INFERRED,
    SHORT,
};

//  Classifies `patrol` against `wanted` new species.
SpeciesTally species_tally(const GrassCatches& patrol, int wanted);

//  True if `dex_count` (when present) meets `dex_needed`, or otherwise `tally != SHORT`.
bool enough_species(SpeciesTally tally, std::optional<int> dex_count, int dex_needed);

//  True if `filled_slots` is present and at least `dex_species`.
bool party_has_species(std::optional<int> filled_slots, int dex_species);

//  Polls `read_look` until the party bar's filled-slot count settles, or returns std::nullopt if unread.
std::optional<int> settled_party_slots(MoveOnlyFunction<std::optional<int>()> read_look);

//  Formats the catch patrol summary using `filled_slots` when present, else `patrol` tally.
std::string catch_summary_text(const GrassCatches& patrol, std::optional<int> filled_slots, int new_species, int party_species);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
