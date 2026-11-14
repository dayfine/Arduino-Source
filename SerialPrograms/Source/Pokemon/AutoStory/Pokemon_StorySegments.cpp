/*  Story Segments
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon_CheckpointAttempts.h"
#include "Pokemon_StoryLog.h"
#include "Pokemon_StorySegments.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  A line for the log, and without its tag for the status sink, if set.
void say(const std::string& line, SegmentHooks& hooks){
    if (hooks.log){
        hooks.log(line);
    }
    if (hooks.status){
        hooks.status(story_short_line(line));
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
        selected.failure = outcome_failed("bad_range", std::format("{} is after {}", *range.from, *range.to));
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

//  One row to success through run_checkpoint_attempts(), reloading and
//  rebuilding after each failed attempt. Returns the last attempt's report,
//  or the reload's failure; `run` says how many attempts it took.
CheckpointReport run_row(
    const Segment& segment, const SegmentPolicy& policy, CheckpointSpec& spec,
    SegmentHooks& hooks, SegmentReport& report, CheckpointRun& run, RunState& state
){
    CheckpointReport result;
    const CheckpointAttempts attempts{
        .max_attempts = policy.attempts_per_checkpoint,
        .run = [&](size_t attempt){
            run.attempts = attempt;
            Checkpoint checkpoint = spec.build();
            if (spec.saves && !checkpoint.save){
                checkpoint.save = [&hooks](RunState& state){
                    return hooks.save ? hooks.save(state) : outcome_failed("no_save_hook");
                };
            }
            EventLogger log = [&hooks](const std::string& line){ say(line, hooks); };
            result = run_checkpoint(state, checkpoint, log);
            if (result.completed){
                return AttemptResult::PASSED;
            }
            if (result.failure.failure == GAME_NOT_RUNNING){
                say(std::format("[Segment] {}: {} failed; the game is not running, so no reload", segment.id, spec.id), hooks);
                return AttemptResult::FINAL;
            }
            return AttemptResult::FAILED;
        },
        .reset = [&](size_t next_attempt){
            say(checkpoint_reset_line(spec.id, next_attempt, policy.attempts_per_checkpoint), hooks);
            report.reloads++;
            Outcome reloaded = reload(state, hooks);
            if (!reloaded.succeeded()){
                result = failed_report(std::move(reloaded));
                return false;
            }
            return true;
        },
        .log = [&hooks](const std::string& line){ say(line, hooks); },
    };
    run.passed = run_checkpoint_attempts(spec.id, attempts).passed;
    return result;
}

SegmentReport stop_segment(const std::string& id, Outcome failure, SegmentHooks& hooks, SegmentReport& report){
    std::string where;
    if (!report.failed_checkpoint.empty()){
        where = std::format(" at {}", report.failed_checkpoint);
        if (!report.failed_event.empty()){
            where += std::format(" {}", report.failed_event);
        }
    }
    say(std::format("[Segment] {}: failed{}: {}", id, where, to_string(failure)), hooks);
    report.failure = std::move(failure);
    return report;
}

} // namespace


SegmentReport run_segment(
    const SegmentRange& range, const SegmentPolicy& policy,
    Segment& segment, SegmentHooks& hooks, RunState& state
){
    SegmentReport report;
    const Selected selected = select(segment, range);
    if (!selected.failure.succeeded()){
        return stop_segment(segment.id, selected.failure, hooks, report);
    }
    say(std::format("[Segment] {}: start, {} checkpoints", segment.id, selected.last - selected.first + 1), hooks);
    for (size_t c = selected.first; c <= selected.last; c++){
        CheckpointSpec& spec = segment.checkpoints[c];
        report.checkpoints_run++;
        report.runs.push_back(CheckpointRun{.id = spec.id});
        CheckpointReport result = run_row(segment, policy, spec, hooks, report, report.runs.back(), state);
        if (!result.completed){
            report.failed_checkpoint = spec.id;
            report.failed_event = result.failed_name;
            return stop_segment(segment.id, std::move(result.failure), hooks, report);
        }
    }
    report.completed = true;
    say(std::format("[Segment] {}: {} of {} checkpoints one-shot", segment.id, report.one_shot_passes(), report.checkpoints_run), hooks);
    say(std::format("[Segment] {}: done, {} checkpoints", segment.id, report.checkpoints_run), hooks);
    return report;
}


namespace{

//  One verify run: the checkpoint built fresh, without its end save.
//  Returns true if it passed.
bool verify_run(
    size_t run, const VerifyPolicy& policy, CheckpointSpec& spec, SegmentHooks& hooks, RunState& state
){
    Checkpoint checkpoint = spec.build();
    checkpoint.save = nullptr;
    EventLogger log = [&hooks](const std::string& line){ say(line, hooks); };
    const CheckpointReport result = run_checkpoint(state, checkpoint, log);
    if (result.completed){
        say(std::format("[Verify] {}: run {}/{} passed", spec.id, run, policy.runs), hooks);
        return true;
    }
    say(std::format(
        "[Verify] {}: run {}/{} failed{}: {}",
        spec.id, run, policy.runs,
        result.failed_name.empty() ? "" : std::format(" at {}", result.failed_name),
        to_string(result.failure)
    ), hooks);
    return false;
}

VerifyReport verify_summary(const VerifyPolicy& policy, SegmentHooks& hooks, VerifyReport& report){
    report.verified = report.failure.succeeded() && report.passes >= policy.passes_needed;
    say(std::format(
        "[Verify] {}: {}/{} passed, {} needed: {}",
        report.checkpoint, report.passes, policy.runs, policy.passes_needed,
        report.verified ? "verified" : "not verified"
    ), hooks);
    return report;
}

} // namespace


VerifyReport verify_checkpoint(
    const std::string& id, const VerifyPolicy& policy,
    Segment& segment, SegmentHooks& hooks, RunState& state
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
        say(std::format("[Verify] {}: {}", id, to_string(report.failure)), hooks);
        return report;
    }
    CheckpointSpec& spec = segment.checkpoints[*index];
    say(std::format(
        "[Verify] {}: start, {} runs from one save, {} must pass", id, policy.runs, policy.passes_needed
    ), hooks);
    for (size_t run = 1; run <= policy.runs; run++){
        if (run > 1){
            Outcome reloaded = reload(state, hooks);
            if (!reloaded.succeeded()){
                say(std::format("[Verify] {}: no reload before run {}: {}", id, run, to_string(reloaded)), hooks);
                report.failure = std::move(reloaded);
                break;
            }
        }
        report.runs++;
        report.passes += verify_run(run, policy, spec, hooks, state) ? 1 : 0;
    }
    return verify_summary(policy, hooks, report);
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
