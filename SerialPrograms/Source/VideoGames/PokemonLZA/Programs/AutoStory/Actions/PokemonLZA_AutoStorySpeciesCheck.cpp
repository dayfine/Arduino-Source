/*  PokemonLZA AutoStory - Species check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "PokemonLZA_AutoStorySpeciesCheck.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


SpeciesTally species_tally(const GrassCatches& patrol, int wanted){
    if (patrol.found >= wanted){
        return patrol.found_by_plate > 0 ? SpeciesTally::INFERRED : SpeciesTally::BANNERS;
    }
    if (patrol.found == 0 && patrol.throws == 0 && patrol.caught_seen > 0){
        return SpeciesTally::INFERRED;
    }
    return SpeciesTally::SHORT;
}

std::string_view species_tally_text(SpeciesTally tally){
    switch (tally){
    case SpeciesTally::BANNERS:
        return "all by the banner";
    case SpeciesTally::INFERRED:
        return "not all by the banner";
    case SpeciesTally::SHORT:
        return "short";
    }
    return "?";
}

bool enough_species(SpeciesTally tally, std::optional<int> dex_count, int dex_needed){
    if (dex_count){
        return *dex_count >= dex_needed;
    }
    return tally != SpeciesTally::SHORT;
}

bool party_has_species(std::optional<int> filled_slots, int dex_species){
    return filled_slots.has_value() && *filled_slots > 0 && *filled_slots >= dex_species;
}

std::string catch_summary_text(const GrassCatches& patrol, std::optional<int> filled_slots, int new_species, int party_species){
    const std::string banners = std::format(
        "the banners: {} new species ({} by a plate only) in {} throws, {} locks on caught species only",
        patrol.found, patrol.found_by_plate, patrol.throws, patrol.caught_seen
    );
    if (filled_slots){
        return std::format(
            "the party bar: {} filled slots of {}: {}; {}",
            *filled_slots, party_species, party_has_species(filled_slots, party_species) ? "done" : "short", banners
        );
    }
    return std::format("the party bar: not read; {}: {}", banners, species_tally_text(species_tally(patrol, new_species)));
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
