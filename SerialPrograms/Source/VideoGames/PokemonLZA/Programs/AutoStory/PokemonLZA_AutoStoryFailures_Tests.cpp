/*  PokemonLZA AutoStory - Failures Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcomeClass.h"
#include "PokemonLZA_AutoStoryFailures.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{

struct FailureClassCase{
    std::string name;
    struct Input{
        std::string failure;
    };
    Input input;
    struct Expectation{
        //  The class the log tags; none: no tag.
        std::optional<std::string_view> tag;
    };
    Expectation expectation;
};

const FailureClassCase FAILURE_CLASS_CASES[] = {
    {.name = "a missed gate is a pose lost", .input = {.failure = "gate_not_seen"}, .expectation = {.tag = "class=pose_lost"}},
    {.name = "no A prompt is a screen unread", .input = {.failure = "no_prompt"}, .expectation = {.tag = "class=screen_unread"}},
    {.name = "a road stop missed is a walk short", .input = {.failure = "zone_road_missed"}, .expectation = {.tag = "class=walk_short"}},
    {.name = "a zone exit missed is a walk short", .input = {.failure = "zone_exit_missed"}, .expectation = {.tag = "class=walk_short"}},
    {.name = "not at the start is a wrong start", .input = {.failure = "not_at_start"}, .expectation = {.tag = "class=wrong_start"}},
    {.name = "a shared name keeps the shared class", .input = {.failure = "operation_failed"}, .expectation = {.tag = "class=operation_failed"}},
    {.name = "an unknown name has no class", .input = {.failure = "no_such_failure"}, .expectation = {.tag = std::nullopt}},
};

UnitTestResult check_failure_class(const FailureClassCase& test){
    const std::string line = Pokemon::to_string_with_class(plza_failed(test.input.failure, "detail"));
    const size_t tag = line.find("class=");
    if (!test.expectation.tag){
        if (tag != std::string::npos){
            return std::format("Expected no class, got \"{}\".", line);
        }
        return true;
    }
    if (line.find(*test.expectation.tag) == std::string::npos){
        return std::format("Expected \"{}\" in \"{}\".", *test.expectation.tag, line);
    }
    return true;
}

//  A table of PLZA's names, none of which the shared list classes: the
//  game's class would hide the list's.
struct NameTableCase{
    std::string name;
    struct Input{
        const std::vector<PlzaFailure>& (*table)();
    };
    Input input;
    struct Expectation{
        //  The names the shared list already has.
        std::vector<std::string_view> shared;
    };
    Expectation expectation;
};

const NameTableCase NAME_TABLE_CASES[] = {
    {.name = "no PLZA name is a shared one", .input = {.table = plza_failures}, .expectation = {.shared = {}}},
};

UnitTestResult check_name_table(const NameTableCase& test){
    std::vector<std::string_view> shared;
    for (const PlzaFailure& failure : test.input.table()){
        if (Pokemon::story_failure_class_of_outcome(failure.name)){
            shared.push_back(failure.name);
        }
    }
    if (shared != test.expectation.shared){
        std::string names;
        for (std::string_view name : shared){
            names += std::format(" {}", name);
        }
        return std::format("In the shared list already:{}", names);
    }
    return true;
}

//  The failure name of a console step that throws.
struct ConsoleStepCase{
    std::string name;
    struct Input{
        std::string action;
    };
    Input input;
    struct Expectation{
        std::string failure;
    };
    Expectation expectation;
};

const ConsoleStepCase CONSOLE_STEP_CASES[] = {
    {.name = "03.08's walk back to the road", .input = {.action = "walk_back_to_zone_road"}, .expectation = {.failure = "zone_road_missed"}},
    {.name = "the check out of the wild zone", .input = {.action = "check_out_of_wild_zone"}, .expectation = {.failure = "zone_exit_missed"}},
    {.name = "03.08's catches keep the shared name", .input = {.action = "catch_in_zone"}, .expectation = {.failure = "operation_failed"}},
};

UnitTestResult check_console_step(const ConsoleStepCase& test){
    const std::string_view failure = console_step_failure(test.input.action);
    if (failure != test.expectation.failure){
        return std::format("Expected {}, got {}.", test.expectation.failure, failure);
    }
    return true;
}

class Test_FailureClass : public UnitTest{
public:
    explicit Test_FailureClass(const FailureClassCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Failure class: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_failure_class(m_test);
    }

private:
    const FailureClassCase& m_test;
};

class Test_NameTable : public UnitTest{
public:
    explicit Test_NameTable(const NameTableCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Failure class: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_name_table(m_test);
    }

private:
    const NameTableCase& m_test;
};

class Test_ConsoleStep : public UnitTest{
public:
    explicit Test_ConsoleStep(const ConsoleStepCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Console step failure: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_console_step(m_test);
    }

private:
    const ConsoleStepCase& m_test;
};

} // namespace


void add_tests_AutoStoryFailures(UnitTestDatabase& database){
    for (const FailureClassCase& test : FAILURE_CLASS_CASES){
        database.add<Test_FailureClass>(test);
    }
    for (const NameTableCase& test : NAME_TABLE_CASES){
        database.add<Test_NameTable>(test);
    }
    for (const ConsoleStepCase& test : CONSOLE_STEP_CASES){
        database.add<Test_ConsoleStep>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
