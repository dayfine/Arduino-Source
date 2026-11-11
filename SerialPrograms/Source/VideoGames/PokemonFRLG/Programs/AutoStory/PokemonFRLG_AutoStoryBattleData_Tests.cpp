/*  AutoStory Battle Data Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The generated battle data, the trainer each story battle fights, and the
 *  attacks chosen against trainers' parties and wild Pokemon from it.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string_view>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "PokemonFRLG_AutoStoryBattleSim.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryBattleDataInput{
    std::string name;
    UnitTestResult (*body)();
};

struct AutoStoryBattleDataExpectation{
    const char* description;
};

struct AutoStoryBattleDataTestCase{
    AutoStoryBattleDataInput input;
    AutoStoryBattleDataExpectation expectation;
};

class Test_AutoStoryBattleData : public UnitTest{
public:
    explicit Test_AutoStoryBattleData(AutoStoryBattleDataTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStoryBattleData - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_scenario.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
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


//  Spot checks against pret: the type chart, Bite, and species' types and
//  stats.
UnitTestResult test_data(){
    if (type_effectiveness(PokemonType::WATER, PokemonType::FIRE) != 20
        || type_effectiveness(PokemonType::DARK, PokemonType::PSYCHIC) != 20
        || type_effectiveness(PokemonType::NORMAL, PokemonType::GHOST) != 0
        || type_effectiveness(PokemonType::WATER, PokemonType::NORMAL) != 10
    ){
        return "Expected Water 2x on Fire, Dark 2x on Psychic, Normal 0x on Ghost, Water 1x on Normal.";
    }
    const MoveData* bite = move_data("bite");
    if (bite == nullptr || bite->power != 60 || bite->type != PokemonType::DARK || bite->pp != 25 || !special_type(bite->type)){
        return "Expected Bite: 60 power, Dark (special in Gen 3), 25 PP.";
    }
    const SpeciesData* starmie = species_data("starmie");
    if (starmie == nullptr || starmie->first != PokemonType::WATER || starmie->second != PokemonType::PSYCHIC || starmie->base.sp_attack != 100){
        return "Expected Starmie: Water, Psychic, base Sp. Atk 100.";
    }
    if (battle_species().size() != 386 || species_data("farfetch-d") == nullptr || species_data("missingno") != nullptr){
        return std::format("Expected the 386 species by the program's slugs, got {}.", battle_species().size());
    }
    return true;
}

//  The story's scripted wild battles, which no trainer fights: the battle
//  step's map and name, and the species (pret setwildbattle). The battle
//  step fights them as any battle with no trainer.
struct StoryWildBattle{
    std::string_view map;
    std::string_view battle;
    std::string_view species;
};
constexpr StoryWildBattle STORY_WILD_BATTLES[] = {
    //  PokemonTower_6F_EventScript_MarowakGhost: the Silph Scope shows it.
    {.map = "PokemonTower_6F", .battle = "marowak_ghost", .species = "marowak"},
};

std::optional<std::string_view> story_wild_battle(std::string_view map, std::string_view battle){
    for (const StoryWildBattle& entry : STORY_WILD_BATTLES){
        if (entry.map == map && entry.battle == battle){
            return entry.species;
        }
    }
    return std::nullopt;
}

//  Every battle step in the table fights a trainer its map battles in
//  pret's scripts, with a party.
UnitTestResult test_story_trainers(){
    size_t battles = 0;
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
            if (step.kind != StoryStepKind::BATTLE){
                continue;
            }
            battles++;
            const std::string map = step.pose_after ? step.pose_after->map : "";
            const std::optional<std::string_view> trainer = story_trainer(map, step.name);
            if (!trainer && story_wild_battle(map, step.name)){
                if (!species_data(*story_wild_battle(map, step.name))){
                    return std::format("{}, {} on {}: no such species.", checkpoint.id, step.name, map);
                }
                continue;
            }
            if (!trainer){
                return std::format("{}, {} on {}: no trainer.", checkpoint.id, step.name, map);
            }
            const std::vector<std::string> on_map = map_trainers(map);
            if (std::find(on_map.begin(), on_map.end(), *trainer) == on_map.end()){
                return std::format("{} on {}: {} isn't one of the map's trainers.", step.name, map, *trainer);
            }
            if (trainer_party(*trainer).empty()){
                return std::format("{} on {}: {} has no party.", step.name, map, *trainer);
            }
        }
    }
    if (battles == 0){
        return "Expected battle steps in the table.";
    }
    if (trainer_party(*story_trainer("CeruleanCity_Gym", "misty")) != std::vector<std::string>{"staryu", "starmie"}){
        return "Expected Misty's Staryu and Starmie.";
    }
    if (story_wild_battle("PokemonTower_6F", "marowak_ghost") != "marowak" || story_trainer("PokemonTower_6F", "marowak_ghost")){
        return "Expected the Tower's Marowak as a wild battle, no trainer's.";
    }
    return true;
}

//  The Rocket Hideout's battles, which no table row fights yet (the
//  09.01 and 09.02 drafts): each names its map's trainer, with pret's
//  party.
UnitTestResult test_hideout_trainers(){
    struct Expected{
        std::string map;
        std::string battle;
        std::vector<std::string> party;
    };
    const Expected EXPECTED[] = {
        {"CeladonCity_GameCorner", "rocket_grunt", {"raticate", "zubat"}},
        {"RocketHideout_B4F", "lift_key_grunt", {"koffing", "zubat"}},
        {"RocketHideout_B4F", "left_guard", {"sandshrew", "ekans", "sandslash"}},
        {"RocketHideout_B4F", "right_guard", {"ekans", "sandshrew", "arbok"}},
        {"RocketHideout_B4F", "giovanni", {"onix", "rhyhorn", "kangaskhan"}},
    };
    for (const Expected& expected : EXPECTED){
        const std::optional<std::string_view> trainer = story_trainer(expected.map, expected.battle);
        if (!trainer){
            return std::format("{} on {}: no trainer.", expected.battle, expected.map);
        }
        const std::vector<std::string> on_map = map_trainers(expected.map);
        if (std::find(on_map.begin(), on_map.end(), *trainer) == on_map.end()){
            return std::format("{} on {}: {} isn't one of the map's trainers.", expected.battle, expected.map, *trainer);
        }
        if (trainer_party(*trainer) != expected.party){
            return std::format("{} on {}: {} hasn't the expected party.", expected.battle, expected.map, *trainer);
        }
    }
    return true;
}

//  Mt. Moon: Super Nerd Miguel's Grimer (Poison) takes Water Gun, with or
//  without Bite known: Bite (60, Dark) ties Water Gun (40 x 1.5 STAB), both
//  special, and the tie goes to the plan's Water Gun.
UnitTestResult test_miguel_grimer(){
    const std::optional<std::string_view> miguel = story_trainer("MtMoon_B2F", "super_nerd_miguel");
    if (!miguel || trainer_party(*miguel) != std::vector<std::string>{"grimer", "voltorb", "koffing"}){
        return "Expected Miguel's Grimer, Voltorb and Koffing (pret's sParty_SuperNerdMiguel).";
    }
    const std::vector<PartyMember> leads{grown_to(new_member("squirtle", 5), 17), grown_to(new_member("squirtle", 5), 20)};
    for (const PartyMember& lead : leads){
        for (const std::vector<std::string>& foes : {std::vector<std::string>{"grimer"}, trainer_party(*miguel)}){
            const std::optional<size_t> slot = attack_slot_against(lead, foes);
            if (slot != WATER_GUN){
                return std::format("{} against {} foes: expected Water Gun, got slot {}.", lead.species, foes.size(), slot_name(slot));
            }
        }
    }
    return true;
}

//  Bite where it does more than Water Gun: on a Psychic or Ghost foe (2x),
//  and on one that resists Water, where Bite's 1x beats Water Gun's
//  0.5 x 1.5 STAB. Against a wild Pokemon as read, and a trainer's party.
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
    if (attack_slot_against(wartortle, trainer_party(*story_trainer("CeruleanCity_Gym", "misty"))) != BITE){
        return "Expected Bite against Misty's party.";
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
        .input = {.name = "every story battle fights a trainer with a party", .body = test_story_trainers},
        .expectation = {.description = "ensures all scripted story battles encounter valid teams"},
    },
    {
        .input = {.name = "the Rocket Hideout's Grunts and Giovanni", .body = test_hideout_trainers},
        .expectation = {.description = "names each hideout battle's trainer and party"},
    },
    {
        .input = {.name = "Miguel's Grimer: Water Gun", .body = test_miguel_grimer},
        .expectation = {.description = "verifies move matchup choice against Miguel Grimer"},
    },
    {
        .input = {.name = "Bite on Psychic, Ghost and Water-resistant foes", .body = test_bite_wins},
        .expectation = {.description = "verifies move matchup prioritization for Bite"},
    },
    {
        .input = {.name = "a wild Pokemon not read: the plan's order", .body = test_wild_fallback},
        .expectation = {.description = "falls back to default move preference when unread"},
    },
};

} // namespace


void add_tests_AutoStoryBattleData(UnitTestDatabase& database){
    for (const auto& scenario : AUTO_STORY_BATTLE_DATA_CASES){
        database.add<Test_AutoStoryBattleData>(scenario);
    }
    add_tests_AutoStoryBattleSim(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
