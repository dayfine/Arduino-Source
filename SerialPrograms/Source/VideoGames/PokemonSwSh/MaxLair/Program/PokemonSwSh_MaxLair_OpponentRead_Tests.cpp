/*  Max Lair Opponent Read Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#include <array>
#include <format>
#include <span>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonSwSh_MaxLair_OpponentRead.h"
#include "PokemonSwSh_MaxLair_OpponentRead_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{
namespace MaxLairInternal{
namespace{

enum class Call{ HEADER, OPEN, WAIT, SUMMARY, CLOSE, INVALID };
struct Input{
    std::string name;
    uint8_t wins;
    std::set<std::string> previous;
    std::string boss;
    std::set<std::string> header;
    std::set<std::string> summary;
};
struct Expectation{
    bool accepted;
    std::set<std::string> opponent;
    std::string boss;
    std::vector<Call> calls;
};
struct TestCase{
    Input input;
    Expectation expectation;
};

// Only screen/input boundaries are scripted; decisions run in production code.
class ScriptedRead : public OpponentReadActions{
public:
    explicit ScriptedRead(const Input& input) : m_input(input) {}
    virtual std::set<std::string> read_header() override{
        m_calls.push_back(Call::HEADER);
        return m_input.header;
    }
    virtual void open_summary() override{ m_calls.push_back(Call::OPEN); }
    virtual std::set<std::string> read_summary() override{
        m_calls.push_back(Call::SUMMARY);
        return m_input.summary;
    }
    virtual void close_summary() override{ m_calls.push_back(Call::CLOSE); }
    virtual void wait_for_inputs() override{ m_calls.push_back(Call::WAIT); }
    virtual void report_invalid() override{ m_calls.push_back(Call::INVALID); }
    const std::vector<Call>& calls() const{ return m_calls; }
private:
    const Input& m_input;
    std::vector<Call> m_calls;
};

std::string describe_calls(std::span<const Call> calls){
    constexpr std::array<std::string_view, 6> NAMES = {"header", "open", "wait", "summary", "close", "invalid"};
    std::string result;
    for (Call call : calls){
        result += NAMES[(size_t)call];
        result += " ";
    }
    return result;
}

class TestOpponentRead : public UnitTest{
public:
    explicit TestOpponentRead(TestCase test)
        : UnitTest("PokemonSwSh::MaxLairOpponentRead - " + test.input.name)
        , m_test(std::move(test))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const Input& input = m_test.input;
        const Expectation& expected = m_test.expectation;
        GlobalState state;
        state.wins = input.wins;
        state.opponent = input.previous;
        state.boss = input.boss;
        ScriptedRead actions(input);
        const bool accepted = read_battle_opponent(logger, state, actions);
        if (accepted != expected.accepted){
            return std::format("Expected accepted={}, got {}", expected.accepted, accepted);
        }
        if (state.opponent != expected.opponent || state.boss != expected.boss){
            return std::format("Expected opponent {} / boss '{}', got {} / '{}'",
                set_to_str(expected.opponent), expected.boss, set_to_str(state.opponent), state.boss);
        }
        if (actions.calls() != expected.calls){
            return std::format("Expected I/O [{}], got [{}]", describe_calls(expected.calls), describe_calls(actions.calls()));
        }
        return true;
    }
private:
    TestCase m_test;
};

// Reported 2026-11-02: Mimikyu was remembered as the boss, then a Kyurem read
// did not repair it. The original OCR/frame error is not reproduced here.
// Other rows are synthetic control-flow boundaries, not recorded console reads.
const TestCase CASES[] = {
    {
        .input = {.name = "valid rental skips summary", .wins = 2, .previous = {}, .boss = "", .header = {"mimikyu-disguised"}, .summary = {}},
        .expectation = {.accepted = true, .opponent = {"mimikyu-disguised"}, .boss = "", .calls = {Call::HEADER, Call::WAIT}},
    },
    {
        .input = {.name = "valid boss header records boss", .wins = 3, .previous = {}, .boss = "", .header = {"kyurem"}, .summary = {}},
        .expectation = {.accepted = true, .opponent = {"kyurem"}, .boss = "kyurem", .calls = {Call::HEADER, Call::WAIT}},
    },
    {
        .input = {.name = "rental header triggers summary recovery", .wins = 3, .previous = {}, .boss = "", .header = {"mimikyu-disguised"}, .summary = {"kyurem"}},
        .expectation = {.accepted = true, .opponent = {"kyurem"}, .boss = "kyurem", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT}},
    },
    {
        .input = {.name = "summary repairs remembered rental boss", .wins = 3, .previous = {}, .boss = "mimikyu-disguised", .header = {"mimikyu-disguised"}, .summary = {"kyurem"}},
        .expectation = {.accepted = true, .opponent = {"kyurem"}, .boss = "kyurem", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT}},
    },
    {
        .input = {.name = "early boss header retries to rental", .wins = 2, .previous = {}, .boss = "", .header = {"kyurem"}, .summary = {"mimikyu-disguised"}},
        .expectation = {.accepted = true, .opponent = {"mimikyu-disguised"}, .boss = "", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT}},
    },
    {
        .input = {.name = "empty header reads summary", .wins = 3, .previous = {}, .boss = "", .header = {}, .summary = {"kyurem"}},
        .expectation = {.accepted = true, .opponent = {"kyurem"}, .boss = "kyurem", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT}},
    },
    {
        .input = {.name = "empty summary with no known boss stops", .wins = 3, .previous = {}, .boss = "", .header = {"mimikyu-disguised"}, .summary = {}},
        .expectation = {.accepted = false, .opponent = {}, .boss = "", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT, Call::INVALID}},
    },
    {
        .input = {.name = "ambiguous summary with no known boss stops", .wins = 3, .previous = {}, .boss = "", .header = {"mimikyu-disguised"}, .summary = {"kyurem", "mimikyu-disguised"}},
        .expectation = {.accepted = false, .opponent = {"kyurem", "mimikyu-disguised"}, .boss = "", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT, Call::INVALID}},
    },
    {
        .input = {.name = "rental summary cannot become boss", .wins = 3, .previous = {}, .boss = "", .header = {}, .summary = {"mimikyu-disguised"}},
        .expectation = {.accepted = false, .opponent = {"mimikyu-disguised"}, .boss = "", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT, Call::INVALID}},
    },
    {
        .input = {.name = "known boss overrides conflicting summary", .wins = 3, .previous = {}, .boss = "kyurem", .header = {"mimikyu-disguised"}, .summary = {"zapdos"}},
        .expectation = {.accepted = true, .opponent = {"kyurem"}, .boss = "kyurem", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT}},
    },
    {
        .input = {.name = "known boss survives empty summary", .wins = 3, .previous = {}, .boss = "kyurem", .header = {"mimikyu-disguised"}, .summary = {}},
        .expectation = {.accepted = true, .opponent = {"kyurem"}, .boss = "kyurem", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT}},
    },
    {
        .input = {.name = "cached valid boss tolerates empty header", .wins = 3, .previous = {"kyurem"}, .boss = "kyurem", .header = {}, .summary = {}},
        .expectation = {.accepted = true, .opponent = {"kyurem"}, .boss = "kyurem", .calls = {Call::HEADER, Call::WAIT}},
    },
    {
        .input = {.name = "cached rental cannot bypass boss-stage check", .wins = 3, .previous = {"mimikyu-disguised"}, .boss = "", .header = {}, .summary = {"kyurem"}},
        .expectation = {.accepted = true, .opponent = {"kyurem"}, .boss = "kyurem", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT}},
    },
    {
        .input = {.name = "ambiguous header uses matching cached rental", .wins = 2, .previous = {"mimikyu-disguised"}, .boss = "", .header = {"mimikyu-disguised", "pikachu"}, .summary = {}},
        .expectation = {.accepted = true, .opponent = {"mimikyu-disguised"}, .boss = "", .calls = {Call::HEADER, Call::WAIT}},
    },
    {
        .input = {.name = "known boss not forced before fourth battle", .wins = 2, .previous = {}, .boss = "kyurem", .header = {"kyurem"}, .summary = {"mimikyu-disguised"}},
        .expectation = {.accepted = true, .opponent = {"mimikyu-disguised"}, .boss = "kyurem", .calls = {Call::HEADER, Call::OPEN, Call::WAIT, Call::SUMMARY, Call::CLOSE, Call::WAIT}},
    },
};

} // namespace

void add_tests_MaxLairOpponentRead(UnitTestDatabase& database){
    for (const TestCase& test : CASES){
        database.add<TestOpponentRead>(test);
    }
}

} // namespace MaxLairInternal
} // namespace PokemonSwSh
} // namespace NintendoSwitch
} // namespace PokemonAutomation
