/*  PokemonLZA AutoStory - Species check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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

bool read_dex_count(Pokemon::StoryCheckLevel checks, SpeciesTally tally){
    if (tally != SpeciesTally::BANNERS){
        return true;
    }
    return checks == Pokemon::StoryCheckLevel::THOROUGH || checks == Pokemon::StoryCheckLevel::TESTING;
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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
