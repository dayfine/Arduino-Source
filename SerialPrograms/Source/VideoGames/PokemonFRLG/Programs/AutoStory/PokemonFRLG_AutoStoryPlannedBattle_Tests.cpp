/*  AutoStory Planned Battle Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League foes data as built into the program, and the planned
 *  policy's state from it: damage rolls checked against @smogon/calc 0.12
 *  (PokemonSimulation's engine) with the same raw stats.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryLeagueFoes.h"
#include "PokemonFRLG_AutoStoryPlannedBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Our Lv54 Blastoise (PokemonSimulation chain.py LV54), knowing the League plan's moves.
PartyMember blastoise(){
    return PartyMember{
        .species = "blastoise",
        .moves = {{"surf", 15}, {"bite", 25}, {"blizzard", 5}, {"strength", 15}},
        .level = 54,
        .stats = Stats{.hp = 171, .attack = 104, .defense = 134, .sp_attack = 118, .sp_defense = 149, .speed = 117},
    };
}

const LeagueFoeData& foe_of(std::string_view trainer, size_t slot){
    const auto it = league_foes_data().trainers.find(trainer);
    if (it == league_foes_data().trainers.end() || slot >= it->second.party.size()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No {} slot {} in the League foes.", trainer, slot));
    }
    return it->second.party[slot];
}

std::vector<int> damage_of(const std::vector<PlannedMove>& moves, std::string_view move){
    for (const PlannedMove& planned : moves){
        if (planned.id == move){
            return planned.damage;
        }
    }
    return {};
}


struct FoesCase{
    std::string name = {};
    struct Input{
        std::string trainer = {};
        size_t slot = 0;
    } input = {};
    struct Expectation{
        std::string species = {};
        int level = 0;
        int hp = 0;
        int full_restores = 0;
    } expectation = {};
};

class Test_LeagueFoes : public UnitTest{
public:
    explicit Test_LeagueFoes(FoesCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedBattle - League foes: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const LeagueFoeData& foe = foe_of(m_case.input.trainer, m_case.input.slot);
        const auto& items = league_foes_data().trainers.find(m_case.input.trainer)->second.items;
        const auto restores = items.find("full-restore");
        const int carried = restores == items.end() ? 0 : restores->second;
        const FoesCase::Expectation& want = m_case.expectation;
        if (foe.species != want.species || foe.level != want.level || foe.stats.hp != want.hp || carried != want.full_restores){
            return std::format("{} Lv{} {} HP, {} Full Restores.", foe.species, foe.level, foe.stats.hp, carried);
        }
        return true;
    }

private:
    FoesCase m_case;
};

const FoesCase FOES_CASES[] = {
    {
        .name = "Lorelei's lead",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LORELEI", .slot = 0},
        .expectation = {.species = "dewgong", .level = 52, .hp = 171, .full_restores = 2},
    },
    {
        .name = "Lance's lead",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 0},
        .expectation = {.species = "gyarados", .level = 56, .hp = 189, .full_restores = 2},
    },
    {
        .name = "the Champion's last",
        .input = {.trainer = "TRAINER_CHAMPION_FIRST_BULBASAUR", .slot = 5},
        .expectation = {.species = "venusaur", .level = 63, .hp = 193, .full_restores = 4},
    },
};


struct DamageCase{
    std::string name = {};
    struct Input{
        std::string trainer = {};
        size_t slot = 0;
        PlannedStages stages = {};
        //  Ours or the foe's.
        bool ours = true;
        std::string move = {};
    } input = {};
    std::vector<int> expectation = {};
};

class Test_PlannedDamage : public UnitTest{
public:
    explicit Test_PlannedDamage(DamageCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryPlannedBattle - damage: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const PlannedInputs inputs{
            .lead = blastoise(),
            .hp = 171,
            .stages = m_case.input.stages,
            .foe = {.foe = foe_of(m_case.input.trainer, m_case.input.slot), .slot = m_case.input.slot},
        };
        const PlannedState state = planned_state(inputs, league_foes_data());
        const std::vector<int> got = damage_of(m_case.input.ours ? state.own.moves : state.foe.moves, m_case.input.move);
        if (got != m_case.expectation){
            return std::format("Rolls {}..{} ({}).", got.empty() ? 0 : got.front(), got.empty() ? 0 : got.back(), got.size());
        }
        return true;
    }

private:
    DamageCase m_case;
};

const DamageCase DAMAGE_CASES[] = {
    {
        .name = "Lance's Gyarados's Hyper Beam on us",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 0, .ours = false, .move = "hyper-beam"},
        .expectation = {68, 68, 69, 70, 71, 72, 72, 73, 74, 75, 76, 76, 77, 78, 79, 80},
    },
    {
        .name = "our Surf at +2 on Lance's Dragonite",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 4, .stages = {.spa = 2}, .move = "surf"},
        .expectation = {45, 46, 46, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 52, 53, 54},
    },
    {
        .name = "our Blizzard on Lance's Dragonite",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 4, .move = "blizzard"},
        .expectation = {159, 161, 163, 165, 167, 169, 171, 172, 174, 176, 178, 180, 182, 184, 186, 188},
    },
    {
        .name = "our Strength at -1 (Intimidate) on Lance's Gyarados",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 0, .stages = {.attack = -1}, .move = "strength"},
        .expectation = {22, 23, 23, 23, 24, 24, 24, 24, 25, 25, 25, 25, 26, 26, 26, 27},
    },
    {
        .name = "Dragonair's Dragon Rage on us",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LANCE", .slot = 1, .ours = false, .move = "dragon-rage"},
        .expectation = {40},
    },
    {
        .name = "our Surf on Lorelei's Lapras (Water Absorb)",
        .input = {.trainer = "TRAINER_ELITE_FOUR_LORELEI", .slot = 4, .move = "surf"},
        .expectation = {},
    },
};

} // namespace


void add_tests_AutoStoryPlannedBattle(UnitTestDatabase& database){
    for (const FoesCase& test_case : FOES_CASES){
        database.add<Test_LeagueFoes>(test_case);
    }
    for (const DamageCase& test_case : DAMAGE_CASES){
        database.add<Test_PlannedDamage>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
