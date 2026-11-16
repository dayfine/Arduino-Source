/*  AutoStory Failure Classes Tests
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
#include "PokemonFRLG_AutoStoryFailureClasses.h"
#include "PokemonFRLG_AutoStoryFailureClasses_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::StoryFailureClass;


namespace{


//  One failure name and its class.
struct NameCase{
    struct Input{
        std::string_view failure;
    };
    struct Expectation{
        //  None: no class.
        std::optional<StoryFailureClass> failure_class;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

const std::vector<NameCase> NAME_CASES = {
    {.name = "FRLG's own name", .input = {.failure = "no_pose"}, .expectation = {.failure_class = StoryFailureClass::POSE_LOST}},
    {.name = "a trainer never seen", .input = {.failure = "no_trainer"}, .expectation = {.failure_class = StoryFailureClass::SCREEN_UNREAD}},
    {.name = "a blackout", .input = {.failure = "blackout"}, .expectation = {.failure_class = StoryFailureClass::BATTLE_LOST}},
    {.name = "a family by its prefix", .input = {.failure = "mart_no_money"}, .expectation = {.failure_class = StoryFailureClass::MENU_FAILED}},
    {.name = "a Fly failure", .input = {.failure = "fly_no_map"}, .expectation = {.failure_class = StoryFailureClass::MENU_FAILED}},
    {.name = "an exact name over its prefix", .input = {.failure = "mart_cancel"}, .expectation = {.failure_class = StoryFailureClass::SIM_MISMATCH}},
    {.name = "an engine name", .input = {.failure = "expect_failed"}, .expectation = {.failure_class = StoryFailureClass::STATE_CHECK_FAILED}},
    {.name = "the shared launch", .input = {.failure = "launch_failed"}, .expectation = {.failure_class = StoryFailureClass::LAUNCH_FAILED}},
    {.name = "an exception out of a step", .input = {.failure = "OperationFailedException"}, .expectation = {.failure_class = StoryFailureClass::OPERATION_FAILED}},
    {.name = "a name no class lists", .input = {.failure = "something_new"}, .expectation = {}},
    {.name = "a prefix alone is no name", .input = {.failure = "mart_"}, .expectation = {}},
};

std::string class_text(std::optional<StoryFailureClass> failure_class){
    return failure_class ? std::string(Pokemon::story_failure_class_info(*failure_class).id) : "none";
}

UnitTestResult check_name(const NameCase& test){
    const std::optional<StoryFailureClass> got = frlg_failure_class(test.input.failure);
    if (got != test.expectation.failure_class){
        return std::format("{}: expected {}, got {}", test.input.failure, class_text(test.expectation.failure_class), class_text(got));
    }
    return true;
}


//  One step failure, as the engine's failure line writes it.
struct OutcomeCase{
    struct Input{
        std::string_view failure;
        std::string_view detail;
    };
    struct Expectation{
        std::string_view line;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

const std::vector<OutcomeCase> OUTCOME_CASES = {
    {.name = "FRLG's own name carries its class",
     .input = {.failure = "no_trainer", .detail = "3 s on the tile"},
     .expectation = {.line = "no_trainer: 3 s on the tile (class=screen_unread)"}},
    {.name = "a family's name carries the family's class",
     .input = {.failure = "fly_no_landing", .detail = "Celadon City"},
     .expectation = {.line = "fly_no_landing: Celadon City (class=menu_failed)"}},
    {.name = "an engine name carries the shared list's class",
     .input = {.failure = "reload_failed", .detail = "no save"},
     .expectation = {.line = "reload_failed: no save (class=reload_failed)"}},
    {.name = "a name no class lists carries none",
     .input = {.failure = "something_new", .detail = "x"},
     .expectation = {.line = "something_new: x"}},
};

UnitTestResult check_outcome(const OutcomeCase& test){
    const std::string got = Pokemon::to_string_with_class(frlg_failed(std::string(test.input.failure), std::string(test.input.detail)));
    if (got != test.expectation.line){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.line, got);
    }
    return true;
}


//  One log line, tagged.
struct LineCase{
    struct Input{
        std::string_view line;
    };
    struct Expectation{
        std::string_view line;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

const std::vector<LineCase> LINE_CASES = {
    {.name = "an event's failure",
     .input = {.line = "[Event] 08.01/3 winston_line: failed: no_trainer: 3 s on the tile north"},
     .expectation = {.line = "[Event] 08.01/3 winston_line: failed: no_trainer: 3 s on the tile north class=screen_unread"}},
    {.name = "a checkpoint's failure",
     .input = {.line = "[Checkpoint] 11.01: failed at 4 fly_celadon: fly_no_map: Celadon City"},
     .expectation = {.line = "[Checkpoint] 11.01: failed at 4 fly_celadon: fly_no_map: Celadon City class=menu_failed"}},
    {.name = "a failure with no detail",
     .input = {.line = "[Event] 08.01/5 hiker: failed: blackout"},
     .expectation = {.line = "[Event] 08.01/5 hiker: failed: blackout class=battle_lost"}},
    {.name = "an exception out of a step",
     .input = {.line = "[Event] 09.02/2 lift: failed: OperationFailedException: no lift"},
     .expectation = {.line = "[Event] 09.02/2 lift: failed: OperationFailedException: no lift class=operation_failed"}},
    {.name = "a load's failure",
     .input = {.line = "[Story] launch_failed: frlg: stuck on title: 12 presses changed nothing"},
     .expectation = {.line = "[Story] launch_failed: frlg: stuck on title: 12 presses changed nothing class=launch_failed"}},
    {.name = "a line that is no failure",
     .input = {.line = "[Event] 08.01/3 winston_line: done"},
     .expectation = {.line = "[Event] 08.01/3 winston_line: done"}},
    {.name = "a story line that names no failure",
     .input = {.line = "[Story] start: checkpoint 08.01, from save"},
     .expectation = {.line = "[Story] start: checkpoint 08.01, from save"}},
    {.name = "a failure name no class lists",
     .input = {.line = "[Event] 08.01/3 winston_line: failed: something_new"},
     .expectation = {.line = "[Event] 08.01/3 winston_line: failed: something_new"}},
    {.name = "a line already tagged",
     .input = {.line = "[Event] 08.01/5 hiker: failed: blackout class=battle_lost"},
     .expectation = {.line = "[Event] 08.01/5 hiker: failed: blackout class=battle_lost"}},
};

UnitTestResult check_line(const LineCase& test){
    const std::string got = tag_failure_line(test.input.line);
    if (got != test.expectation.line){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.line, got);
    }
    return true;
}


class FailureNameTest : public UnitTest{
public:
    explicit FailureNameTest(NameCase test)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Failure class: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_name(m_test);
    }

private:
    NameCase m_test;
};

class FailureOutcomeTest : public UnitTest{
public:
    explicit FailureOutcomeTest(OutcomeCase test)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Failure outcome: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_outcome(m_test);
    }

private:
    OutcomeCase m_test;
};

class FailureLineTest : public UnitTest{
public:
    explicit FailureLineTest(LineCase test)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Failure line: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_line(m_test);
    }

private:
    LineCase m_test;
};


} // namespace


void add_tests_AutoStoryFailureClasses(UnitTestDatabase& database){
    for (const NameCase& test : NAME_CASES){
        database.add<FailureNameTest>(test);
    }
    for (const LineCase& test : LINE_CASES){
        database.add<FailureLineTest>(test);
    }
    for (const OutcomeCase& test : OUTCOME_CASES){
        database.add<FailureOutcomeTest>(test);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
