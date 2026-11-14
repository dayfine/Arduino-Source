/*  Story Failure Classes Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The list's own checks: every sim fault and every engine failure name
 *  maps to one class, and every class id reads back from the text a log
 *  line or run record writes.
 *
 */

#include <algorithm>
#include <cstddef>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon_StoryFailureClasses.h"
#include "Pokemon_StoryFailureClasses_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  The class id of `failure_class`, or "none" for nothing: for messages.
std::string_view id_or_none(std::optional<StoryFailureClass> failure_class){
    return failure_class ? story_failure_class_info(*failure_class).id : "none";
}


//  An engine Outcome failure name and the class it maps to.
struct OutcomeClassTestCase{
    struct Input{
        std::string_view failure;
    };

    struct Expectation{
        std::optional<StoryFailureClass> failure_class;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_OutcomeClass : public UnitTest{
public:
    explicit Test_OutcomeClass(OutcomeClassTestCase test_case)
        : UnitTest(std::format("Pokemon::StoryFailureClasses - outcome: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<StoryFailureClass> got = story_failure_class_of_outcome(m_case.input.failure);
        if (got != m_case.expectation.failure_class){
            return std::format(
                "\"{}\": expected {}, got {}",
                m_case.input.failure, id_or_none(m_case.expectation.failure_class), id_or_none(got)
            );
        }
        return true;
    }

private:
    const OutcomeClassTestCase m_case;
};

using C = StoryFailureClass;

//  Every failure name the engine's code returns (autostory/engine), and
//  #477's launch_failed.
const OutcomeClassTestCase OUTCOME_CASES[] = {
    {.name = "HOME menu", .input = {"game_not_running"}, .expectation = {C::GAME_NOT_RUNNING}},
    {.name = "launch", .input = {"launch_failed"}, .expectation = {C::LAUNCH_FAILED}},
    {.name = "start checks", .input = {"start_failed"}, .expectation = {C::WRONG_START}},
    {.name = "another save", .input = {"wrong_save"}, .expectation = {C::WRONG_START}},
    {.name = "preflight", .input = {"precondition_failed"}, .expectation = {C::WRONG_START}},
    {.name = "reload", .input = {"reload_failed"}, .expectation = {C::RELOAD_FAILED}},
    {.name = "save", .input = {"save_failed"}, .expectation = {C::SAVE_FAILED}},
    {.name = "unsupported step", .input = {"unsupported_step"}, .expectation = {C::STORY_ERROR}},
    {.name = "unknown checkpoint", .input = {"unknown_checkpoint"}, .expectation = {C::STORY_ERROR}},
    {.name = "bad range", .input = {"bad_range"}, .expectation = {C::STORY_ERROR}},
    {.name = "empty segment", .input = {"empty_segment"}, .expectation = {C::STORY_ERROR}},
    {.name = "bad verify policy", .input = {"bad_verify_policy"}, .expectation = {C::STORY_ERROR}},
    {.name = "lint", .input = {"lint_failed"}, .expectation = {C::STORY_ERROR}},
    {.name = "no save hook", .input = {"no_save_hook"}, .expectation = {C::STORY_ERROR}},
    {.name = "console only", .input = {"console_only"}, .expectation = {C::STORY_ERROR}},
    {.name = "too many interrupts", .input = {"too_many_interrupts"}, .expectation = {C::WILD_BATTLES}},
    {.name = "unhandled interrupt", .input = {"unhandled_interrupt"}, .expectation = {C::INTERRUPT_FAILED}},
    {.name = "interrupt not cleared", .input = {"interrupt_failed"}, .expectation = {C::INTERRUPT_FAILED}},
    {.name = "interrupt unsettled", .input = {"interrupt_unsettled"}, .expectation = {C::INTERRUPT_FAILED}},
    {.name = "interrupted", .input = {"interrupted"}, .expectation = {C::INTERRUPT_FAILED}},
    {.name = "talk without text", .input = {"talk_no_text"}, .expectation = {C::SCREEN_UNREAD}},
    {.name = "mash without overworld", .input = {"mash_no_overworld"}, .expectation = {C::SCREEN_UNREAD}},
    {.name = "expect", .input = {"expect_failed"}, .expectation = {C::STATE_CHECK_FAILED}},
    {.name = "require", .input = {"require_failed"}, .expectation = {C::STATE_CHECK_FAILED}},
    {.name = "checkpoint end", .input = {"end_failed"}, .expectation = {C::STATE_CHECK_FAILED}},
    {.name = "postconditions", .input = {"postconditions_failed"}, .expectation = {C::STATE_CHECK_FAILED}},
    {.name = "operation", .input = {"operation_failed"}, .expectation = {C::OPERATION_FAILED}},
    {.name = "a game's own name", .input = {"no_banner"}, .expectation = {std::nullopt}},
    {.name = "empty", .input = {""}, .expectation = {std::nullopt}},
};


//  A check over the whole list.
struct ListCheckTestCase{
    //  The check, run over story_failure_classes().
    struct Input{
        UnitTestResult (*check)();
    };

    //  What holds when the check passes.
    struct Expectation{
        std::string_view holds;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_ListCheck : public UnitTest{
public:
    explicit Test_ListCheck(ListCheckTestCase test_case)
        : UnitTest(std::format("Pokemon::StoryFailureClasses - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.check();
    }

private:
    const ListCheckTestCase m_case;
};

UnitTestResult rows_in_enum_order(){
    const std::vector<StoryFailureClassInfo>& classes = story_failure_classes();
    for (size_t c = 0; c < classes.size(); c++){
        if (static_cast<size_t>(classes[c].failure_class) != c){
            return std::format("row {} ({}) is out of the enum's order", c, classes[c].id);
        }
    }
    return true;
}

UnitTestResult every_sim_fault_maps_to_one_class(){
    for (FaultClass fault : fault_classes()){
        size_t rows = 0;
        for (const StoryFailureClassInfo& info : story_failure_classes()){
            rows += std::ranges::count(info.injected_by, fault);
        }
        if (rows != 1){
            return std::format("sim fault \"{}\" is in {} rows, not 1", fault_class_name(fault), rows);
        }
        if (!story_failure_class_of(fault)){
            return std::format("sim fault \"{}\" maps to no class", fault_class_name(fault));
        }
    }
    return true;
}

UnitTestResult no_outcome_in_two_classes(){
    std::map<std::string_view, std::string_view> seen;
    for (const StoryFailureClassInfo& info : story_failure_classes()){
        for (std::string_view failure : info.outcomes){
            const auto [iter, added] = seen.emplace(failure, info.id);
            if (!added){
                return std::format("\"{}\" is in {} and {}", failure, iter->second, info.id);
            }
        }
    }
    return true;
}

UnitTestResult ids_round_trip(){
    std::set<std::string_view> ids;
    for (const StoryFailureClassInfo& info : story_failure_classes()){
        if (!ids.insert(info.id).second){
            return std::format("id \"{}\" is in two rows", info.id);
        }
        const std::optional<StoryFailureClass> got = read_story_failure_class_tag(story_failure_class_tag(info.failure_class));
        if (got != info.failure_class){
            return std::format("id \"{}\" reads back as {}", info.id, id_or_none(got));
        }
    }
    return true;
}

UnitTestResult tags_round_trip_through_a_log_line(){
    for (const StoryFailureClassInfo& info : story_failure_classes()){
        const std::string line = std::format(
            "[Checkpoint] 00.04: failed at 2 talk_mom: expect_failed: has_parcel ({})",
            story_failure_class_tag(info.failure_class)
        );
        const std::optional<StoryFailureClass> got = read_story_failure_class_tag(line);
        if (got != info.failure_class){
            return std::format("\"{}\" reads back as {}", line, id_or_none(got));
        }
    }
    return true;
}

UnitTestResult tags_round_trip_through_a_run_record(){
    for (const StoryFailureClassInfo& info : story_failure_classes()){
        const std::string field = std::format("\"failure\": \"{}\"", story_failure_class_tag(info.failure_class));
        const std::optional<StoryFailureClass> got = read_story_failure_class_tag(field);
        if (got != info.failure_class){
            return std::format("{} reads back as {}", field, id_or_none(got));
        }
    }
    return true;
}

UnitTestResult no_class_without_a_tag(){
    const std::string_view lines[] = {
        "[Checkpoint] 00.04: failed at 2 talk_mom: expect_failed: has_parcel",
        "[Checkpoint] 00.04: failed (class=no_such_class)",
        "class=",
    };
    for (std::string_view line : lines){
        const std::optional<StoryFailureClass> got = read_story_failure_class_tag(line);
        if (got){
            return std::format("\"{}\" reads as {}", line, id_or_none(got));
        }
    }
    return true;
}

const ListCheckTestCase LIST_CHECKS[] = {
    {.name = "rows in the enum's order", .input = {rows_in_enum_order}, .expectation = {"story_failure_class_info() indexes by the enum"}},
    {.name = "every sim fault maps to one class", .input = {every_sim_fault_maps_to_one_class}, .expectation = {"the sweeps' faults all have a class"}},
    {.name = "no outcome name in two classes", .input = {no_outcome_in_two_classes}, .expectation = {"a failure name has one class"}},
    {.name = "ids round-trip", .input = {ids_round_trip}, .expectation = {"each id is unique and its tag alone reads back"}},
    {.name = "tags round-trip through a log line", .input = {tags_round_trip_through_a_log_line}, .expectation = {"the run log names the class"}},
    {.name = "tags round-trip through a run record", .input = {tags_round_trip_through_a_run_record}, .expectation = {"a run record names the class"}},
    {.name = "no class without a tag", .input = {no_class_without_a_tag}, .expectation = {"untagged or unknown text has no class"}},
};

} // namespace


void add_tests_StoryFailureClasses(UnitTestDatabase& database){
    for (const OutcomeClassTestCase& test_case : OUTCOME_CASES){
        database.add<Test_OutcomeClass>(test_case);
    }
    for (const ListCheckTestCase& test_case : LIST_CHECKS){
        database.add<Test_ListCheck>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
