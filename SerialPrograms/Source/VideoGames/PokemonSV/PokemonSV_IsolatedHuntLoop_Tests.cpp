#include "Programs/ShinyHunting/PokemonSV_IsolatedHuntLoop.h"
#include "PokemonSV_IsolatedHuntLoop_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

struct LoopCase{
    std::string name;
    std::vector<IsolatedHuntRound> rounds = {};
    IsolatedHuntLimits limits{.max_rounds = 2};
    bool prepare_ok = true;
    bool travel_ok = true;
    bool refresh_ok = true;
    bool reanchor_ok = true;
    bool acquired = false;
    IsolatedHuntBattleResult battle = IsolatedHuntBattleResult::STOP;
    IsolatedHuntResult expected;
    std::string steps;
};

class ScriptedHunt : public IsolatedHuntActions{
public:
    ScriptedHunt(const LoopCase& test) : m_test(test){}
    std::string steps;
    virtual bool prepare_sandwich(const IsolatedHuntRequest&) override{ steps += "sandwich "; return m_test.prepare_ok; }
    virtual bool travel_to_anchor(const IsolatedHuntRequest&) override{
        steps += "travel ";
        return m_travel++ == 0 ? m_test.travel_ok : m_test.reanchor_ok;
    }
    virtual IsolatedHuntRound clear_round(const IsolatedHuntRequest&) override{
        steps += "clear ";
        m_clock += 1000;
        return m_next < m_test.rounds.size() ? m_test.rounds[m_next++] : IsolatedHuntRound{.kills = 2};
    }
    virtual bool refresh_spawns(const IsolatedHuntRequest&) override{ steps += "refresh "; return m_test.refresh_ok; }
    virtual bool acquire_refused_target(const IsolatedHuntRequest&) override{ steps += "acquire "; return m_test.acquired; }
    virtual IsolatedHuntBattleResult handle_battle(const IsolatedHuntRequest& request) override{
        steps += "battle:" + request.ball + ":" + std::to_string(request.ball_limit) + " ";
        return m_test.battle;
    }
    virtual uint64_t now_ms() const override{ return m_clock; }
private:
    const LoopCase& m_test;
    size_t m_next = 0;
    size_t m_travel = 0;
    uint64_t m_clock = 0;
};

std::string describe(const IsolatedHuntResult& result){
    return "stop=" + std::to_string((int)result.stop) + ", kills=" + std::to_string(result.kills) +
        ", rounds=" + std::to_string(result.rounds) + ", encounters=" + std::to_string(result.encounters) +
        ", refusals=" + std::to_string(result.refusals) + ", refreshes=" + std::to_string(result.refreshes);
}

std::vector<LoopCase> clearing_cases(){
    return {
        {
            .name = "Sandwich before travel and bounded clearing",
            .expected = {.stop = IsolatedHuntStop::ROUND_LIMIT, .kills = 4, .rounds = 2},
            .steps = "sandwich travel clear clear ",
        },
        {
            .name = "Unresolved refusal never despawns target",
            .rounds = {{.event = IsolatedHuntEvent::REFUSAL}},
            .expected = {.stop = IsolatedHuntStop::UNRESOLVED_REFUSAL, .rounds = 1, .refusals = 1},
            .steps = "sandwich travel clear acquire ",
        },
    };
}

std::vector<LoopCase> battle_cases(){
    return {
        {
            .name = "Battle policy stops with configured ball",
            .rounds = {{.event = IsolatedHuntEvent::BATTLE}},
            .expected = {.stop = IsolatedHuntStop::BATTLE_POLICY, .rounds = 1, .encounters = 1},
            .steps = "sandwich travel clear battle:luxury-ball:15 ",
        },
        {
            .name = "Refusal acquisition delegates battle handling",
            .rounds = {{.event = IsolatedHuntEvent::REFUSAL}}, .acquired = true,
            .expected = {.stop = IsolatedHuntStop::BATTLE_POLICY, .rounds = 1, .encounters = 1, .refusals = 1},
            .steps = "sandwich travel clear acquire battle:luxury-ball:15 ",
        },
        {
            .name = "Reanchor failure prevents more clearing",
            .rounds = {{.event = IsolatedHuntEvent::BATTLE}}, .reanchor_ok = false,
            .battle = IsolatedHuntBattleResult::CONTINUE,
            .expected = {.stop = IsolatedHuntStop::TRAVEL_FAILED, .rounds = 1, .encounters = 1},
            .steps = "sandwich travel clear battle:luxury-ball:15 travel ",
        },
        {
            .name = "Handled battle requires reanchor before more clearing",
            .rounds = {{.event = IsolatedHuntEvent::BATTLE}}, .battle = IsolatedHuntBattleResult::CONTINUE,
            .expected = {.stop = IsolatedHuntStop::ROUND_LIMIT, .kills = 2, .rounds = 2, .encounters = 1},
            .steps = "sandwich travel clear battle:luxury-ball:15 travel clear ",
        },
    };
}

std::vector<LoopCase> limit_cases(){
    return {
        {
            .name = "Time limit checked between rounds", .limits = {.max_rounds = 2, .duration_ms = 500},
            .expected = {.stop = IsolatedHuntStop::TIME_LIMIT, .kills = 2, .rounds = 1},
            .steps = "sandwich travel clear ",
        },
        {
            .name = "Empty rounds refresh once then stop at bound",
            .rounds = {{.kills = 0}, {.kills = 0}}, .limits = {.max_rounds = 5, .max_empty_rounds = 2},
            .expected = {.stop = IsolatedHuntStop::NO_PROGRESS, .rounds = 2, .refreshes = 1},
            .steps = "sandwich travel clear refresh clear ",
        },
        {
            .name = "Refresh failure does not resume", .rounds = {{.kills = 0}}, .refresh_ok = false,
            .expected = {.stop = IsolatedHuntStop::REFRESH_FAILED, .rounds = 1},
            .steps = "sandwich travel clear refresh ",
        },
        {
            .name = "Final round does not waste a refresh", .rounds = {{.kills = 0}}, .limits = {.max_rounds = 1},
            .expected = {.stop = IsolatedHuntStop::ROUND_LIMIT, .rounds = 1},
            .steps = "sandwich travel clear ",
        },
        {
            .name = "Expired time budget prevents refresh", .rounds = {{.kills = 0}},
            .limits = {.max_rounds = 2, .duration_ms = 500},
            .expected = {.stop = IsolatedHuntStop::TIME_LIMIT, .rounds = 1},
            .steps = "sandwich travel clear ",
        },
        {
            .name = "Kills reset consecutive empty rounds", .rounds = {{.kills = 0}, {.kills = 2}, {.kills = 0}},
            .limits = {.max_rounds = 3, .max_empty_rounds = 2},
            .expected = {.stop = IsolatedHuntStop::ROUND_LIMIT, .kills = 2, .rounds = 3, .refreshes = 1},
            .steps = "sandwich travel clear refresh clear clear ",
        },
        {
            .name = "Zero round budget rejected before ingredients", .limits = {.max_rounds = 0},
            .expected = {.stop = IsolatedHuntStop::INVALID_PLAN}, .steps = "",
        },
    };
}

std::vector<LoopCase> setup_cases(){
    return {
        {
            .name = "Sandwich failure prevents travel", .prepare_ok = false,
            .expected = {.stop = IsolatedHuntStop::SETUP_FAILED}, .steps = "sandwich ",
        },
        {
            .name = "Travel failure prevents clearing", .travel_ok = false,
            .expected = {.stop = IsolatedHuntStop::TRAVEL_FAILED}, .steps = "sandwich travel ",
        },
        {
            .name = "Battle failure prevents travel or clearing", .rounds = {{.event = IsolatedHuntEvent::BATTLE}},
            .battle = IsolatedHuntBattleResult::FAILED,
            .expected = {.stop = IsolatedHuntStop::BATTLE_FAILED, .rounds = 1, .encounters = 1},
            .steps = "sandwich travel clear battle:luxury-ball:15 ",
        },
    };
}

std::vector<LoopCase> loop_cases(){
    auto tests = clearing_cases();
    const auto battles = battle_cases();
    tests.insert(tests.end(), battles.begin(), battles.end());
    const auto limits = limit_cases();
    tests.insert(tests.end(), limits.begin(), limits.end());
    const auto setup = setup_cases();
    tests.insert(tests.end(), setup.begin(), setup.end());
    return tests;
}

}

UnitTestResult test_isolated_hunt_loop(){
    const IsolatedHuntRequest request{
        .pokemon = "charcadet", .type = IsolatedEncounterType::FIRE,
        .location = IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL,
        .ball = "luxury-ball", .ball_limit = 15,
    };
    for (const auto& test : loop_cases()){
        ScriptedHunt actions(test);
        const auto result = run_isolated_hunt(request, test.limits, actions);
        if (describe(result) != describe(test.expected) || actions.steps != test.steps){
            return test.name + ": expected " + describe(test.expected) + " [" + test.steps +
                "]; read " + describe(result) + " [" + actions.steps + "]";
        }
    }
    return true;
}

}
}
}
