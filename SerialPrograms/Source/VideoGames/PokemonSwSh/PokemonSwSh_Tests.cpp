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
    size_t caught_count;
    bool expected_accepted;
    std::string expected_boss;
    std::string expected_caught_boss;
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
        const bool accepted = state.infer_boss_from_opponent();
        if (accepted != m_scenario.expected_accepted){
            return std::format("Expected opponent accepted={}, got {}", m_scenario.expected_accepted, accepted);
        }
        if (state.boss != m_scenario.expected_boss){
            return std::format("Expected boss '{}', got '{}'", m_scenario.expected_boss, state.boss);
        }
        const std::string caught = state.caught_boss(m_scenario.caught_count);
        if (caught != m_scenario.expected_caught_boss){
            return std::format("Expected caught boss '{}', got '{}'", m_scenario.expected_caught_boss, caught);
        }
        return true;
    }

private:
    BossStateCase m_scenario;
};

void add_tests_MaxLairBossState(UnitTestDatabase& database){
    const BossStateCase cases[] = {
        {.name = "rental before three wins", .wins = 2, .opponent = {"mimikyu-disguised"}, .boss = "", .caught_count = 3,
            .expected_accepted = true, .expected_boss = "", .expected_caught_boss = ""},
        {.name = "Mimikyu at boss stage", .wins = 3, .opponent = {"mimikyu-disguised"}, .boss = "", .caught_count = 3,
            .expected_accepted = true, .expected_boss = "mimikyu-disguised", .expected_caught_boss = "mimikyu-disguised"},
        {.name = "Kyurem after Mimikyu misidentification", .wins = 3, .opponent = {"kyurem"}, .boss = "mimikyu-disguised", .caught_count = 3,
            .expected_accepted = true, .expected_boss = "mimikyu-disguised", .expected_caught_boss = "mimikyu-disguised"},
        {.name = "identified boss lost", .wins = 3, .opponent = {"kyurem"}, .boss = "kyurem", .caught_count = 3,
            .expected_accepted = true, .expected_boss = "kyurem", .expected_caught_boss = "kyurem"},
        {.name = "identified boss caught", .wins = 3, .opponent = {"kyurem"}, .boss = "", .caught_count = 4,
            .expected_accepted = true, .expected_boss = "kyurem", .expected_caught_boss = "kyurem"},
        {.name = "boss before three wins must retry", .wins = 2, .opponent = {"kyurem"}, .boss = "", .caught_count = 2,
            .expected_accepted = false, .expected_boss = "", .expected_caught_boss = ""},
        {.name = "empty boss-stage read", .wins = 3, .opponent = {}, .boss = "", .caught_count = 3,
            .expected_accepted = true, .expected_boss = "", .expected_caught_boss = ""},
        {.name = "ambiguous boss-stage read", .wins = 3, .opponent = {"kyurem", "mimikyu-disguised"}, .boss = "", .caught_count = 3,
            .expected_accepted = true, .expected_boss = "kyurem", .expected_caught_boss = "kyurem"},
        {.name = "rental read after known boss", .wins = 3, .opponent = {"mimikyu-disguised"}, .boss = "kyurem", .caught_count = 3,
            .expected_accepted = true, .expected_boss = "kyurem", .expected_caught_boss = "kyurem"},
        {.name = "four catches after rental boss", .wins = 3, .opponent = {"mimikyu-disguised"}, .boss = "mimikyu-disguised", .caught_count = 4,
            .expected_accepted = true, .expected_boss = "mimikyu-disguised", .expected_caught_boss = "mimikyu-disguised"},
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
