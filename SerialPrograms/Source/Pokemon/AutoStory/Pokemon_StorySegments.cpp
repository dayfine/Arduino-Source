/*  Story Segments
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon_StoryLog.h"
#include "Pokemon_StorySegments.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  "[Event] 00.03/2 x: start" as "00.03/2 x: start", for the status sink.
std::string without_tag(const std::string& line){
    if (line.empty() || line[0] != '['){
        return line;
    }
    const size_t end = line.find("] ");
    return end == std::string::npos ? line : line.substr(end + 2);
}

//  A line for the log, and without its tag for the status sink, if set.
void say(SegmentHooks& hooks, const std::string& line){
    if (hooks.log){
        hooks.log(line);
    }
    if (hooks.status){
        hooks.status(without_tag(line));
    }
}

std::optional<size_t> index_of(const Segment& segment, const std::string& id){
    for (size_t c = 0; c < segment.checkpoints.size(); c++){
        if (segment.checkpoints[c].id == id){
            return c;
        }
    }
    return std::nullopt;
}

//  The rows `range` names, both inclusive, or the failure that stops the
//  segment before it starts.
struct Selected{
    size_t first = 0;
    size_t last = 0;
    Outcome failure;
};

Selected select(const Segment& segment, const SegmentRange& range){
    Selected selected;
    if (segment.checkpoints.empty()){
        selected.failure = outcome_failed("empty_segment");
        return selected;
    }
    selected.last = segment.checkpoints.size() - 1;
    if (range.from){
        const std::optional<size_t> index = index_of(segment, *range.from);
        if (!index){
            selected.failure = outcome_failed("unknown_checkpoint", *range.from);
            return selected;
        }
        selected.first = *index;
    }
    if (range.to){
        const std::optional<size_t> index = index_of(segment, *range.to);
        if (!index){
            selected.failure = outcome_failed("unknown_checkpoint", *range.to);
            return selected;
        }
        selected.last = *index;
    }
    if (selected.first > selected.last){
        selected.failure = outcome_failed("bad_range", *range.from + " is after " + *range.to);
    }
    return selected;
}

CheckpointReport failed_report(Outcome failure){
    CheckpointReport report;
    report.failure = std::move(failure);
    return report;
}

//  Empties the run state and reloads the last save; the reload says where
//  the player stands.
Outcome reload(RunState& state, SegmentHooks& hooks){
    state = RunState{};
    if (!hooks.reload){
        return outcome_failed("reload_failed", "no reload hook");
    }
    Outcome reloaded = hooks.reload(state);
    if (!reloaded.succeeded()){
        return outcome_failed("reload_failed", to_string(reloaded));
    }
    if (reloaded.pose){
        state.pose = *reloaded.pose;
    }
    return outcome_ok();
}

//  The segment's save hook as a checkpoint's save.
Act hook_save(SegmentHooks& hooks){
    return [&hooks](RunState& state){
        return hooks.save ? hooks.save(state) : outcome_failed("no_save_hook");
    };
}

//  `save`, after the game's postcondition check of checkpoint `id`: one
//  that failed stops the save. Without a check, logs that nothing was
//  checked, never that it passed.
Act checked_save(Act save, std::string id, SegmentHooks& hooks){
    return [save = std::move(save), id = std::move(id), &hooks](RunState& state) mutable{
        if (!hooks.check_postconditions){
            say(hooks, std::format("[Postcondition] {}: not checked: the game has no postcondition check", id));
            return save(state);
        }
        const Outcome checked = hooks.check_postconditions(state, id);
        if (!checked.succeeded()){
            say(hooks, std::format("[Checkpoint] {}: postconditions failed, not saving", id));
            return outcome_failed("postconditions_failed", to_string(checked));
        }
        return save(state);
    };
}

//  A row to run, and where it stands in the run.
struct Row{
    CheckpointSpec& spec;
    //  The run's first: its save is the one the run started from.
    bool first = false;
    //  The run's last: it ends at the run's end state.
    bool last = false;
};

//  The checkpoint's save as `policy.save_mode` allows: one the mode skips
//  logs that it did not save; the run's last in AFTER_VERIFICATION saves
//  (with the segment's hook if it has none) after the postcondition check.
void set_save(Checkpoint& checkpoint, const Row& row, const SegmentPolicy& policy, SegmentHooks& hooks){
    const StorySaveMode mode = policy.save_mode;
    const bool saves_at_end = mode == StorySaveMode::AFTER_VERIFICATION && row.last;
    if ((row.spec.saves || saves_at_end) && !checkpoint.save){
        checkpoint.save = hook_save(hooks);
    }
    if (!checkpoint.save){
        return;
    }
    const SavePoint point = row.last ? SavePoint::RUN_END : SavePoint::CHECKPOINT_END;
    if (!story_save_allowed(mode, CheckpointVerdict::PASSED, point)){
        checkpoint.save = [&hooks, id = row.spec.id, mode](RunState&){
            say(hooks, std::format("[Checkpoint] {}: not saving (save mode {})", id, story_save_mode_name(mode)));
            return outcome_ok();
        };
        return;
    }
    if (saves_at_end){
        checkpoint.save = checked_save(std::move(checkpoint.save), row.spec.id, hooks);
    }
}

//  Attempts the row gets: a row after the first has a save at its start
//  only when every checkpoint saves, so otherwise a reload would go back
//  to the run's start, and it gets one.
size_t row_attempts(const Row& row, const SegmentPolicy& policy){
    if (row.first || policy.save_mode == StorySaveMode::PER_CHECKPOINT){
        return policy.attempts_per_checkpoint;
    }
    return 1;
}

//  One row to success, reloading and rebuilding after each failed attempt.
//  Returns the last attempt's report, or the reload's failure; `run` says
//  how many attempts it took.
CheckpointReport run_row(
    RunState& state, const Segment& segment, const Row& row,
    const SegmentPolicy& policy, SegmentHooks& hooks, SegmentReport& report, CheckpointRun& run
){
    const size_t attempts = row_attempts(row, policy);
    for (size_t attempt = 1; ; attempt++){
        run.attempts = attempt;
        Checkpoint checkpoint = row.spec.build();
        set_save(checkpoint, row, policy, hooks);
        EventLogger log = [&hooks](const std::string& line){ say(hooks, line); };
        CheckpointReport result = run_checkpoint(state, checkpoint, log);
        if (result.completed){
            run.passed = true;
            say(hooks, checkpoint_pass_line(row.spec.id, attempt));
            return result;
        }
        if (result.failure.failure == GAME_NOT_RUNNING){
            say(hooks, std::format("[Segment] {}: {} failed; the game is not running, so no reload", segment.id, row.spec.id));
            return result;
        }
        if (attempt >= attempts){
            if (attempts < policy.attempts_per_checkpoint){
                say(hooks, std::format(
                    "[Segment] {}: {} failed; save mode {} left no save at its start, so no reload",
                    segment.id, row.spec.id, story_save_mode_name(policy.save_mode)
                ));
            }
            return result;
        }
        say(hooks, segment_reload_line(segment.id, row.spec.id, attempt + 1, policy.attempts_per_checkpoint));
        report.reloads++;
        Outcome reloaded = reload(state, hooks);
        if (!reloaded.succeeded()){
            return failed_report(std::move(reloaded));
        }
    }
}

SegmentReport stop_segment(SegmentReport& report, SegmentHooks& hooks, const std::string& id, Outcome failure){
    std::string where;
    if (!report.failed_checkpoint.empty()){
        where = " at " + report.failed_checkpoint;
        if (!report.failed_event.empty()){
            where += " " + report.failed_event;
        }
    }
    say(hooks, std::format("[Segment] {}: failed{}: {}", id, where, to_string(failure)));
    report.failure = std::move(failure);
    return report;
}

} // namespace


SegmentReport run_segment(
    RunState& state, Segment& segment, const SegmentRange& range,
    const SegmentPolicy& policy, SegmentHooks& hooks
){
    SegmentReport report;
    const Selected selected = select(segment, range);
    if (!selected.failure.succeeded()){
        return stop_segment(report, hooks, segment.id, selected.failure);
    }
    say(hooks, std::format("[Segment] {}: start, {} checkpoints", segment.id, selected.last - selected.first + 1));
    if (policy.save_mode != StorySaveMode::PER_CHECKPOINT){
        say(hooks, std::format("[Segment] {}: save mode {}", segment.id, story_save_mode_name(policy.save_mode)));
    }
    for (size_t c = selected.first; c <= selected.last; c++){
        CheckpointSpec& spec = segment.checkpoints[c];
        report.checkpoints_run++;
        report.runs.push_back(CheckpointRun{.id = spec.id});
        const Row row{.spec = spec, .first = c == selected.first, .last = c == selected.last};
        CheckpointReport result = run_row(state, segment, row, policy, hooks, report, report.runs.back());
        if (!result.completed){
            report.failed_checkpoint = spec.id;
            report.failed_event = result.failed_name;
            return stop_segment(report, hooks, segment.id, std::move(result.failure));
        }
    }
    report.completed = true;
    say(hooks, std::format("[Segment] {}: {} of {} checkpoints one-shot", segment.id, report.one_shot_passes(), report.checkpoints_run));
    say(hooks, std::format("[Segment] {}: done, {} checkpoints", segment.id, report.checkpoints_run));
    return report;
}


namespace{

//  One verify run: the checkpoint built fresh. Its end save only if
//  `save_at_end` (AFTER_VERIFICATION's last run, every run before it
//  passed), after the postcondition check. Returns true if it passed.
bool verify_run(
    RunState& state, CheckpointSpec& spec, size_t run, const VerifyPolicy& policy, SegmentHooks& hooks,
    bool save_at_end
){
    Checkpoint checkpoint = spec.build();
    if (save_at_end){
        Act save = checkpoint.save ? std::move(checkpoint.save) : hook_save(hooks);
        checkpoint.save = checked_save(std::move(save), spec.id, hooks);
    }else{
        checkpoint.save = nullptr;
    }
    EventLogger log = [&hooks](const std::string& line){ say(hooks, line); };
    const CheckpointReport result = run_checkpoint(state, checkpoint, log);
    if (result.completed){
        say(hooks, std::format("[Verify] {}: run {}/{} passed", spec.id, run, policy.runs));
        return true;
    }
    say(hooks, std::format(
        "[Verify] {}: run {}/{} failed{}: {}",
        spec.id, run, policy.runs,
        result.failed_name.empty() ? "" : " at " + result.failed_name,
        to_string(result.failure)
    ));
    return false;
}

//  AFTER_VERIFICATION saves at the end of the last run when every run
//  before it passed.
bool verify_saves_after(const VerifyReport& report, const VerifyPolicy& policy, size_t run){
    return policy.save_mode == StorySaveMode::AFTER_VERIFICATION && run == policy.runs && report.passes + 1 == run;
}

VerifyReport verify_summary(VerifyReport& report, const VerifyPolicy& policy, SegmentHooks& hooks){
    if (policy.save_mode == StorySaveMode::AFTER_VERIFICATION){
        say(hooks, std::format(
            "[Verify] {}: {}", report.checkpoint,
            report.saved ? "every run passed: saved at the end state" : "not every run passed: not saving"
        ));
    }
    report.verified = report.failure.succeeded() && report.passes >= policy.passes_needed;
    say(hooks, std::format(
        "[Verify] {}: {}/{} passed, {} needed: {}",
        report.checkpoint, report.passes, policy.runs, policy.passes_needed,
        report.verified ? "verified" : "not verified"
    ));
    return report;
}

} // namespace


VerifyReport verify_checkpoint(
    RunState& state, Segment& segment, const std::string& id,
    const VerifyPolicy& policy, SegmentHooks& hooks
){
    VerifyReport report;
    report.checkpoint = id;
    const std::optional<size_t> index = index_of(segment, id);
    if (!index){
        report.failure = outcome_failed("unknown_checkpoint", id);
    }else if (policy.runs == 0 || policy.passes_needed > policy.runs){
        report.failure = outcome_failed("bad_verify_policy", std::format("{} of {} runs", policy.passes_needed, policy.runs));
    }
    if (!report.failure.succeeded()){
        say(hooks, std::format("[Verify] {}: {}", id, to_string(report.failure)));
        return report;
    }
    CheckpointSpec& spec = segment.checkpoints[*index];
    say(hooks, std::format(
        "[Verify] {}: start, {} runs from one save, {} must pass", id, policy.runs, policy.passes_needed
    ));
    for (size_t run = 1; run <= policy.runs; run++){
        if (run > 1){
            Outcome reloaded = reload(state, hooks);
            if (!reloaded.succeeded()){
                say(hooks, std::format("[Verify] {}: no reload before run {}: {}", id, run, to_string(reloaded)));
                report.failure = std::move(reloaded);
                break;
            }
        }
        report.runs++;
        const bool save_at_end = verify_saves_after(report, policy, run);
        const bool passed = verify_run(state, spec, run, policy, hooks, save_at_end);
        report.passes += passed ? 1 : 0;
        report.saved = save_at_end && passed;
    }
    return verify_summary(report, policy, hooks);
}


std::string describe(Segment& segment){
    std::string text = std::format("segment {}: {} checkpoints\n", segment.id, segment.checkpoints.size());
    for (const CheckpointSpec& spec : segment.checkpoints){
        text += std::format("  {} \"{}\"{}\n", spec.id, spec.title, spec.saves ? ", saves" : "");
    }
    for (CheckpointSpec& spec : segment.checkpoints){
        if (spec.build){
            text += describe(spec.build());
        }
    }
    return text;
}



} // namespace Pokemon
} // namespace PokemonAutomation
