/*  Program Transition - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Who each start and stop of a program is said to come from, as
 *  ProgramSession reports its steps to ProgramTransitionLatch.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "ProgramTransition.h"
#include "ProgramTransition_Tests.h"

namespace PokemonAutomation{

namespace{

using namespace std::chrono_literals;


constexpr std::string_view REQUEST_ID = "5f0c2a8e-3d41-4b7a-9c55-0e6a1f2b7c10";

//  One step ProgramSession reports.
enum class Step{
    UI_START,
    LAB_START,
    UI_STOP,
    LAB_STOP,
    ERROR_REPORTED,
    FINISHED,
};
struct LatchInput{
    std::vector<Step> steps;
};
struct LatchExpectation{
    std::optional<ProgramAction> action;    //  None: no transition yet.
    ActionSource source = ActionSource::UI;
    bool has_request_id = false;
    size_t at_step = 0;     //  The step (from 1) whose time the transition has.
};
struct LatchCase{
    std::string name;
    LatchInput input;
    LatchExpectation expectation;
};

WallClock step_time(size_t step){
    return WallClock{} + Seconds(1000 + step);
}
void apply(Step step, WallClock now, ProgramTransitionLatch& latch){
    switch (step){
    case Step::UI_START:
        latch.started(ActionSource::UI, std::nullopt, now);
        return;
    case Step::LAB_START:
        latch.started(ActionSource::LAB, std::string(REQUEST_ID), now);
        return;
    case Step::UI_STOP:
        latch.stop_requested(ActionSource::UI, std::nullopt, now);
        return;
    case Step::LAB_STOP:
        latch.stop_requested(ActionSource::LAB, std::string(REQUEST_ID), now);
        return;
    case Step::ERROR_REPORTED:
        latch.error_reported();
        return;
    case Step::FINISHED:
        latch.finished(now);
        return;
    }
}
UnitTestResult check(const LatchCase& c){
    ProgramTransitionLatch latch;
    for (size_t i = 0; i < c.input.steps.size(); i++){
        apply(c.input.steps[i], step_time(i + 1), latch);
    }
    const std::optional<ProgramTransition> last = latch.last();
    const LatchExpectation& expected = c.expectation;
    if (!expected.action){
        return last ? UnitTestResult("expected no transition") : UnitTestResult(true);
    }
    if (!last || last->action != *expected.action || last->source != expected.source){
        return "wrong action or source";
    }
    if (last->request_id.has_value() != expected.has_request_id){
        return "a request id goes with a lab request only";
    }
    if (last->at != step_time(expected.at_step)){
        return "the transition has the time of the wrong step";
    }
    return true;
}
class Test_ProgramTransitionLatch : public UnitTest{
public:
    explicit Test_ProgramTransitionLatch(LatchCase test_case)
        : UnitTest(std::format("CommonFramework::ProgramTransition - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    LatchCase m_case;
};
const LatchCase LATCH_CASES[] = {
    {.name = "nothing yet", .input = {.steps = {}}, .expectation = {.action = std::nullopt}},
    {
        .name = "UI start",
        .input = {.steps = {Step::UI_START}},
        .expectation = {.action = ProgramAction::START, .source = ActionSource::UI, .at_step = 1},
    },
    {
        .name = "lab start",
        .input = {.steps = {Step::LAB_START}},
        .expectation = {.action = ProgramAction::START, .source = ActionSource::LAB, .has_request_id = true, .at_step = 1},
    },
    {
        .name = "UI stop after a lab start",
        .input = {.steps = {Step::LAB_START, Step::UI_STOP, Step::FINISHED}},
        .expectation = {.action = ProgramAction::STOP, .source = ActionSource::UI, .at_step = 2},
    },
    {
        .name = "lab stop is latched through the end of the run",
        .input = {.steps = {Step::UI_START, Step::LAB_STOP, Step::FINISHED}},
        .expectation = {.action = ProgramAction::STOP, .source = ActionSource::LAB, .has_request_id = true, .at_step = 2},
    },
    {
        .name = "error while stopping keeps who stopped",
        .input = {.steps = {Step::UI_START, Step::LAB_STOP, Step::ERROR_REPORTED, Step::FINISHED}},
        .expectation = {.action = ProgramAction::STOP, .source = ActionSource::LAB, .has_request_id = true, .at_step = 2},
    },
    {
        .name = "program finished by itself",
        .input = {.steps = {Step::LAB_START, Step::FINISHED}},
        .expectation = {.action = ProgramAction::STOP, .source = ActionSource::PROGRAM, .at_step = 2},
    },
    {
        .name = "program ended with an error",
        .input = {.steps = {Step::UI_START, Step::ERROR_REPORTED, Step::FINISHED}},
        .expectation = {.action = ProgramAction::STOP, .source = ActionSource::PROGRAM_ERROR, .at_step = 3},
    },
    {
        .name = "error of an earlier run is forgotten",
        .input = {.steps = {Step::UI_START, Step::ERROR_REPORTED, Step::FINISHED, Step::UI_START, Step::FINISHED}},
        .expectation = {.action = ProgramAction::STOP, .source = ActionSource::PROGRAM, .at_step = 5},
    },
    {
        .name = "stop of an earlier run is forgotten",
        .input = {.steps = {Step::UI_START, Step::LAB_STOP, Step::FINISHED, Step::UI_START, Step::FINISHED}},
        .expectation = {.action = ProgramAction::STOP, .source = ActionSource::PROGRAM, .at_step = 5},
    },
};


}  // namespace


void add_tests_ProgramTransition(UnitTestDatabase& database){
    for (const LatchCase& c : LATCH_CASES){
        database.add<Test_ProgramTransitionLatch>(c);
    }
}



}  // namespace PokemonAutomation
