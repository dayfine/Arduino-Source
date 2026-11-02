/*  Pokemon Sword/Shield Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "VideoGames/PokemonSwSh/MaxLair/Framework/PokemonSwSh_MaxLair_State.h"
#include "VideoGames/PokemonSwSh/Inference/PokemonSwSh_DialogBoxDetector.h"
#include "VideoGames/PokemonSwSh/Inference/PokemonSwSh_DialogTriangleDetector.h"
#include "VideoGames/PokemonSwSh/Inference/PokemonSwSh_BoxGenderDetector.h"
#include "VideoGames/PokemonSwSh/Inference/PokemonSwSh_BoxShinySymbolDetector.h"
#include "VideoGames/PokemonSwSh/Inference/PokemonSwSh_MainMenuDetector.h"
#include "VideoGames/PokemonSwSh/Inference/PokemonSwSh_YCommDetector.h"
#include "VideoGames/PokemonSwSh/Inference/PokemonSwSh_SelectionArrowFinder.h"
#include "VideoGames/PokemonSwSh/MaxLair/Inference/PokemonSwSh_MaxLair_Detect_BattleMenu.h"
#include "PokemonSwSh_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{



namespace{
using MaxLairInternal::GlobalState;

struct BossStateCase{
    std::string name;
    uint8_t wins;
    std::set<std::string> opponent;
    std::string boss;
    uint8_t final_wins;     //  wins on the caught screen
    bool expected_accepted;
    std::string expected_boss;
    std::string expected_boss_if_caught;
};

class Test_MaxLairBossState : public UnitTest{
public:
    explicit Test_MaxLairBossState(BossStateCase scenario)
        : UnitTest("PokemonSwSh::MaxLairBossState - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        GlobalState state;
        state.wins = m_scenario.wins;
        state.opponent = m_scenario.opponent;
        state.boss = m_scenario.boss;
        const bool accepted = state.accept_opponent();
        if (accepted != m_scenario.expected_accepted){
            return std::format("Expected opponent accepted={}, got {}", m_scenario.expected_accepted, accepted);
        }
        if (state.boss != m_scenario.expected_boss){
            return std::format("Expected boss '{}', got '{}'", m_scenario.expected_boss, state.boss);
        }
        state.wins = m_scenario.final_wins;
        const std::string caught = state.boss_if_caught();
        if (caught != m_scenario.expected_boss_if_caught){
            return std::format("Expected boss_if_caught '{}', got '{}'", m_scenario.expected_boss_if_caught, caught);
        }
        return true;
    }

private:
    BossStateCase m_scenario;
};

void add_tests_MaxLairBossState(UnitTestDatabase& database){
    //  Cases marked BUG record main's current, wrong behaviour; the next PR (#172) fixes them.
    const BossStateCase cases[] = {
        {.name = "rental before three wins", .wins = 2, .opponent = {"mimikyu-disguised"}, .boss = "", .final_wins = 3,
            .expected_accepted = true, .expected_boss = "", .expected_boss_if_caught = ""},
        //  BUG: main accepts a rental (Mimikyu) as the boss and reports it caught; the next PR (#172) makes this retry.
        {.name = "Mimikyu at boss stage", .wins = 3, .opponent = {"mimikyu-disguised"}, .boss = "", .final_wins = 3,
            .expected_accepted = true, .expected_boss = "mimikyu-disguised", .expected_boss_if_caught = "mimikyu-disguised"},
        //  BUG: main keeps the rental it misread as the boss after the real boss is read; #172 replaces it with Kyurem.
        {.name = "Kyurem after Mimikyu misidentification", .wins = 3, .opponent = {"kyurem"}, .boss = "mimikyu-disguised", .final_wins = 3,
            .expected_accepted = true, .expected_boss = "mimikyu-disguised", .expected_boss_if_caught = "mimikyu-disguised"},
        //  BUG: main reports the boss as caught although the run ended without a fourth win; #172 reports no catch.
        {.name = "identified boss lost", .wins = 3, .opponent = {"kyurem"}, .boss = "kyurem", .final_wins = 3,
            .expected_accepted = true, .expected_boss = "kyurem", .expected_boss_if_caught = "kyurem"},
        {.name = "identified boss caught", .wins = 3, .opponent = {"kyurem"}, .boss = "", .final_wins = 4,
            .expected_accepted = true, .expected_boss = "kyurem", .expected_boss_if_caught = "kyurem"},
        {.name = "boss before three wins must retry", .wins = 2, .opponent = {"kyurem"}, .boss = "", .final_wins = 2,
            .expected_accepted = false, .expected_boss = "", .expected_boss_if_caught = ""},
        //  BUG: main accepts a boss stage where no opponent was read; #172 makes this retry.
        {.name = "empty boss-stage read", .wins = 3, .opponent = {}, .boss = "", .final_wins = 3,
            .expected_accepted = true, .expected_boss = "", .expected_boss_if_caught = ""},
        //  BUG: main accepts a two-candidate read and takes the first as the boss; #172 makes this retry.
        {.name = "ambiguous boss-stage read", .wins = 3, .opponent = {"kyurem", "mimikyu-disguised"}, .boss = "", .final_wins = 3,
            .expected_accepted = true, .expected_boss = "kyurem", .expected_boss_if_caught = "kyurem"},
        //  BUG: main accepts a rental read at the boss stage and reports the boss caught without a fourth win; #172 makes this retry and reports no catch.
        {.name = "rental read after known boss", .wins = 3, .opponent = {"mimikyu-disguised"}, .boss = "kyurem", .final_wins = 3,
            .expected_accepted = true, .expected_boss = "kyurem", .expected_boss_if_caught = "kyurem"},
        //  BUG: main accepts a rental read and reports the rental as the caught boss; #172 makes this retry and reports no catch.
        {.name = "fourth win after rental boss", .wins = 3, .opponent = {"mimikyu-disguised"}, .boss = "mimikyu-disguised", .final_wins = 4,
            .expected_accepted = true, .expected_boss = "mimikyu-disguised", .expected_boss_if_caught = "mimikyu-disguised"},
    };
    for (const BossStateCase& scenario : cases){
        database.add<Test_MaxLairBossState>(scenario);
    }
}
}

void add_tests(UnitTestDatabase& database){
    add_tests_MaxLairBossState(database);
    NintendoSwitch::PokemonSwSh::add_tests_DialogBoxDetector(database);
    NintendoSwitch::PokemonSwSh::add_tests_DialogTriangleDetector(database);
    NintendoSwitch::PokemonSwSh::add_tests_BoxGenderDetector(database);
    NintendoSwitch::PokemonSwSh::add_tests_BoxShinySymbolDetector(database);
    NintendoSwitch::PokemonSwSh::add_tests_PartyMenuDetector(database);
    NintendoSwitch::PokemonSwSh::add_tests_YCommDetector(database);
    NintendoSwitch::PokemonSwSh::add_tests_SelectionArrowFinder(database);
    NintendoSwitch::PokemonSwSh::MaxLairInternal::add_tests_MaxLairBattleMenuDetector(database);
}



}
}
}
