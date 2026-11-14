/*  PokemonLZA AutoStory - Failures Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
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
    {.name = "a zone start missed is a walk short", .input = {.failure = "zone_start_missed"}, .expectation = {.tag = "class=walk_short"}},
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

//  No PLZA name shadows a name the shared list classes: the game's class
//  would hide the list's.
UnitTestResult check_no_shared_names(){
    for (const PlzaFailure& failure : plza_failures()){
        if (Pokemon::story_failure_class_of_outcome(failure.name)){
            return std::format("\"{}\" is in the shared list already.", failure.name);
        }
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

class Test_NoSharedNames : public UnitTest{
public:
    Test_NoSharedNames()
        : UnitTest("PokemonLZA::AutoStory - Failure class: no PLZA name is a shared one")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_no_shared_names();
    }
};

} // namespace


void add_tests_AutoStoryFailures(UnitTestDatabase& database){
    for (const FailureClassCase& test : FAILURE_CLASS_CASES){
        database.add<Test_FailureClass>(test);
    }
    database.add<Test_NoSharedNames>();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
