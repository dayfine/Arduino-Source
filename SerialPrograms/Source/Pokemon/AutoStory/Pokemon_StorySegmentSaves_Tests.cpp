/*  Story Segment Saves Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The checkpoint retry loop (Pokemon_CheckpointAttempts.h) on scripted
 *  attempts, then when the segment runner and verify mode save in each save
 *  mode (Types/Pokemon_StorySaveMode.h), against a world that only records
 *  what happened.
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_CheckpointAttempts.h"
#include "Pokemon_StorySegmentSaves_Tests.h"
#include "Pokemon_StorySegments.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The retry loop given each attempt's result in turn.
struct AttemptsCase{
    std::string_view name;
    struct Input{
        size_t max_attempts;
        std::vector<AttemptResult> results;
        //  The reset before this attempt fails; 0 for none.
        size_t failed_reset = 0;
    };

    struct Expectation{
        bool passed;
        size_t attempts;
        //  The attempts and resets, in order: "run 1", "reset 2", ...
        std::vector<std::string> trace;
    };

    Input input;
    Expectation expectation;
};

using A = AttemptResult;

const AttemptsCase ATTEMPTS_CASES[] = {
    {
        .name = "passes first time",
        .input = {.max_attempts = 3, .results = {A::PASSED}},
        .expectation = {.passed = true, .attempts = 1, .trace = {"run 1", "[Checkpoint] 00.04: passed on attempt 1 (one-shot)"}},
    },
    {
        .name = "passes after a reset",
        .input = {.max_attempts = 3, .results = {A::FAILED, A::PASSED}},
        .expectation = {.passed = true, .attempts = 2, .trace = {"run 1", "reset 2", "run 2", "[Checkpoint] 00.04: passed on attempt 2 (1 reset)"}},
    },
    {
        .name = "stops at max_attempts",
        .input = {.max_attempts = 2, .results = {A::FAILED, A::FAILED, A::PASSED}},
        .expectation = {.passed = false, .attempts = 2, .trace = {"run 1", "reset 2", "run 2"}},
    },
    {
        .name = "FINAL stops without a reset",
        .input = {.max_attempts = 3, .results = {A::FINAL, A::PASSED}},
        .expectation = {.passed = false, .attempts = 1, .trace = {"run 1"}},
    },
    {
        .name = "a failed reset stops",
        .input = {.max_attempts = 3, .results = {A::FAILED, A::PASSED}, .failed_reset = 2},
        .expectation = {.passed = false, .attempts = 1, .trace = {"run 1", "reset 2"}},
    },
    {
        .name = "max_attempts 0 still runs once",
        .input = {.max_attempts = 0, .results = {A::FAILED, A::PASSED}},
        .expectation = {.passed = false, .attempts = 1, .trace = {"run 1"}},
    },
};

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += std::format("\n{}", line);
    }
    return text;
}

class Test_CheckpointAttempts : public UnitTest{
public:
    explicit Test_CheckpointAttempts(AttemptsCase test_case)
        : UnitTest(std::format("Pokemon::StorySegmentSaves - attempts: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<std::string> trace;
        const AttemptsCase::Input& in = m_case.input;
        const CheckpointAttempts attempts{
            .max_attempts = in.max_attempts,
            .run = [&](size_t attempt){
                trace.push_back(std::format("run {}", attempt));
                return in.results.at(attempt - 1);
            },
            .reset = [&](size_t next_attempt){
                trace.push_back(std::format("reset {}", next_attempt));
                return next_attempt != in.failed_reset;
            },
            .log = [&](const std::string& line){
                trace.push_back(line);
            },
        };
        const CheckpointAttemptsResult result = run_checkpoint_attempts("00.04", attempts);
        const AttemptsCase::Expectation& expected = m_case.expectation;
        if (result.passed != expected.passed || result.attempts != expected.attempts || trace != expected.trace){
            return std::format(
                "expected {} after {} attempts, got {} after {}:{}",
                expected.passed ? "a pass" : "no pass", expected.attempts,
                result.passed ? "a pass" : "no pass", result.attempts, joined(trace)
            );
        }
        return true;
    }

private:
    const AttemptsCase m_case;
};


//  A world of three checkpoints that only records what happened: each
//  checkpoint's walk, each save and each reload.
struct Lab{
    std::vector<std::string> trace;
    std::vector<std::string> lines;
    //  Checkpoints whose walk fails this many more times.
    std::map<std::string, int> failures;
    //  The game's postcondition check: none when unset.
    std::optional<Outcome> postconditions;

    SegmentHooks hooks(){
        SegmentHooks hooks;
        hooks.save = [this](RunState&){
            trace.push_back("save");
            return outcome_ok();
        };
        hooks.reload = [this](RunState&){
            trace.push_back("reload");
            return outcome_ok();
        };
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        if (postconditions){
            hooks.check_postconditions = [this](const std::string& id, RunState&){
                trace.push_back(std::format("check {}", id));
                return *postconditions;
            };
        }
        return hooks;
    }
    bool has(const std::string& line) const{
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    }
    std::string all() const{
        std::string text;
        for (const std::string& line : lines){
            text += std::format("\n{}", line);
        }
        return text;
    }
};

CheckpointSpec walk_row(std::string id, Lab& lab){
    CheckpointSpec spec;
    spec.id = id;
    spec.saves = true;
    spec.build = [&lab, id]{
        Checkpoint checkpoint;
        checkpoint.id = id;
        Event walk;
        walk.name = "walk";
        walk.act = [&lab, id](RunState&){
            lab.trace.push_back(id);
            int& left = lab.failures[id];
            if (left > 0){
                left--;
                return outcome_failed("blocked", "a wanderer on the path");
            }
            return outcome_ok();
        };
        Recovery stop;
        stop.kind = RecoveryKind::STOP;
        walk.recoveries.push_back(std::move(stop));
        checkpoint.events.push_back(std::move(walk));
        return checkpoint;
    };
    return spec;
}

Segment three_rows(Lab& lab){
    Segment segment;
    segment.id = "00";
    for (std::string_view id : {"00.01", "00.02", "00.03"}){
        segment.checkpoints.push_back(walk_row(std::string(id), lab));
    }
    return segment;
}

SegmentReport run_in(StorySaveMode mode, Lab& lab){
    Segment segment = three_rows(lab);
    SegmentHooks hooks = lab.hooks();
    RunState state;
    return run_segment(SegmentRange{}, SegmentPolicy{.save_mode = mode}, segment, hooks, state);
}

UnitTestResult test_none_never_saves(){
    Lab lab;
    const SegmentReport report = run_in(StorySaveMode::NONE, lab);
    const std::vector<std::string> expected = {"00.01", "00.02", "00.03"};
    if (!report.completed || lab.trace != expected){
        return std::format("Expected three checkpoints and no save:{}", lab.all());
    }
    if (!lab.has("[Segment] 00: save mode none") || !lab.has("[Checkpoint] 00.02: not saving (save mode none)")){
        return std::format("Expected the skipped saves logged:{}", lab.all());
    }
    return true;
}

UnitTestResult test_per_checkpoint_saves_each(){
    Lab lab;
    const SegmentReport report = run_in(StorySaveMode::PER_CHECKPOINT, lab);
    const std::vector<std::string> expected = {"00.01", "save", "00.02", "save", "00.03", "save"};
    if (!report.completed || lab.trace != expected){
        return std::format("Expected a save after each checkpoint:{}", lab.all());
    }
    return true;
}

UnitTestResult test_after_verification_saves_at_the_end(){
    Lab lab;
    const SegmentReport report = run_in(StorySaveMode::AFTER_VERIFICATION, lab);
    const std::vector<std::string> expected = {"00.01", "00.02", "00.03", "save"};
    if (!report.completed || lab.trace != expected){
        return std::format("Expected one save, at the end:{}", lab.all());
    }
    if (!lab.has("[Postcondition] 00.03: not checked: the game has no postcondition check")){
        return std::format("Expected the missing check logged as not checked:{}", lab.all());
    }
    return true;
}

UnitTestResult test_after_verification_checks_before_saving(){
    Lab lab;
    lab.postconditions = outcome_ok();
    const SegmentReport report = run_in(StorySaveMode::AFTER_VERIFICATION, lab);
    const std::vector<std::string> expected = {"00.01", "00.02", "00.03", "check 00.03", "save"};
    if (!report.completed || lab.trace != expected){
        return std::format("Expected the check, then the save:{}", lab.all());
    }
    return true;
}

//  A run whose game reports it on the wrong floor at the end must not save.
UnitTestResult test_failed_postconditions_never_save(){
    Lab lab;
    lab.postconditions = outcome_failed("postconditions", "map: expected RocketHideout_B3F, read RocketHideout_B4F");
    const SegmentReport report = run_in(StorySaveMode::AFTER_VERIFICATION, lab);
    if (report.completed || report.failed_checkpoint != "00.03"){
        return std::format("Expected the run failed at its last checkpoint:{}", lab.all());
    }
    if (std::find(lab.trace.begin(), lab.trace.end(), "save") != lab.trace.end()){
        return std::format("A run whose postconditions failed must not save:{}", lab.all());
    }
    if (!lab.has("[Checkpoint] 00.03: postconditions failed, not saving")){
        return std::format("Expected the refused save logged:{}", lab.all());
    }
    return true;
}

//  Without per-checkpoint saves, a reload would go back to the run's start.
UnitTestResult test_no_reload_after_the_first(){
    Lab lab;
    lab.failures["00.02"] = 1;
    const SegmentReport report = run_in(StorySaveMode::NONE, lab);
    const std::vector<std::string> expected = {"00.01", "00.02"};
    if (report.completed || report.reloads != 0 || lab.trace != expected){
        return std::format("Expected 00.02 to fail without a reload:{}", lab.all());
    }
    if (!lab.has("[Segment] 00: 00.02 failed; save mode none left no save at its start, so no reload")){
        return std::format("Expected why it didn't reload:{}", lab.all());
    }
    return true;
}

UnitTestResult test_first_row_still_reloads(){
    Lab lab;
    lab.failures["00.01"] = 1;
    const SegmentReport report = run_in(StorySaveMode::NONE, lab);
    const std::vector<std::string> expected = {"00.01", "reload", "00.01", "00.02", "00.03"};
    if (!report.completed || report.reloads != 1 || lab.trace != expected){
        return std::format("Expected the first checkpoint retried from the start save:{}", lab.all());
    }
    return true;
}

VerifyReport verify_in(StorySaveMode mode, Lab& lab){
    Segment segment = three_rows(lab);
    SegmentHooks hooks = lab.hooks();
    RunState state;
    const VerifyPolicy policy{.runs = 3, .passes_needed = 2, .save_mode = mode};
    return verify_checkpoint("00.02", policy, segment, hooks, state);
}

UnitTestResult test_verify_saves_once_every_run_passed(){
    Lab lab;
    const VerifyReport report = verify_in(StorySaveMode::AFTER_VERIFICATION, lab);
    const std::vector<std::string> expected = {"00.02", "reload", "00.02", "reload", "00.02", "save"};
    if (!report.verified || !report.saved || lab.trace != expected){
        return std::format("Expected one save, after the last run:{}", lab.all());
    }
    if (!lab.has("[Verify] 00.02: every run passed: saved at the end state")){
        return std::format("Expected the save logged:{}", lab.all());
    }
    return true;
}

UnitTestResult test_verify_no_save_after_a_failed_run(){
    Lab lab;
    lab.failures["00.02"] = 1;
    const VerifyReport report = verify_in(StorySaveMode::AFTER_VERIFICATION, lab);
    if (!report.verified || report.saved || std::find(lab.trace.begin(), lab.trace.end(), "save") != lab.trace.end()){
        return std::format("Verified 2/3, but a failed run means no save:{}", lab.all());
    }
    if (!lab.has("[Verify] 00.02: not every run passed: not saving")){
        return std::format("Expected why it didn't save:{}", lab.all());
    }
    return true;
}


//  A named test; it returns its own failure.
struct NamedTest{
    std::string_view name;
    UnitTestResult (*test)();
};

class Test_SegmentSaves : public UnitTest{
public:
    explicit Test_SegmentSaves(NamedTest test)
        : UnitTest(std::format("Pokemon::StorySegmentSaves - {}", test.name))
        , m_test(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_test.test();
    }

private:
    const NamedTest m_test;
};

const NamedTest SEGMENT_SAVE_TESTS[] = {
    {.name = "none never saves", .test = test_none_never_saves},
    {.name = "per checkpoint saves each", .test = test_per_checkpoint_saves_each},
    {.name = "after verification saves at the end", .test = test_after_verification_saves_at_the_end},
    {.name = "after verification checks before saving", .test = test_after_verification_checks_before_saving},
    {.name = "failed postconditions never save", .test = test_failed_postconditions_never_save},
    {.name = "no reload after the first checkpoint", .test = test_no_reload_after_the_first},
    {.name = "the first checkpoint still reloads", .test = test_first_row_still_reloads},
    {.name = "verify saves once every run passed", .test = test_verify_saves_once_every_run_passed},
    {.name = "verify: no save after a failed run", .test = test_verify_no_save_after_a_failed_run},
};


} // namespace


void add_tests_StorySegmentSaves(UnitTestDatabase& database){
    for (const AttemptsCase& test_case : ATTEMPTS_CASES){
        database.add<Test_CheckpointAttempts>(test_case);
    }
    for (const NamedTest& test : SEGMENT_SAVE_TESTS){
        database.add<Test_SegmentSaves>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
