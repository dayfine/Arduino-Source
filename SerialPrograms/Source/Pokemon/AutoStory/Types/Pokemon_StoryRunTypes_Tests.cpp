/*  Story Run Types Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  When a run may save (Pokemon_StorySaveMode.h), the phase names a
 *  checkpoint's metadata uses (Pokemon_StoryCheckpointMetadata.h), and
 *  where a run pauses (Pokemon_StoryBreakpoints.h).
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryBreakpoints.h"
#include "Pokemon_StoryCheckpointMetadata.h"
#include "Pokemon_StoryRunTypes_Tests.h"
#include "Pokemon_StorySaveMode.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  One row of the save decision: mode x verdict x save point.
struct SaveDecisionCase{
    struct Input{
        StorySaveMode mode;
        CheckpointVerdict verdict;
        SavePoint point;
    };

    struct Expectation{
        bool saves;
    };

    Input input;
    Expectation expectation;
};

std::string decision_name(const SaveDecisionCase::Input& input){
    return std::format(
        "{}, {}, {}", story_save_mode_name(input.mode),
        input.verdict == CheckpointVerdict::PASSED ? "passed" : "failed",
        input.point == SavePoint::RUN_END ? "run end" : "checkpoint end"
    );
}

class Test_SaveDecision : public UnitTest{
public:
    explicit Test_SaveDecision(SaveDecisionCase test_case)
        : UnitTest(std::format("Pokemon::StoryRunTypes - save decision: {}", decision_name(test_case.input)))
        , m_case(test_case)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const SaveDecisionCase::Input& in = m_case.input;
        const bool saves = story_save_allowed(in.mode, in.verdict, in.point);
        if (saves != m_case.expectation.saves){
            return std::format("expected {}, got {}", m_case.expectation.saves ? "a save" : "no save", saves ? "a save" : "no save");
        }
        return true;
    }

private:
    SaveDecisionCase m_case;
};

using M = StorySaveMode;
using V = CheckpointVerdict;
using S = SavePoint;

//  A failed checkpoint never saves; NONE never saves; AFTER_VERIFICATION
//  saves only at the run's end; PER_CHECKPOINT at every checkpoint's end.
const SaveDecisionCase SAVE_DECISIONS[] = {
    {.input = {M::NONE, V::FAILED, S::CHECKPOINT_END}, .expectation = {.saves = false}},
    {.input = {M::NONE, V::FAILED, S::RUN_END}, .expectation = {.saves = false}},
    {.input = {M::NONE, V::PASSED, S::CHECKPOINT_END}, .expectation = {.saves = false}},
    {.input = {M::NONE, V::PASSED, S::RUN_END}, .expectation = {.saves = false}},
    {.input = {M::AFTER_VERIFICATION, V::FAILED, S::CHECKPOINT_END}, .expectation = {.saves = false}},
    {.input = {M::AFTER_VERIFICATION, V::FAILED, S::RUN_END}, .expectation = {.saves = false}},
    {.input = {M::AFTER_VERIFICATION, V::PASSED, S::CHECKPOINT_END}, .expectation = {.saves = false}},
    {.input = {M::AFTER_VERIFICATION, V::PASSED, S::RUN_END}, .expectation = {.saves = true}},
    {.input = {M::PER_CHECKPOINT, V::FAILED, S::CHECKPOINT_END}, .expectation = {.saves = false}},
    {.input = {M::PER_CHECKPOINT, V::FAILED, S::RUN_END}, .expectation = {.saves = false}},
    {.input = {M::PER_CHECKPOINT, V::PASSED, S::CHECKPOINT_END}, .expectation = {.saves = true}},
    {.input = {M::PER_CHECKPOINT, V::PASSED, S::RUN_END}, .expectation = {.saves = true}},
};


//  A named check that returns its own failure.
struct NamedCheck{
    std::string_view name;
    UnitTestResult (*check)(CancellableScope& scope);
};

class Test_NamedCheck : public UnitTest{
public:
    explicit Test_NamedCheck(NamedCheck check)
        : UnitTest(std::format("Pokemon::StoryRunTypes - {}", check.name))
        , m_check(check)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return m_check.check(scope);
    }

private:
    const NamedCheck m_check;
};


UnitTestResult phase_names_round_trip(CancellableScope&){
    for (StoryPhase phase : {StoryPhase::TESTING, StoryPhase::VERIFICATION, StoryPhase::REPRODUCTION}){
        if (read_story_phase(story_phase_name(phase)) != phase){
            return std::format("\"{}\" does not read back", story_phase_name(phase));
        }
    }
    if (read_story_phase("Testing") || read_story_phase("")){
        return std::string("read a phase from a name that is none");
    }
    return true;
}

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n    " + line;
    }
    return text;
}

//  The labels each pause point paused on, in `mode`: one step with two legs.
std::vector<std::string> paused_on(StoryBreakpoints mode){
    std::vector<std::string> paused;
    StoryPausePoints points(mode, [&](std::string_view label){
        paused.emplace_back(label);
    });
    points.before_step("12.03/1 first");
    points.before_leg("leg 1/2 east 3");
    points.before_leg("leg 2/2 north 1");
    points.before_step("12.03/2 second");
    return paused;
}

UnitTestResult pause_points_by_mode(CancellableScope&){
    if (!paused_on(StoryBreakpoints::OFF).empty()){
        return std::format("OFF paused:{}", joined(paused_on(StoryBreakpoints::OFF)));
    }
    const std::vector<std::string> steps = {"12.03/1 first", "12.03/2 second"};
    if (paused_on(StoryBreakpoints::STEPS) != steps){
        return std::format("STEPS paused:{}", joined(paused_on(StoryBreakpoints::STEPS)));
    }
    const std::vector<std::string> legs = {
        "12.03/1 first", "12.03/1 first leg 1/2 east 3", "12.03/1 first leg 2/2 north 1", "12.03/2 second",
    };
    if (paused_on(StoryBreakpoints::LEGS) != legs){
        return std::format("LEGS paused:{}", joined(paused_on(StoryBreakpoints::LEGS)));
    }
    return true;
}

UnitTestResult pause_points_log_and_keep_the_label(CancellableScope&){
    std::vector<std::string> lines;
    StoryPausePoints points(StoryBreakpoints::STEPS, [](std::string_view){});
    points.set_log([&](std::string_view line){
        lines.emplace_back(line);
    });
    points.before_step("12.03/1 first");
    const std::vector<std::string> expected = {"[Break] 12.03/1 first: paused", "[Break] 12.03/1 first: continued"};
    if (lines != expected){
        return std::format("unexpected log:{}", joined(lines));
    }
    if (points.step_label() != "12.03/1 first"){
        return std::format("step label \"{}\"", points.step_label());
    }
    StoryPausePoints no_hook(StoryBreakpoints::LEGS, {});
    no_hook.set_log([&](std::string_view line){
        lines.emplace_back(line);
    });
    no_hook.before_step("12.03/2 second");
    if (lines.size() != expected.size() || no_hook.step_label() != "12.03/2 second"){
        return std::format("a pause point with no hook logged or lost its label:{}", joined(lines));
    }
    return true;
}

UnitTestResult pause_hook_from_a_wait(CancellableScope& scope){
    if (make_story_pause_hook({}, scope)){
        return std::string("expected no hook without a wait");
    }
    std::vector<std::string> waited;
    const StoryPauseHook hook = make_story_pause_hook([&](std::string_view label, CancellableScope& with){
        waited.push_back(std::format("{} {}", label, &with == &scope));
    }, scope);
    if (!hook){
        return std::string("expected a hook for a wait");
    }
    hook("12.03/1 first");
    if (waited != std::vector<std::string>{"12.03/1 first true"}){
        return std::format("expected the wait to get the label and the scope:{}", joined(waited));
    }
    return true;
}

UnitTestResult pause_labels(CancellableScope&){
    if (story_step_pause_label("12.03", 4, "pallet_shore") != "12.03/5 pallet_shore"){
        return story_step_pause_label("12.03", 4, "pallet_shore");
    }
    const std::string with_end = story_leg_pause_label(1, 5, Direction::EAST, 4, TilePos{10, 10});
    if (with_end != "leg 2/5 east 4 -> (10, 10)"){
        return with_end;
    }
    const std::string without_end = story_leg_pause_label(0, 1, Direction::NORTH, 2, std::nullopt);
    if (without_end != "leg 1/1 north 2"){
        return without_end;
    }
    return true;
}

const NamedCheck CHECKS[] = {
    {.name = "phase names round-trip", .check = phase_names_round_trip},
    {.name = "pause points by mode", .check = pause_points_by_mode},
    {.name = "pause points log and keep the label", .check = pause_points_log_and_keep_the_label},
    {.name = "pause hook from a wait", .check = pause_hook_from_a_wait},
    {.name = "pause labels", .check = pause_labels},
};


} // namespace


void add_tests_StoryRunTypes(UnitTestDatabase& database){
    for (const SaveDecisionCase& test_case : SAVE_DECISIONS){
        database.add<Test_SaveDecision>(test_case);
    }
    for (const NamedCheck& check : CHECKS){
        database.add<Test_NamedCheck>(check);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
