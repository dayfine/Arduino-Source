/*  Legendary Catch Requirements
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_LegendaryCatchRequirements_H
#define PokemonAutomation_PokemonFRLG_LegendaryCatchRequirements_H

#include <map>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


enum class LegendaryTarget{
    Articuno,
    Zapdos,
    Moltres,
    Mewtwo,
    Deoxys,
    Lugia,
    HoOh,
    Raikou,
    Entei,
    Suicune,
};

struct PartyMemberRequirement{
    std::vector<std::string> accepted_species;
    unsigned min_level = 1;
    unsigned min_speed = 0;
    bool required = true;
    std::string role_description;
};

enum class BagPocket{
    Items,
    KeyItems,
    PokeBalls,
};

struct BagItemRequirement{
    BagPocket pocket = BagPocket::Items;
    std::string item_slug;
    std::string display_name;
    unsigned min_count = 1;
};

struct LegendaryCatchRequirements{
    std::vector<PartyMemberRequirement> party_members;
    std::vector<BagItemRequirement> bag_items;
};

struct ObservedPartyMember{
    std::string species;
    unsigned level = 0;
    unsigned speed = 0;
};

//  Compute the combined party (OR / MAX) and bag (SUM / MAX) requirements for
//  all selected legendaries.
LegendaryCatchRequirements compute_legendary_catch_requirements(
    const std::vector<LegendaryTarget>& targets,
    unsigned luxury_balls_per_legendary = 50,
    bool include_type_walls = false
);

//  Return a list of unmet party member requirements (empty if satisfied).
std::vector<std::string> check_party_requirements(
    const LegendaryCatchRequirements& requirements,
    const std::vector<ObservedPartyMember>& party
);

//  Return a list of unmet bag item requirements (empty if satisfied).
//  `bag_counts` maps lowercase item slug (e.g. "luxury-ball", "max-repel") to count.
std::vector<std::string> check_bag_requirements(
    const LegendaryCatchRequirements& requirements,
    const std::map<std::string, unsigned>& bag_counts
);

void add_tests_LegendaryCatchRequirements(UnitTestDatabase& database);


}
}
}
#endif
