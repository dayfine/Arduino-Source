/*  Story Save Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  When a run saves (Types/Pokemon_StorySaveMode.h): the decision table,
 *  then the segment runner and verify mode in each mode against a world
 *  that only records what happened. Then the postcondition check
 *  (Pokemon_StoryPostconditionCheck.h) on FRLG 00.31's end state.
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
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Data/Pokemon_StoryStepJson.h"
#include "Pokemon_StoryPostconditionCheck.h"
#include "Pokemon_StorySavePolicy_Tests.h"
#include "Pokemon_StorySegments.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  One row of the decision table.
struct SaveDecision{
    StorySaveMode mode;
    CheckpointVerdict verdict;
    SavePoint point;
    bool saves;
};

constexpr SaveDecision SAVE_DECISIONS[] = {
    {StorySaveMode::NONE,                CheckpointVerdict::FAILED, SavePoint::CHECKPOINT_END, false},
    {StorySaveMode::NONE,                CheckpointVerdict::FAILED, SavePoint::RUN_END,        false},
    {StorySaveMode::NONE,                CheckpointVerdict::PASSED, SavePoint::CHECKPOINT_END, false},
    {StorySaveMode::NONE,                CheckpointVerdict::PASSED, SavePoint::RUN_END,        false},
    {StorySaveMode::AFTER_VERIFICATION,  CheckpointVerdict::FAILED, SavePoint::CHECKPOINT_END, false},
    {StorySaveMode::AFTER_VERIFICATION,  CheckpointVerdict::FAILED, SavePoint::RUN_END,        false},
    {StorySaveMode::AFTER_VERIFICATION,  CheckpointVerdict::PASSED, SavePoint::CHECKPOINT_END, false},
    {StorySaveMode::AFTER_VERIFICATION,  CheckpointVerdict::PASSED, SavePoint::RUN_END,        true},
    {StorySaveMode::PER_CHECKPOINT,      CheckpointVerdict::FAILED, SavePoint::CHECKPOINT_END, false},
    {StorySaveMode::PER_CHECKPOINT,      CheckpointVerdict::FAILED, SavePoint::RUN_END,        false},
    {StorySaveMode::PER_CHECKPOINT,      CheckpointVerdict::PASSED, SavePoint::CHECKPOINT_END, true},
    {StorySaveMode::PER_CHECKPOINT,      CheckpointVerdict::PASSED, SavePoint::RUN_END,        true},
};

UnitTestResult test_decision_table(){
    for (const SaveDecision& row : SAVE_DECISIONS){
        if (story_save_allowed(row.mode, row.verdict, row.point) != row.saves){
            return std::format(
                "Wrong decision for {}{}{}", story_save_mode_name(row.mode),
                row.verdict == CheckpointVerdict::PASSED ? ", passed" : ", failed",
                row.point == SavePoint::RUN_END ? ", run end" : ", mid-run"
            );
        }
    }
    return true;
}


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
    return run_segment(state, segment, SegmentRange{}, SegmentPolicy{.save_mode = mode}, hooks);
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

//  FRLG 00.31, 2026-11-10: the run said it passed on the wrong floor.
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
    return verify_checkpoint(state, segment, "00.02", policy, hooks);
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


//  FRLG 00.31's end, in the shared format: beside B3F's Rare Candy, with
//  the LIFT KEY and both Rare Candies.
inline constexpr std::string_view CHECKPOINT_0031 = R"({
    "id": "00.31",
    "title": "Rocket Hideout to the LIFT KEY",
    "start": {"map": "CeladonCity_PokemonCenter_1F", "tiles": [[7, 4]], "facing": "north"},
    "events": [
        {"id": "beside_b3f_rare_candy", "kind": "route", "args": {"target": {"map": "RocketHideout_B3F", "tiles": [[12, 13]], "facing": "north"}}},
        {"id": "b3f_rare_candy", "kind": "talk"}
    ],
    "metadata": {"phase": "testing", "postconditions": {
        "position": {"map": "RocketHideout_B3F", "tiles": [[12, 13]], "facing": "north"},
        "items": [{"item": "LIFT_KEY", "quantity": 1}, {"item": "RARE_CANDY", "quantity": 2}]
    }}
})";

StoryCheckpoint checkpoint_0031(){
    return read_story_checkpoint(parse_json(std::string(CHECKPOINT_0031)), "00.31");
}

ObservedState read_0031(std::string map, int candies){
    ObservedState observed;
    observed.map = std::move(map);
    observed.tile = TilePos{12, 13};
    observed.facing = Direction::NORTH;
    observed.items = {{"LIFT_KEY", 1}, {"RARE_CANDY", (size_t)candies}};
    return observed;
}

UnitTestResult test_postconditions_json(){
    const StoryCheckpoint checkpoint = checkpoint_0031();
    const StoryPostconditions& post = checkpoint.postconditions;
    if (!post.position || post.position->map != "RocketHideout_B3F" || post.items.size() != 2
        || post.items[1] != StoryItem{.item = "RARE_CANDY", .quantity = 2}
    ){
        return "Unexpected postconditions read.";
    }
    const StoryCheckpoint again = read_story_checkpoint(story_checkpoint_json(checkpoint), "again");
    if (again.postconditions != post){
        return std::format("The postconditions should survive a write and read: {}", story_checkpoint_json(checkpoint).dump());
    }
    return true;
}

UnitTestResult test_postconditions_pass(){
    const PostconditionReport report = check_postconditions(checkpoint_0031(), read_0031("RocketHideout_B3F", 2));
    const std::vector<std::string> lines = postcondition_log_lines(report);
    if (!report.all_passed() || lines.back() != "[Postcondition] 00.31: 5 passed, 0 failed, 0 not checked: passed"){
        return std::format("Expected every postcondition passed: {}", lines.back());
    }
    return true;
}

//  The 2026-11-10 false pass: still on B4F, the Rare Candy not picked up.
UnitTestResult test_postconditions_fail(){
    const PostconditionReport report = check_postconditions(checkpoint_0031(), read_0031("RocketHideout_B4F", 1));
    const std::vector<std::string> lines = postcondition_log_lines(report);
    const std::vector<std::string> expected = {
        "[Postcondition] 00.31 map: failed: expected RocketHideout_B3F, read RocketHideout_B4F",
        "[Postcondition] 00.31 tile: not checked: on another map",
        "[Postcondition] 00.31 facing: not checked: on another map",
        "[Postcondition] 00.31 item LIFT_KEY: passed: 1",
        "[Postcondition] 00.31 item RARE_CANDY: failed: expected 2, read 1",
        "[Postcondition] 00.31: 1 passed, 2 failed, 2 not checked: failed",
    };
    if (lines != expected){
        std::string text;
        for (const std::string& line : lines){
            text += std::format("\n{}", line);
        }
        return std::format("Unexpected lines:{}", text);
    }
    return true;
}

UnitTestResult test_postconditions_not_checked(){
    StoryCheckpoint checkpoint = checkpoint_0031();
    checkpoint.postconditions.flags = {"hideout_lift_key_grunt"};
    ObservedState observed;
    observed.map = "RocketHideout_B3F";
    const PostconditionReport report = check_postconditions(checkpoint, observed);
    const std::vector<std::string> lines = postcondition_log_lines(report);
    if (report.all_passed() || report.count(PostconditionVerdict::NOT_CHECKED) != 5
        || lines.back() != "[Postcondition] 00.31: 1 passed, 0 failed, 5 not checked: incomplete"
    ){
        return std::format("What isn't read is not checked, never passed: {}", lines.back());
    }
    observed.flags["hideout_lift_key_grunt"] = false;
    if (check_postconditions(checkpoint, observed).count(PostconditionVerdict::FAILED) != 1){
        return "A flag read clear should fail.";
    }
    return true;
}

UnitTestResult test_end_pose_from_the_plan(){
    StoryCheckpoint checkpoint = checkpoint_0031();
    checkpoint.postconditions.position.reset();
    const std::optional<GridPose> end = checkpoint_end_pose(checkpoint);
    if (!end || end->map != "RocketHideout_B3F" || end->tiles != std::vector<TilePos>{TilePos{12, 13}}){
        return "Expected the last route's target as the end.";
    }
    StoryStep legs;
    legs.kind = StoryStepKind::LEGS;
    legs.name = "taps";
    checkpoint.steps.push_back(std::move(legs));
    if (checkpoint_end_pose(checkpoint)){
        return "Legs after the last target leave the end unknown.";
    }
    if (!check_postconditions(checkpoint, read_0031("RocketHideout_B3F", 2)).all_passed()){
        return "Without an end pose, the items alone should be checked.";
    }
    return true;
}


//  The scenario to run.
struct SavePolicyInput{
    std::string name;
    UnitTestResult (*test)() = nullptr;
};

//  What the scenario checks; it returns its own failure.
struct SavePolicyExpectation{
    std::string_view description;
};

// A save-policy scenario and its expected result.
struct SavePolicyCase{
    SavePolicyInput input;
    SavePolicyExpectation expectation;
};

class Test_StorySavePolicy : public UnitTest{
public:
    explicit Test_StorySavePolicy(SavePolicyCase test_case)
        : UnitTest(std::format("Pokemon::StorySavePolicy - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.test();
    }

private:
    SavePolicyCase m_case;
};

const SavePolicyCase SAVE_POLICY_CASES[] = {
    {
        .input = {.name = "decision table", .test = test_decision_table},
        .expectation = {.description = "mode x verdict x save point, all twelve rows"},
    },
    {
        .input = {.name = "none never saves", .test = test_none_never_saves},
        .expectation = {.description = "three passing checkpoints, no save, each skip logged"},
    },
    {
        .input = {.name = "per checkpoint saves each", .test = test_per_checkpoint_saves_each},
        .expectation = {.description = "a save after every passing checkpoint"},
    },
    {
        .input = {.name = "after verification saves at the end", .test = test_after_verification_saves_at_the_end},
        .expectation = {.description = "one save after the last checkpoint; no check logged as not checked"},
    },
    {
        .input = {.name = "after verification checks before saving", .test = test_after_verification_checks_before_saving},
        .expectation = {.description = "the game's postcondition check runs before the save"},
    },
    {
        .input = {.name = "failed postconditions never save", .test = test_failed_postconditions_never_save},
        .expectation = {.description = "a failed check fails the run and refuses the save"},
    },
    {
        .input = {.name = "no reload after the first checkpoint", .test = test_no_reload_after_the_first},
        .expectation = {.description = "a later checkpoint has no save to reload, so it fails at once"},
    },
    {
        .input = {.name = "the first checkpoint still reloads", .test = test_first_row_still_reloads},
        .expectation = {.description = "the first checkpoint reloads the run's start save"},
    },
    {
        .input = {.name = "verify saves once every run passed", .test = test_verify_saves_once_every_run_passed},
        .expectation = {.description = "3/3 passed: one save at the end of run 3"},
    },
    {
        .input = {.name = "verify: no save after a failed run", .test = test_verify_no_save_after_a_failed_run},
        .expectation = {.description = "2/3 verified, but no save"},
    },
    {
        .input = {.name = "postconditions: json", .test = test_postconditions_json},
        .expectation = {.description = "00.31's postconditions read and written back"},
    },
    {
        .input = {.name = "postconditions: pass", .test = test_postconditions_pass},
        .expectation = {.description = "B3F (12, 13) north, LIFT_KEY 1, RARE_CANDY 2: passed"},
    },
    {
        .input = {.name = "postconditions: fail", .test = test_postconditions_fail},
        .expectation = {.description = "still on B4F with 1 Rare Candy: failed"},
    },
    {
        .input = {.name = "postconditions: not checked", .test = test_postconditions_not_checked},
        .expectation = {.description = "unread parts are not checked, never passed"},
    },
    {
        .input = {.name = "postconditions: end pose from the plan", .test = test_end_pose_from_the_plan},
        .expectation = {.description = "the last route's target, or unknown after legs"},
    },
};

} // namespace


void add_tests_StorySavePolicy(UnitTestDatabase& database){
    for (const SavePolicyCase& test_case : SAVE_POLICY_CASES){
        database.add<Test_StorySavePolicy>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
