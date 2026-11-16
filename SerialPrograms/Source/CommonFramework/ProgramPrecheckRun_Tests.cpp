/*  Program Precheck Run - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a checks-only run stages candidate options: the merge over the current
 *  options (groups keep the keys not given, unknown keys are refused), and
 *  the check that the program then holds every candidate value.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "ProgramPrecheckRun.h"
#include "ProgramPrecheckRun_Tests.h"

namespace PokemonAutomation{

namespace{


constexpr std::string_view CURRENT = R"({"COUNT":5,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.5})";


struct MergeInput{
    std::string candidate = {};
};
struct MergeExpectation{
    std::optional<std::string> merged = {};     //  Compact JSON with sorted keys, when merged.
    std::optional<std::string> refused_key = {};    //  When refused.
};
struct MergeCase{
    std::string name;
    MergeInput input;
    MergeExpectation expectation;
};

UnitTestResult check(const MergeCase& c){
    const std::expected<JsonValue, RefusedOption> merged =
        merge_candidate_options(parse_json(std::string(CURRENT)), parse_json(c.input.candidate));
    if (!merged){
        if (c.expectation.refused_key == merged.error().key){
            return true;
        }
        return std::format("refused {} ({}), expected {}", merged.error().key, merged.error().message,
            c.expectation.merged.value_or("a refusal of another key"));
    }
    const std::string text = merged->dump(-1);
    if (c.expectation.merged == text){
        return true;
    }
    return std::format("merged to\n{}\nexpected {}", text, c.expectation.merged.value_or("a refusal"));
}

class Test_MergeCandidateOptions : public UnitTest{
public:
    explicit Test_MergeCandidateOptions(MergeCase test_case)
        : UnitTest(std::format("CommonFramework::ProgramPrecheckRun - merge - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    MergeCase m_case;
};

const MergeCase MERGE_CASES[] = {
    {
        .name = "a value replaces the current one, the rest stay",
        .input = {.candidate = R"({"COUNT":7})"},
        .expectation = {.merged = R"({"COUNT":7,"NOTIFICATIONS":{"Enabled":true,"Screenshot":false},"SPEED":0.5})"},
    },
    {
        .name = "a group keeps the keys not given",
        .input = {.candidate = R"({"NOTIFICATIONS":{"Screenshot":true}})"},
        .expectation = {.merged = R"({"COUNT":5,"NOTIFICATIONS":{"Enabled":true,"Screenshot":true},"SPEED":0.5})"},
    },
    {
        .name = "an empty candidate changes nothing",
        .input = {.candidate = R"({})"},
        .expectation = {.merged = std::string(CURRENT)},
    },
    {
        .name = "an unknown key is refused",
        .input = {.candidate = R"({"COUNT":7,"SPEEED":1.0})"},
        .expectation = {.refused_key = "SPEEED"},
    },
    {
        .name = "an unknown key in a group is refused with its key path",
        .input = {.candidate = R"({"NOTIFICATIONS":{"Sound":true}})"},
        .expectation = {.refused_key = "NOTIFICATIONS.Sound"},
    },
    {
        .name = "a value for a group is refused",
        .input = {.candidate = R"({"NOTIFICATIONS":false})"},
        .expectation = {.refused_key = "NOTIFICATIONS"},
    },
    {
        .name = "a candidate that is not an object is refused",
        .input = {.candidate = R"([1,2])"},
        .expectation = {.refused_key = ""},
    },
};


struct LoadedInput{
    std::string candidate = {};
    std::string loaded = {};     //  The options read back after loading the merged candidate.
};
struct LoadedExpectation{
    std::optional<std::string> refused_key = {};    //  None: the program holds every value.
};
struct LoadedCase{
    std::string name;
    LoadedInput input;
    LoadedExpectation expectation;
};

UnitTestResult check(const LoadedCase& c){
    const std::optional<RefusedOption> refused =
        find_refused_option(parse_json(c.input.candidate), parse_json(c.input.loaded));
    const std::optional<std::string> key = refused ? std::optional<std::string>(refused->key) : std::nullopt;
    if (key == c.expectation.refused_key){
        return true;
    }
    const std::string got = refused ? std::format("{} ({})", refused->key, refused->message) : "nothing";
    return std::format("refused {}, expected {}", got, c.expectation.refused_key.value_or("nothing"));
}

class Test_FindRefusedOption : public UnitTest{
public:
    explicit Test_FindRefusedOption(LoadedCase test_case)
        : UnitTest(std::format("CommonFramework::ProgramPrecheckRun - loaded - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    LoadedCase m_case;
};

const LoadedCase LOADED_CASES[] = {
    {
        .name = "every value held",
        .input = {
            .candidate = R"({"COUNT":7,"NOTIFICATIONS":{"Screenshot":true}})",
            .loaded = R"({"COUNT":7,"NOTIFICATIONS":{"Enabled":true,"Screenshot":true},"SPEED":0.5})",
        },
        .expectation = {},
    },
    {
        .name = "an integer for a number option is the same value",
        .input = {.candidate = R"({"SPEED":2})", .loaded = R"({"COUNT":5,"SPEED":2.0})"},
        .expectation = {},
    },
    {
        .name = "a number clamped to its bounds",
        .input = {.candidate = R"({"COUNT":70})", .loaded = R"({"COUNT":10,"SPEED":0.5})"},
        .expectation = {.refused_key = "COUNT"},
    },
    {
        .name = "a disabled choice in a group, ignored",
        .input = {
            .candidate = R"({"NOTIFICATIONS":{"Screenshot":true}})",
            .loaded = R"({"NOTIFICATIONS":{"Enabled":true,"Screenshot":false}})",
        },
        .expectation = {.refused_key = "NOTIFICATIONS.Screenshot"},
    },
};


}  // namespace



void add_tests_ProgramPrecheckRun(UnitTestDatabase& database){
    for (const MergeCase& c : MERGE_CASES){
        database.add<Test_MergeCandidateOptions>(c);
    }
    for (const LoadedCase& c : LOADED_CASES){
        database.add<Test_FindRefusedOption>(c);
    }
}



}  // namespace PokemonAutomation
