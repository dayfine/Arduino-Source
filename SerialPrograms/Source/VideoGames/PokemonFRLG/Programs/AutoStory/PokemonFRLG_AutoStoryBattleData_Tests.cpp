/*  AutoStory Battle Data Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The battle data, and the attacks chosen against trainers' parties and wild Pokemon from it;
 *  a status cured in a trainer's battle.
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

//  Route 21 (console B, 2026-11-13, 12.03): Swimmer Roland's Poliwhirl,
//  Water Absorb, healed off every Surf for 15 turns. No Water move against
//  a Water foe, and no move a foe's ability may make useless (Water Absorb,
//  Volt Absorb, Flash Fire, Levitate), even where it hits 1x (Quagsire)
//  or the foe has another ability too (Growlithe, Lanturn).
UnitTestResult test_useless_moves(){
    struct Case{
        std::string lead;
        std::vector<KnownMove> moves;
        std::string foe;
        size_t expected;
    };
    const std::vector<Case> cases = {
        {"wartortle", {{"tackle", 35}, {"bite", 25}, {"surf", 15}, {"water-gun", 25}}, "poliwhirl", 1},
        {"wartortle", {{"surf", 15}, {"bite", 25}, {"tackle", 35}, {"withdraw", 40}}, "quagsire", 1},
        {"wartortle", {{"surf", 15}, {"water-gun", 25}, {"tackle", 35}, {"withdraw", 40}}, "lapras", 2},
        {"raichu", {{"thunder-shock", 30}, {"tackle", 35}, {"growl", 40}, {"leer", 30}}, "lanturn", 1},
        {"raichu", {{"earthquake", 10}, {"tackle", 35}, {"growl", 40}, {"leer", 30}}, "claydol", 1},
        {"arcanine", {{"ember", 25}, {"tackle", 35}, {"growl", 40}, {"leer", 30}}, "growlithe", 1},
    };
    for (const Case& test : cases){
        const PartyMember lead{.species = test.lead, .moves = test.moves, .moves_through_level = 32, .moves_read = true, .level = 32};
        const std::optional<size_t> slot = attack_slot_against(lead, {test.foe});
        if (slot != test.expected){
            return std::format("{} against {}: expected slot {}, got slot {}.", test.lead, test.foe, test.expected, slot_name(slot));
        }
    }
    return true;
}

//  A trainer the plan doesn't name (Roland, fought on a leg): his Pokemon
//  as read off the HP box is what the attack is chosen against.
UnitTestResult test_unplanned_trainer_foe(){
    Supplies supplies;
    supplies.lead = grown_to(new_member("squirtle", 5), 32);
    supplies.lead.moves = {{"tackle", 35}, {"bite", 25}, {"surf", 15}, {"water-gun", 25}};
    supplies.potions = 5;
    supplies.foe_out = "poliwhirl";
    BattleDecisions trainer(supplies, BattleKind::TRAINER);
    const size_t slot = trainer.turn(GREEN).slot;
    if (slot != 1){
        return std::format("Expected Bite against Roland's Poliwhirl, got slot {}.", slot);
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

//  Victory Road's lead as it met Cooltrainer Colby (console B, 2026-11-16,
//  19.04d): Blastoise Lv51 with Bite, Strength, Surf and Blizzard.
PartyMember victory_road_blastoise(){
    return PartyMember{
        .species = "blastoise",
        .moves = {{"bite", 25}, {"strength", 15}, {"surf", 15}, {"blizzard", 5}},
        .moves_through_level = 51,
        .moves_read = true,
        .level = 51,
    };
}

//  19.04d (console B, 2026-11-16 03:34): against Colby's Water party the
//  lead used Bite, as 1x as Strength and first in the plan's order, until
//  its PP ran out. The best type matchup, then the most damage of the
//  matchups that tie (power, half again for the lead's own type).
UnitTestResult test_matchup_then_power(){
    const PartyMember blastoise = victory_road_blastoise();
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"kingler", "strength"},
        {"tentacruel", "strength"},
        {"blastoise", "strength"},
        {"onix", "surf"},
        {"rhydon", "surf"},
        {"raticate", "surf"},
        {"dragonair", "blizzard"},
        {"pidgeot", "blizzard"},
    };
    for (const auto& [foe, expected] : cases){
        const std::optional<size_t> slot = attack_slot_against(blastoise, {foe});
        const std::string chosen = slot ? blastoise.moves[*slot].move : "none";
        if (chosen != expected){
            return std::format("{}: expected {}, got {}.", foe, expected, chosen);
        }
    }
    return true;
}

//  Lorelei (console B, 2026-11-16, 20.01): after four X Specials the lead
//  used Strength on Dewgong for 11 turns. PokeGuy's Blastoise Lv51
//  (PokemonSimulation docs/battles/pokeguy-conditions.md, lorelei) and her
//  foes' Defense and Sp. Def from the same table.
PartyMember lorelei_blastoise(){
    PartyMember blastoise = victory_road_blastoise();
    blastoise.stats = Stats{.hp = 159, .attack = 107, .defense = 108, .sp_attack = 123, .sp_defense = 124, .speed = 107};
    return blastoise;
}

std::string pick(const PartyMember& member, std::string_view foe, const AttackContext& context){
    const std::optional<size_t> slot = attack_slot_against(member, {std::string(foe)}, context);
    return slot ? member.moves[*slot].move : "none";
}

constexpr FoeStats DEWGONG = {.defense = 103, .sp_defense = 119};

//  Bite (Dark: special in Gen 3) at x3 hits ~83, Strength ~38. Surf would
//  hit harder still, but Water on a Water foe is never chosen.
UnitTestResult test_dewgong_after_setup(){
    const std::string chosen = pick(lorelei_blastoise(), "dewgong", {.sp_attack_stage = 4, .foe = DEWGONG});
    return chosen == "bite" ? UnitTestResult(true) : UnitTestResult(std::format("Expected Bite at +4, got {}.", chosen));
}

//  Strength ~38 against Bite ~29 and Blizzard ~7 (resisted twice, and
//  Thick Fat halves its power).
UnitTestResult test_dewgong_without_setup(){
    const std::string chosen = pick(lorelei_blastoise(), "dewgong", {.foe = DEWGONG});
    return chosen == "strength" ? UnitTestResult(true) : UnitTestResult(std::format("Expected Strength at +0, got {}.", chosen));
}

//  Jynx (Ice/Psychic): Defense 59, Sp. Def 135. Strength ~65, Surf ~57,
//  Bite (2x) ~52: the matchup alone took Bite.
UnitTestResult test_jynx_physical(){
    const std::string chosen = pick(lorelei_blastoise(), "jynx", {.foe = FoeStats{.defense = 59, .sp_defense = 135}});
    return chosen == "strength" ? UnitTestResult(true) : UnitTestResult(std::format("Expected Strength on Jynx, got {}.", chosen));
}

//  A Raticate with Defense and Sp. Def 80 (made up for the case): Surf
//  ~99 at +0 beats Strength ~49; at Attack +3 (x2.5) Strength hits ~119.
UnitTestResult test_x_attack_physical(){
    const FoeStats even{.defense = 80, .sp_defense = 80};
    const std::string plain = pick(lorelei_blastoise(), "raticate", {.foe = even});
    const std::string boosted = pick(lorelei_blastoise(), "raticate", {.attack_stage = 3, .foe = even});
    if (plain != "surf" || boosted != "strength"){
        return std::format("Expected Surf at Attack +0 and Strength at +3, got {} and {}.", plain, boosted);
    }
    return true;
}

//  19.04d (console B, 2026-11-16 03:36): paralysed by Colby's Kingler, the
//  lead fought his four other Pokemon with PAR and 2 Parlyz Heals in the
//  bag. A trainer's battle cures PAR with a Parlyz Heal once the HP needs
//  no Super Potion first; a wild Pokemon is run from instead.
UnitTestResult test_paralysis_cured(){
    struct Input{
        BattleKind kind;
        bool hp_yellow;
    };
    struct Expectation{
        TurnChoice choice;
        std::string item;
    };
    struct Case{
        std::string name;
        Input input;
        Expectation expectation;
    };
    const std::vector<Case> cases = {
        {.name = "a trainer, HP green", .input = {BattleKind::TRAINER, false}, .expectation = {TurnChoice::MEDICINE, "parlyz-heal"}},
        {.name = "a trainer, HP yellow", .input = {BattleKind::TRAINER, true}, .expectation = {TurnChoice::MEDICINE, "super-potion"}},
        {.name = "a wild Pokemon", .input = {BattleKind::WILD_FIGHT, false}, .expectation = {TurnChoice::RUN, ""}},
    };
    for (const Case& c : cases){
        Supplies supplies;
        supplies.lead = victory_road_blastoise();
        supplies.foe_out = "poliwhirl";
        supplies.super_potions = 4;
        supplies.parlyz_heals = 2;
        BattleDecisions decisions(supplies, c.input.kind);
        const PartyStatus paralysed{.hp_seen = true, .hp_yellow = c.input.hp_yellow, .hp_red = false, .hp_fill = 0.5, .status = StatusCondition::PARALYSIS};
        const TurnAction action = decisions.turn(paralysed);
        if (action.choice != c.expectation.choice || action.item != c.expectation.item){
            return std::format("{}: expected \"{}\", got choice {} \"{}\".", c.name, c.expectation.item, static_cast<int>(action.choice), action.item);
        }
    }
    return true;
}

//  A Parlyz Heal used: one fewer in the bag, and the lead counts as cured.
UnitTestResult test_parlyz_heal_used(){
    Supplies supplies;
    supplies.lead = victory_road_blastoise();
    supplies.parlyz_heals = 2;
    BattleDecisions decisions(supplies, BattleKind::TRAINER);
    decisions.turn(PartyStatus{.hp_seen = true, .hp_yellow = false, .hp_red = false, .hp_fill = 0.7, .status = StatusCondition::PARALYSIS});
    decisions.turn_done(TurnChoice::MEDICINE, TurnResult::DONE);
    if (supplies.parlyz_heals != 1 || supplies.status != StatusCondition::NONE){
        return std::format("{} Parlyz Heal(s) left, status {}; expected 1 left and no status.", supplies.parlyz_heals, status_name(supplies.status));
    }
    return true;
}

const AutoStoryBattleDataTestCase AUTO_STORY_BATTLE_DATA_CASES[] = {
    {
        .input = {.name = "Victory Road's Blastoise: the best matchup, then the strongest move", .body = test_matchup_then_power},
        .expectation = {.description = "Strength on Kingler, Tentacruel and Blastoise; Surf on Onix, Rhydon and Raticate; Blizzard on Dragonair and Pidgeot"},
    },
    {
        .input = {.name = "Lorelei's Dewgong at Sp. Atk +4", .body = test_dewgong_after_setup},
        .expectation = {.description = "Bite, the special move Water-on-Water leaves, over Strength"},
    },
    {
        .input = {.name = "Lorelei's Dewgong at Sp. Atk +0", .body = test_dewgong_without_setup},
        .expectation = {.description = "Strength"},
    },
    {
        .input = {.name = "Lorelei's Jynx, Defense 59", .body = test_jynx_physical},
        .expectation = {.description = "Strength over Surf and the super effective Bite"},
    },
    {
        .input = {.name = "X Attack counted for a physical move", .body = test_x_attack_physical},
        .expectation = {.description = "Surf at Attack +0, Strength at +3"},
    },
    {
        .input = {.name = "PAR with Parlyz Heals in the bag", .body = test_paralysis_cured},
        .expectation = {.description = "a Parlyz Heal against a trainer at green HP, a Super Potion first at yellow, RUN from a wild Pokemon"},
    },
    {
        .input = {.name = "a Parlyz Heal used", .body = test_parlyz_heal_used},
        .expectation = {.description = "one fewer Parlyz Heal, the status cured"},
    },
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
        .input = {.name = "no Water move on a Water foe, none an ability makes useless", .body = test_useless_moves},
        .expectation = {.description = "Roland's Poliwhirl (Water Absorb) takes Bite, not Surf"},
    },
    {
        .input = {.name = "an unplanned trainer's Pokemon as read", .body = test_unplanned_trainer_foe},
        .expectation = {.description = "attacks the read foe out without a plan party"},
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
