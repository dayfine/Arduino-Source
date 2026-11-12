/*  AutoStory Battle Data Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The battle data, and the attacks chosen against trainers' parties and wild Pokemon from it.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "PokemonFRLG_AutoStoryParty.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryBattleDataInput{
    std::string name;
    UnitTestResult (*body)();
};

struct AutoStoryBattleDataExpectation{
    std::string_view description;
};

struct AutoStoryBattleDataTestCase{
    AutoStoryBattleDataInput input;
    AutoStoryBattleDataExpectation expectation;
};

class Test_AutoStoryBattleData : public UnitTest{
public:
    explicit Test_AutoStoryBattleData(AutoStoryBattleDataTestCase scenario)
        : UnitTest(std::format("PokemonFRLG::AutoStoryBattleData - {}", scenario.input.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_scenario.input.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStoryBattleDataTestCase m_scenario;
};

const PartyStatus GREEN{.hp_seen = true, .hp_yellow = false, .hp_red = false, .status = StatusCondition::NONE};

//  The story's Wartortle once Bite is in: Tackle, Bite, Water Gun, Withdraw.
constexpr size_t BITE = 1;
constexpr size_t WATER_GUN = BOTTOM_LEFT_MOVE;

std::string slot_name(std::optional<size_t> slot){
    return slot ? std::to_string(*slot) : "none";
}

//  The slot a wild battle's first turn attacks with, against `species` as
//  read off the screen.
size_t wild_turn(const PartyMember& lead, std::string_view species){
    Supplies supplies;
    supplies.lead = lead;
    supplies.potions = 5;
    supplies.wild_foes = wild_foes(species);
    BattleDecisions wild(supplies, BattleKind::WILD_FIGHT);
    return wild.turn(GREEN).slot;
}


//  Spot checks against pret: the type chart, Bite, Growl and species'
//  types.
UnitTestResult test_data(){
    if (type_effectiveness(PokemonType::WATER, PokemonType::FIRE) != 20
        || type_effectiveness(PokemonType::DARK, PokemonType::PSYCHIC) != 20
        || type_effectiveness(PokemonType::NORMAL, PokemonType::GHOST) != 0
        || type_effectiveness(PokemonType::WATER, PokemonType::NORMAL) != 10
    ){
        return "Expected Water 2x on Fire, Dark 2x on Psychic, Normal 0x on Ghost, Water 1x on Normal.";
    }
    const MoveData* bite = move_data("bite");
    if (bite == nullptr || !bite->damaging || bite->type != PokemonType::DARK || bite->pp != 25){
        return "Expected Bite: damaging, Dark, 25 PP.";
    }
    const MoveData* growl = move_data("growl");
    if (growl == nullptr || growl->damaging || growl->pp != 40){
        return "Expected Growl: no damage, 40 PP.";
    }
    const SpeciesData* starmie = species_data("starmie");
    if (starmie == nullptr || starmie->first != PokemonType::WATER || starmie->second != PokemonType::PSYCHIC){
        return "Expected Starmie: Water, Psychic.";
    }
    if (battle_species().size() != 386 || species_data("farfetch-d") == nullptr || species_data("missingno") != nullptr){
        return std::format("Expected the 386 species by the program's slugs, got {}.", battle_species().size());
    }
    return true;
}

//  Mt. Moon: Super Nerd Miguel's Grimer (Poison) takes Water Gun, with or
//  without Bite known: Water Gun and Bite both hit his party 1x, and the
//  tie goes to the plan's Water Gun.
UnitTestResult test_miguel_grimer(){
    const std::vector<std::string> miguel{"grimer", "voltorb", "koffing"};
    const std::vector<PartyMember> leads{grown_to(new_member("squirtle", 5), 17), grown_to(new_member("squirtle", 5), 20)};
    for (const PartyMember& lead : leads){
        for (const std::vector<std::string>& foes : {std::vector<std::string>{"grimer"}, miguel}){
            const std::optional<size_t> slot = attack_slot_against(lead, foes);
            if (slot != WATER_GUN){
                return std::format("{} against {} foes: expected Water Gun, got slot {}.", lead.species, foes.size(), slot_name(slot));
            }
        }
    }
    return true;
}

//  Bite where it hits better than Water Gun: on a Psychic or Ghost foe
//  (2x), and on one that resists Water, where Bite's 1x beats Water Gun's
//  0.5x. Against a wild Pokemon as read, and a trainer's party.
UnitTestResult test_bite_wins(){
    const PartyMember wartortle = grown_to(new_member("squirtle", 5), 20);
    for (std::string_view foe : {"abra", "starmie", "gastly", "staryu", "oddish", "dratini"}){
        const size_t slot = wild_turn(wartortle, foe);
        if (slot != BITE){
            return std::format("A wild {}: expected Bite, got slot {}.", foe, slot);
        }
    }
    for (std::string_view foe : {"geodude", "pidgey", "rattata"}){
        const size_t slot = wild_turn(wartortle, foe);
        if (slot != WATER_GUN){
            return std::format("A wild {}: expected Water Gun, got slot {}.", foe, slot);
        }
    }
    if (attack_slot_against(wartortle, {"staryu", "starmie"}) != BITE){
        return "Expected Bite against Misty's party.";
    }
    return true;
}

//  The default risks: the rival and gym leaders HIGH, route trainers,
//  grunts and a battle that names no trainer LOW.
UnitTestResult test_risks(){
    const std::vector<std::pair<std::string_view, BattleRisk>> cases = {
        {"TRAINER_RIVAL_CERULEAN_BULBASAUR", BattleRisk::HIGH},
        {"TRAINER_LEADER_MISTY", BattleRisk::HIGH},
        {"TRAINER_LEADER_BROCK", BattleRisk::HIGH},
        {"TRAINER_LASS_ALI", BattleRisk::LOW},
        {"TRAINER_TEAM_ROCKET_GRUNT", BattleRisk::LOW},
        {"", BattleRisk::LOW},
    };
    for (const auto& [trainer, risk] : cases){
        if (default_battle_risk(trainer) != risk){
            return std::format("\"{}\": expected {}.", trainer, battle_risk_name(risk));
        }
    }
    return true;
}

//  The Tower rival (console B, 2026-11-12, blacked out): Blastoise with
//  Bite, Water Pulse, Bubble and Dig used Bite on everything. Water Pulse
//  where it ties: with Bite against Pidgeotto, with Bubble and Dig against
//  Growlithe; Bite against Kadabra, Gyarados and Ivysaur.
UnitTestResult test_water_pulse_ties(){
    PartyMember blastoise = grown_to(new_member("squirtle", 5), 36);
    blastoise.moves = {{"bite", 25}, {"water-pulse", 20}, {"bubble", 30}, {"dig", 10}};
    constexpr size_t BITE_SLOT = 0;
    constexpr size_t WATER_PULSE_SLOT = 1;
    const std::vector<std::pair<std::string, size_t>> cases = {
        {"pidgeotto", WATER_PULSE_SLOT},
        {"growlithe", WATER_PULSE_SLOT},
        {"kadabra", BITE_SLOT},
        {"gyarados", BITE_SLOT},
        {"ivysaur", BITE_SLOT},
    };
    for (const auto& [foe, expected] : cases){
        const std::optional<size_t> slot = attack_slot_against(blastoise, {foe});
        if (slot != expected){
            return std::format("{}: expected slot {}, got slot {}.", foe, expected, slot ? std::to_string(*slot) : "none");
        }
    }
    return true;
}

//  A wild Pokemon that wasn't read, or a name the data doesn't have, gets
//  the plan's order: Water Gun first.
UnitTestResult test_wild_fallback(){
    const PartyMember wartortle = grown_to(new_member("squirtle", 5), 20);
    for (std::string_view read : {"", "missingno"}){
        if (!wild_foes(read).empty()){
            return std::format("Expected no foes for \"{}\".", read);
        }
        const size_t slot = wild_turn(wartortle, read);
        if (slot != WATER_GUN || attack_slot(wartortle) != WATER_GUN){
            return std::format("A wild \"{}\": expected the plan's Water Gun, got slot {}.", read, slot);
        }
    }
    return true;
}

const AutoStoryBattleDataTestCase AUTO_STORY_BATTLE_DATA_CASES[] = {
    {
        .input = {.name = "type chart, moves and species from pret", .body = test_data},
        .expectation = {.description = "validates decompiled type charts, moves, and species"},
    },
    {
        .input = {.name = "Miguel's Grimer: Water Gun", .body = test_miguel_grimer},
        .expectation = {.description = "verifies move matchup choice against Miguel Grimer"},
    },
    {
        .input = {.name = "Water Pulse on ties at the Tower rival", .body = test_water_pulse_ties},
        .expectation = {.description = "prefers the strongest STAB move when matchups tie"},
    },
    {
        .input = {.name = "Bite on Psychic, Ghost and Water-resistant foes", .body = test_bite_wins},
        .expectation = {.description = "verifies move matchup prioritization for Bite"},
    },
    {
        .input = {.name = "a wild Pokemon not read: the plan's order", .body = test_wild_fallback},
        .expectation = {.description = "falls back to default move preference when unread"},
    },
    {
        .input = {.name = "battle risks by trainer", .body = test_risks},
        .expectation = {.description = "rival and gym leaders HIGH, route trainers and grunts LOW"},
    },
};

} // namespace


void add_tests_AutoStoryBattleData(UnitTestDatabase& database){
    for (const auto& scenario : AUTO_STORY_BATTLE_DATA_CASES){
        database.add<Test_AutoStoryBattleData>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
