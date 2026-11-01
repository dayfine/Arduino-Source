/*  Story Segments
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
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

//  The policy's in-place recovery, after each event's own: the game's
//  recover hook, if any, then the event again. Counts into `used`.
void add_policy_recoveries(Checkpoint& checkpoint, const SegmentPolicy& policy, SegmentHooks& hooks, size_t& used){
    if (policy.recoveries_per_event == 0){
        return;
    }
    for (Event& event : checkpoint.events){
        Recovery recovery;
        recovery.kind = RecoveryKind::REANCHOR;
        recovery.attempts = policy.recoveries_per_event;
        recovery.why = "the segment's policy: recover in place before reloading";
        recovery.prepare = [&hooks, &used, name = event.name](RunState& state){
            used++;
            return hooks.recover ? hooks.recover(state, name) : outcome_ok();
        };
        event.recoveries.push_back(std::move(recovery));
    }
}

std::string resets_text(size_t attempts){
    if (attempts == 1){
        return "one-shot";
    }
    return std::format("{} reset{}", attempts - 1, attempts == 2 ? "" : "s");
}

//  One row to success, reloading and rebuilding after each failed attempt.
//  Returns the last attempt's report, or the reload's failure; `run` says
//  how many attempts and recoveries it took.
CheckpointReport run_row(
    RunState& state, const Segment& segment, CheckpointSpec& spec,
    const SegmentPolicy& policy, SegmentHooks& hooks, SegmentReport& report, CheckpointRun& run
){
    for (size_t attempt = 1; ; attempt++){
        run.attempts = attempt;
        Checkpoint checkpoint = spec.build();
        if (spec.saves && !checkpoint.save){
            checkpoint.save = [&hooks](RunState& state){
                return hooks.save ? hooks.save(state) : outcome_failed("no_save_hook");
            };
        }
        add_policy_recoveries(checkpoint, policy, hooks, run.recoveries);
        EventLogger log = [&hooks](const std::string& line){ say(hooks, line); };
        CheckpointReport result = run_checkpoint(state, checkpoint, log);
        if (result.completed){
            run.passed = true;
            say(hooks, std::format(
                "[Checkpoint] {}: passed on attempt {} ({}){}",
                spec.id, attempt, resets_text(attempt),
                run.recoveries == 0 ? "" : std::format(", {} in place", run.recoveries)
            ));
            return result;
        }
        if (policy.stop_at_first_failure){
            say(hooks, std::format(
                "[Segment] {}: {} failed; stopping at the first failure, the game stays where it failed",
                segment.id, spec.id
            ));
            report.stopped_at_first_failure = true;
            return result;
        }
        if (attempt >= policy.attempts_per_checkpoint){
            return result;
        }
        say(hooks, std::format(
            "[Segment] {}: {} failed, reloading for attempt {}/{}",
            segment.id, spec.id, attempt + 1, policy.attempts_per_checkpoint
        ));
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

}


bool CheckpointRun::one_shot() const{
    return passed && attempts == 1 && recoveries == 0;
}

size_t SegmentReport::one_shot_passes() const{
    size_t count = 0;
    for (const CheckpointRun& run : runs){
        count += run.one_shot() ? 1 : 0;
    }
    return count;
}


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
    for (size_t c = selected.first; c <= selected.last; c++){
        CheckpointSpec& spec = segment.checkpoints[c];
        report.checkpoints_run++;
        report.runs.push_back(CheckpointRun{.id = spec.id});
        CheckpointReport result = run_row(state, segment, spec, policy, hooks, report, report.runs.back());
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



}
}
