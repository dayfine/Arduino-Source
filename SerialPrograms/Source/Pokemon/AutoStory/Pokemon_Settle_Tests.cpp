/*  Settle Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The rule table for each SettleKind, and run_settle() on a scripted
 *  screen: what it presses, when it ends and how it fails.
 *
 */

#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_Settle.h"
#include "Pokemon_Settle_Tests.h"
#include "Pokemon_TestExpect.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

using namespace std::chrono_literals;


//  A rule's fields, for comparing and for messages.
std::string describe(const SettleRule& rule){
    return std::format(
        "warp={} quiet={}ms calm={}ms still={} look={}ms cap={}ms",
        rule.judge.warp, rule.judge.quiet.count(), rule.judge.calm.count(),
        screen_event_name(rule.judge.still), rule.look_every.count(), rule.cap.count()
    );
}


//  The rule a kind of settle runs by.
struct RuleTestCase{
    struct Input{
        SettleKind kind;
    };

    struct Expectation{
        std::string rule;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_Rule : public UnitTest{
public:
    explicit Test_Rule(RuleTestCase test_case)
        : UnitTest(std::format("Pokemon::Settle - rule: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return expect_equal("settle_rule()", describe(settle_rule(m_case.input.kind)), m_case.expectation.rule);
    }

private:
    const RuleTestCase m_case;
};

const RuleTestCase RULE_CASES[] = {
    {
        .name = "a talk waits out 5 s of quiet",
        .input = {SettleKind::TALK},
        .expectation = {"warp=false quiet=5000ms calm=15000ms still=screen still look=1200ms cap=120000ms"},
    },
    {
        .name = "after a warp, the fade or banner comes first",
        .input = {SettleKind::AFTER_WARP},
        .expectation = {"warp=true quiet=5000ms calm=15000ms still=screen still look=1200ms cap=120000ms"},
    },
    {
        .name = "a door waits 1 s of the player still, looking often",
        .input = {SettleKind::DOOR},
        .expectation = {"warp=false quiet=1000ms calm=2500ms still=player still look=200ms cap=120000ms"},
    },
};


//  The screen a settle sees, a look per press: the script's looks in
//  order, then its last one for good. Each press and each fled battle
//  moves the clock on.
class ScriptedSettleIO : public SettleIO{
public:
    ScriptedSettleIO(std::vector<ScreenEvents> looks, bool flee)
        : m_looks(std::move(looks))
        , m_flee(flee)
    {}

    virtual SettleLook look() override{
        const size_t index = m_next < m_looks.size() ? m_next : m_looks.size() - 1;
        m_next++;
        return {m_looks[index], m_now};
    }
    virtual void press(SceneMove move, Milliseconds wait) override{
        m_presses += move == SceneMove::NEXT ? "N" : "B";
        m_now += wait;
    }
    virtual bool flee_battle() override{
        m_presses += "F";
        m_now += 5000ms;
        return m_flee;
    }
    virtual void log(std::string_view) override{}

    //  What it pressed: N for the talk's button, B for B, F for a flee.
    const std::string& presses() const{
        return m_presses;
    }

private:
    const std::vector<ScreenEvents> m_looks;
    const bool m_flee;
    size_t m_next = 0;
    WallClock m_now = WallClock();
    std::string m_presses;
};

const ScreenEvents QUIET{ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::SCREEN_STILL, ScreenEvent::PLAYER_STILL};
const ScreenEvents BOX{ScreenEvent::DIALOG_BOX, ScreenEvent::SCREEN_STILL};
//  The overworld with someone walking past: the screen moves, the player does not.
const ScreenEvents PEOPLE_WALKING{ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::PLAYER_STILL};
//  A black frame as BDSP's eyes see it: no box, so "overworld", and still.
const ScreenEvents FADED{ScreenEvent::FADE_BLACK, ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::SCREEN_STILL, ScreenEvent::PLAYER_STILL};
const ScreenEvents BATTLE{ScreenEvent::BATTLE_STARTED};

std::vector<ScreenEvents> repeat(const ScreenEvents& look, size_t count){
    return std::vector<ScreenEvents>(count, look);
}
std::vector<ScreenEvents> concat(std::vector<ScreenEvents> a, const std::vector<ScreenEvents>& b){
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

std::string_view result_name(SettleResult result){
    switch (result){
    case SettleResult::QUIET:       return "quiet";
    case SettleResult::BATTLE:      return "battle";
    case SettleResult::TIMED_OUT:   return "timed out";
    }
    return "?";
}


//  run_settle() over a scripted screen.
struct RunTestCase{
    struct Input{
        SettleRule rule;
        std::vector<ScreenEvents> looks;
        //  Whether a battle that starts is fled (a wild one).
        bool flee = true;
    };

    struct Expectation{
        SettleResult result;
        //  The presses (ScriptedSettleIO::presses()).
        std::string presses;
        //  The end's text, checked for a timeout only: its failure.
        std::string failure = {};
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_Run : public UnitTest{
public:
    explicit Test_Run(RunTestCase test_case)
        : UnitTest(std::format("Pokemon::Settle - run: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ScriptedSettleIO io(m_case.input.looks, m_case.input.flee);
        const SettleEnd end = run_settle(io, m_case.input.rule, "Step");
        if (end.result != m_case.expectation.result){
            return std::format("expected {}, got {} ({})", result_name(m_case.expectation.result), result_name(end.result), end.text);
        }
        if (end.result == SettleResult::TIMED_OUT){
            const UnitTestResult text = expect_equal("failure", end.text, m_case.expectation.failure);
            if (text.result != UnitTestResult::PASSED){
                return text;
            }
        }
        return expect_equal("presses", io.presses(), m_case.expectation.presses);
    }

private:
    const RunTestCase m_case;
};

SettleRule talk_before_battle(){
    SettleRule rule = settle_rule(SettleKind::TALK);
    rule.judge.end_on_quiet = false;
    return rule;
}

const RunTestCase RUN_CASES[] = {
    {
        .name = "a talk presses B while quiet, until 5 s of it",
        .input = {.rule = settle_rule(SettleKind::TALK), .looks = {QUIET}},
        .expectation = {.result = SettleResult::QUIET, .presses = "BBBBB"},
    },
    {
        .name = "a box is answered with the talk's button",
        .input = {.rule = settle_rule(SettleKind::TALK), .looks = concat(repeat(BOX, 2), {QUIET})},
        .expectation = {.result = SettleResult::QUIET, .presses = "NNBBBBB"},
    },
    {
        .name = "a door's settle ends 1 s after the fade is back up",
        .input = {.rule = settle_rule(SettleKind::DOOR), .looks = concat(repeat(FADED, 3), {QUIET})},
        .expectation = {.result = SettleResult::QUIET, .presses = "BBBBBBBB"},
    },
    {
        .name = "people walking about do not hold a door's settle up",
        .input = {.rule = settle_rule(SettleKind::DOOR), .looks = {PEOPLE_WALKING}},
        .expectation = {.result = SettleResult::QUIET, .presses = "BBBBB"},
    },
    {
        .name = "a wild battle is fled and the settle goes on",
        .input = {.rule = settle_rule(SettleKind::DOOR), .looks = concat({QUIET, BATTLE}, {QUIET})},
        .expectation = {.result = SettleResult::QUIET, .presses = "BFBBBBB"},
    },
    {
        .name = "a battle not fled ends it",
        .input = {.rule = talk_before_battle(), .looks = concat(repeat(BOX, 2), {BATTLE}), .flee = false},
        .expectation = {.result = SettleResult::BATTLE, .presses = "NNF"},
    },
    {
        .name = "no fade or banner by the cap is a map mismatch",
        .input = {.rule = {.judge = {.warp = true}, .look_every = 1000ms, .cap = 3000ms}, .looks = {QUIET}},
        .expectation = {
            .result = SettleResult::TIMED_OUT, .presses = "BBB",
            .failure = "Step: no fade or banner within 3 s, so we are not where the step goes (map mismatch).",
        },
    },
    {
        .name = "no quiet overworld by the cap",
        .input = {.rule = {.look_every = 1000ms, .cap = 3000ms}, .looks = {BOX}},
        .expectation = {
            .result = SettleResult::TIMED_OUT, .presses = "NNN",
            .failure = "Step: not back on a quiet overworld within 3 s.",
        },
    },
    {
        .name = "no battle by the cap after a trainer's talk",
        .input = {.rule = {.judge = {.end_on_quiet = false}, .look_every = 1000ms, .cap = 3000ms}, .looks = {QUIET}},
        .expectation = {
            .result = SettleResult::TIMED_OUT, .presses = "BBB",
            .failure = "Step: no battle within 3 s of the talk.",
        },
    },
};

} // namespace


void add_tests_Settle(UnitTestDatabase& database){
    for (const RuleTestCase& item : RULE_CASES){
        database.add<Test_Rule>(item);
    }
    for (const RunTestCase& item : RUN_CASES){
        database.add<Test_Run>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
