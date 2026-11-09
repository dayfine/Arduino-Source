/*  BDSP AutoStory - Wild battle escape policy tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonBDSP_AutoStory_WildBattlePolicy.h"
#include "PokemonBDSP_AutoStory_WildBattlePolicy_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  Scripted game steps: each run() returns the next outcome, and the last
//  one again once they run out.
class ScriptedEscapeActions : public WildBattleEscapeActions{
public:
    explicit ScriptedEscapeActions(std::vector<WildRunOutcome> outcomes)
        : m_outcomes(std::move(outcomes))
    {}
    virtual WildRunOutcome run() override{
        const WildRunOutcome outcome = m_outcomes[std::min(m_runs, m_outcomes.size() - 1)];
        m_runs++;
        return outcome;
    }
    virtual void fight() override{
        m_fights++;
    }
    virtual void note(const std::string&) override{}

    size_t runs() const{
        return m_runs;
    }
    size_t fights() const{
        return m_fights;
    }

private:
    std::vector<WildRunOutcome> m_outcomes;
    size_t m_runs = 0;
    size_t m_fights = 0;
};

const char* result_name(WildEscapeResult result){
    switch (result){
    case WildEscapeResult::ESCAPED:
        return "ESCAPED";
    case WildEscapeResult::FOUGHT:
        return "FOUGHT";
    case WildEscapeResult::UNCONFIRMED:
        return "UNCONFIRMED";
    }
    return "?";
}

struct EscapePolicyInput{
    std::string name;
    std::vector<WildRunOutcome> outcomes;
    WildBattleEscapePolicy policy = {};
};
struct EscapePolicyExpectation{
    WildEscapeResult result = WildEscapeResult::ESCAPED;
    size_t runs = 0;
    size_t fights = 0;
};
struct EscapePolicyCase{
    EscapePolicyInput input;
    EscapePolicyExpectation expectation;
};

class Test_WildBattlePolicy : public UnitTest{
public:
    explicit Test_WildBattlePolicy(EscapePolicyCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Wild battle escape policy: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ScriptedEscapeActions actions(m_case.input.outcomes);
        const WildEscapeResult result = escape_wild_battle(actions, m_case.input.policy);
        const EscapePolicyExpectation& expected = m_case.expectation;
        if (result != expected.result || actions.runs() != expected.runs || actions.fights() != expected.fights){
            return std::format("expected {} after {} Run presses and {} fights, got {} after {} and {}.",
                result_name(expected.result), expected.runs, expected.fights,
                result_name(result), actions.runs(), actions.fights());
        }
        return true;
    }

private:
    EscapePolicyCase m_case;
};

constexpr WildRunOutcome ESCAPED = WildRunOutcome::ESCAPED;
constexpr WildRunOutcome TRAPPED = WildRunOutcome::TRAPPED;
constexpr WildRunOutcome UNCERTAIN = WildRunOutcome::UNCERTAIN;

const EscapePolicyCase WILD_BATTLE_POLICY_CASES[] = {
    {
        .input = {.name = "the first Run works", .outcomes = {ESCAPED}},
        .expectation = {.result = WildEscapeResult::ESCAPED, .runs = 1},
    },
    {
        .input = {.name = "text still scrolling twice, then away", .outcomes = {UNCERTAIN, UNCERTAIN, ESCAPED}},
        .expectation = {.result = WildEscapeResult::ESCAPED, .runs = 3},
    },
    {
        .input = {.name = "two refusals, then the third Run works", .outcomes = {TRAPPED, TRAPPED, ESCAPED}},
        .expectation = {.result = WildEscapeResult::ESCAPED, .runs = 3},
    },
    {
        .input = {.name = "never a menu or an exit: the attempts run out", .outcomes = {UNCERTAIN}, .policy = {.attempts = 5}},
        .expectation = {.result = WildEscapeResult::UNCONFIRMED, .runs = 5},
    },
    //  Onix's Bind in Oreburgh Mine, console B 2026-11-07: nine Runs refused.
    {
        .input = {.name = "bound: every Run refused, so the battle is fought", .outcomes = {TRAPPED}},
        .expectation = {.result = WildEscapeResult::FOUGHT, .runs = 3, .fights = 1},
    },
    {
        .input = {.name = "refusals between scrolling text count up to the fight", .outcomes = {TRAPPED, UNCERTAIN, TRAPPED, UNCERTAIN, TRAPPED}},
        .expectation = {.result = WildEscapeResult::FOUGHT, .runs = 5, .fights = 1},
    },
    {
        .input = {.name = "bound with the fight turned off: the attempts run out", .outcomes = {TRAPPED}, .policy = {.attempts = 6, .fallback = TrappedFallback::GIVE_UP}},
        .expectation = {.result = WildEscapeResult::UNCONFIRMED, .runs = 6},
    },
};

} // namespace


void add_tests_AutoStory_WildBattlePolicy(UnitTestDatabase& database){
    for (const EscapePolicyCase& item : WILD_BATTLE_POLICY_CASES){
        database.add<Test_WildBattlePolicy>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
