/*  Program Precheck Run - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checks-only run against a fake program: the candidate merged over the
 *  current options (groups keep the keys not given, unknown keys are
 *  refused), a value the program does not hold refused, options loaded only
 *  on the options thread and put back after the checks, also when one
 *  throws or is cancelled, and kept when a user edits them during the
 *  checks.
 *
 */

#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "ProgramPrecheckRun.h"
#include "ProgramPrecheckRun_Tests.h"

namespace PokemonAutomation{

namespace{


constexpr std::string_view CURRENT = R"({"COUNT":5,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.5})";

//  What the fake program's precheck does while it runs.
enum class CheckAction{
    PASS,
    USER_SETS_COUNT_TO_9,   //  A user edits COUNT in the panel meanwhile.
    THROW,
    CANCEL,     //  The panel changes: the checks' scope is cancelled.
};

//  A program whose options are CURRENT. Loading clamps COUNT to 10, stores
//  SPEED as a double, and ignores NOTIFICATIONS.Screenshot (as false) while
//  NOTIFICATIONS.Enabled is false; check_validity() refuses COUNT 0. Counts
//  option calls made off the options thread.
class FakeProgram : public PrecheckOnlyTarget{
public:
    explicit FakeProgram(CheckAction action)
        : m_options(parse_json(std::string(CURRENT)))
        , m_action(action)
    {}

    bool on_options_thread = false;
    mutable size_t off_thread_calls = 0;
    std::optional<std::string> seen_by_checks = {};

    virtual JsonValue options() const override{
        count_thread();
        return m_options.clone();
    }
    virtual void load_options(const JsonValue& options) override{
        count_thread();
        m_options = options.clone();
        JsonObject& object = *m_options.to_object();
        JsonValue& count = object["COUNT"];
        if (count.to_integer_default() > 10){
            count = JsonValue(int64_t(10));
        }
        object["SPEED"] = JsonValue(object["SPEED"].to_double_default());
        JsonObject& notifications = *object["NOTIFICATIONS"].to_object();
        if (!notifications["Enabled"].to_boolean_default()){
            notifications["Screenshot"] = JsonValue(false);
        }
    }
    virtual std::string check_validity() const override{
        count_thread();
        return m_options.to_object()->get_value("COUNT")->to_integer_default() == 0 ? "COUNT must be at least 1." : "";
    }
    virtual std::optional<std::vector<PrecheckReport>> run_checks() override{
        seen_by_checks = m_options.dump(-1);
        switch (m_action){
        case CheckAction::PASS:
            break;
        case CheckAction::USER_SETS_COUNT_TO_9:
            (*m_options.to_object())["COUNT"] = JsonValue(int64_t(9));
            break;
        case CheckAction::THROW:
            throw std::runtime_error("the console did not answer");
        case CheckAction::CANCEL:
            throw OperationCancelledException();
        }
        return std::vector<PrecheckReport>{PrecheckReport{.name = "black_border", .status = PrecheckReportStatus::PASSED, .message = ""}};
    }
    std::string held() const{
        return m_options.dump(-1);
    }

private:
    void count_thread() const{
        if (!on_options_thread){
            off_thread_calls++;
        }
    }

    JsonValue m_options;
    CheckAction m_action;
};

struct RunInput{
    std::string candidate = {};
    CheckAction action = CheckAction::PASS;
    bool restore_call_fails = false;    //  The options thread refuses its second call.
};
struct RunExpectation{
    std::optional<std::string> refused_key = {};
    std::optional<std::string> seen_by_checks = {};     //  None: no check ran.
    bool threw = false;
    bool cancelled = false;
    bool restored = false;
    std::vector<std::string> edited_keys = {};
    std::string held = {};      //  The options the program holds afterwards.
};
struct RunCase{
    std::string name;
    RunInput input;
    RunExpectation expectation;
};

UnitTestResult check(const RunCase& c, Logger& logger){
    FakeProgram program(c.input.action);
    size_t options_thread_calls = 0;
    const OptionsThreadCall on_options_thread = [&](const std::function<void()>& body){
        options_thread_calls++;
        if (c.input.restore_call_fails && options_thread_calls == 2){
            throw std::runtime_error("the options thread is gone");
        }
        program.on_options_thread = true;
        try{
            body();
        }catch (...){
            program.on_options_thread = false;
            throw;
        }
        program.on_options_thread = false;
    };
    PrecheckOnlyRun run;
    bool threw = false;
    try{
        run = run_prechecks_only(parse_json(c.input.candidate), on_options_thread, program, logger);
    }catch (std::runtime_error&){
        threw = true;
    }
    const RunExpectation& e = c.expectation;
    const std::optional<std::string> refused_key = run.refused ? std::optional<std::string>(run.refused->key) : std::nullopt;
    if (program.off_thread_calls != 0){
        return std::format("{} option calls off the options thread", program.off_thread_calls);
    }
    if (threw != e.threw || refused_key != e.refused_key || program.seen_by_checks != e.seen_by_checks){
        return std::format("threw {}, refused {}, checks saw {}; expected threw {}, refused {}, checks saw {}",
            threw, refused_key.value_or("nothing"), program.seen_by_checks.value_or("nothing"),
            e.threw, e.refused_key.value_or("nothing"), e.seen_by_checks.value_or("nothing"));
    }
    if (!threw && (run.cancelled != e.cancelled || run.restored != e.restored || run.edited_keys != e.edited_keys)){
        return std::format("cancelled {}, restored {}, {} edited keys; expected cancelled {}, restored {}, {} edited keys",
            run.cancelled, run.restored, run.edited_keys.size(), e.cancelled, e.restored, e.edited_keys.size());
    }
    if (program.held() != e.held){
        return std::format("the program holds\n{}\nexpected\n{}", program.held(), e.held);
    }
    return true;
}

class Test_RunPrechecksOnly : public UnitTest{
public:
    explicit Test_RunPrechecksOnly(RunCase test_case)
        : UnitTest(std::format("CommonFramework::ProgramPrecheckRun - run - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return check(m_case, logger);
    }

private:
    RunCase m_case;
};

const RunCase RUN_CASES[] = {
    {
        .name = "a value replaces the current one, the rest stay",
        .input = {.candidate = R"({"COUNT":7})", .action = CheckAction::PASS},
        .expectation = {.seen_by_checks = R"({"COUNT":7,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.5})", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "a group keeps the keys not given",
        .input = {.candidate = R"({"NOTIFICATIONS":{"Screenshot":true}})", .action = CheckAction::PASS},
        .expectation = {.seen_by_checks = R"({"COUNT":5,"NOTIFICATIONS":{"Enabled":true,"Screenshot":true},"SPEED":0.5})", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "an empty candidate changes nothing",
        .input = {.candidate = R"({})", .action = CheckAction::PASS},
        .expectation = {.seen_by_checks = R"({"COUNT":5,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.5})", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "an integer for a number option is the same value",
        .input = {.candidate = R"({"SPEED":2})", .action = CheckAction::PASS},
        .expectation = {.seen_by_checks = R"({"COUNT":5,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":2.0})", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "an unknown key is refused",
        .input = {.candidate = R"({"COUNT":7,"SPEEED":1.0})", .action = CheckAction::PASS},
        .expectation = {.refused_key = "SPEEED", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "an unknown key in a group is refused with its key path",
        .input = {.candidate = R"({"NOTIFICATIONS":{"Sound":true}})", .action = CheckAction::PASS},
        .expectation = {.refused_key = "NOTIFICATIONS.Sound", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "a value for a group is refused",
        .input = {.candidate = R"({"NOTIFICATIONS":false})", .action = CheckAction::PASS},
        .expectation = {.refused_key = "NOTIFICATIONS", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "a candidate that is not an object is refused",
        .input = {.candidate = R"([1,2])", .action = CheckAction::PASS},
        .expectation = {.refused_key = "", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "a number the program clamps is refused",
        .input = {.candidate = R"({"COUNT":70})", .action = CheckAction::PASS},
        .expectation = {.refused_key = "COUNT", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "a choice the program ignores is refused with its key path",
        .input = {.candidate = R"({"NOTIFICATIONS":{"Enabled":false,"Screenshot":true}})", .action = CheckAction::PASS},
        .expectation = {.refused_key = "NOTIFICATIONS.Screenshot", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "a combination check_validity refuses is refused",
        .input = {.candidate = R"({"COUNT":0})", .action = CheckAction::PASS},
        .expectation = {.refused_key = "", .restored = true, .held = std::string(CURRENT)},
    },
    {
        .name = "an option a user edits during the checks is kept and named",
        .input = {.candidate = R"({"SPEED":0.75})", .action = CheckAction::USER_SETS_COUNT_TO_9},
        .expectation = {
            .seen_by_checks = R"({"COUNT":5,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.75})",
            .restored = false,
            .edited_keys = {"COUNT"},
            .held = R"({"COUNT":9,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.75})",
        },
    },
    {
        .name = "a check that throws still puts the prior options back",
        .input = {.candidate = R"({"COUNT":7})", .action = CheckAction::THROW},
        .expectation = {
            .seen_by_checks = R"({"COUNT":7,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.5})",
            .threw = true,
            .held = std::string(CURRENT),
        },
    },
    {
        .name = "cancelled checks put the prior options back and report it",
        .input = {.candidate = R"({"COUNT":7})", .action = CheckAction::CANCEL},
        .expectation = {
            .seen_by_checks = R"({"COUNT":7,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.5})",
            .cancelled = true,
            .restored = true,
            .held = std::string(CURRENT),
        },
    },
    {
        .name = "cancelled checks whose restore fails report the options not restored",
        .input = {.candidate = R"({"COUNT":7})", .action = CheckAction::CANCEL, .restore_call_fails = true},
        .expectation = {
            .seen_by_checks = R"({"COUNT":7,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.5})",
            .cancelled = true,
            .restored = false,
            .held = R"({"COUNT":7,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.5})",
        },
    },
};


}  // namespace



void add_tests_ProgramPrecheckRun(UnitTestDatabase& database){
    for (const RunCase& c : RUN_CASES){
        database.add<Test_RunPrechecksOnly>(c);
    }
}



}  // namespace PokemonAutomation
