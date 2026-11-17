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
            say(std::format("[Postcondition] {}: not checked: the game has no postcondition check", id), hooks);
            return save(state);
        }
        const Outcome checked = hooks.check_postconditions(id, state);
        if (!checked.succeeded()){
            say(checkpoint_log_line(id, "postconditions failed, not saving"), hooks);
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
void set_save(const Row& row, const SegmentPolicy& policy, SegmentHooks& hooks, Checkpoint& checkpoint){
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
            say(checkpoint_log_line(id, std::format("not saving (save mode {})", story_save_mode_name(mode))), hooks);
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

//  One row to success through run_checkpoint_attempts(), reloading and
//  rebuilding after each failed attempt. Returns the last attempt's report,
//  or the reload's failure; `run` says how many attempts it took.
CheckpointReport run_row(
    const Segment& segment, const Row& row, const SegmentPolicy& policy,
    SegmentHooks& hooks, SegmentReport& report, CheckpointRun& run, RunState& state
){
    const size_t max_attempts = row_attempts(row, policy);
    CheckpointReport result;
    const CheckpointAttempts attempts{
        .max_attempts = max_attempts,
        .run = [&](size_t attempt){
            run.attempts = attempt;
            Checkpoint checkpoint = row.spec.build();
            set_save(row, policy, hooks, checkpoint);
            EventLogger log = [&hooks](const std::string& line){ say(line, hooks); };
            result = run_checkpoint(state, checkpoint, log);
            if (result.completed){
                return AttemptResult::PASSED;
            }
            if (result.failure.failure == GAME_NOT_RUNNING){
                say(std::format("[Segment] {}: {} failed; the game is not running, so no reload", segment.id, row.spec.id), hooks);
                return AttemptResult::FINAL;
            }
            if (attempt >= max_attempts && max_attempts < policy.attempts_per_checkpoint){
                say(std::format(
                    "[Segment] {}: {} failed; save mode {} left no save at its start, so no reload",
                    segment.id, row.spec.id, story_save_mode_name(policy.save_mode)
                ), hooks);
            }
            return AttemptResult::FAILED;
        },
        .reset = [&](size_t next_attempt){
            say(checkpoint_reset_line(row.spec.id, next_attempt, policy.attempts_per_checkpoint), hooks);
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
    run.passed = run_checkpoint_attempts(row.spec.id, attempts).passed;
    return result;
}

//  Whether a verify try that completed passes: the game's postcondition
//  check, then its pass predicate.
Outcome verify_pass(const std::string& id, SegmentHooks& hooks, RunState& state){
    if (hooks.check_postconditions){
        const Outcome checked = hooks.check_postconditions(id, state);
        if (!checked.succeeded()){
            return outcome_failed("postconditions_failed", to_string(checked));
        }
    }
    if (hooks.verify_pass){
        const Outcome passed = hooks.verify_pass(id, state);
        if (!passed.succeeded()){
            return outcome_failed("verify_no_pass", to_string(passed));
        }
    }
    return outcome_ok();
}

//  The verification loop of a row with `verify` (run_segment()).
class VerifyRow{
public:
    VerifyRow(const Row& row, const SegmentPolicy& policy, SegmentHooks& hooks, SegmentReport& report, CheckpointRun& run, RunState& state)
        : m_row(row)
        , m_verify(*row.spec.verify)
        , m_policy(policy)
        , m_hooks(hooks)
        , m_report(report)
        , m_run(run)
        , m_state(state)
    {}

    CheckpointReport run(){
        const std::string& id = m_row.spec.id;
        say(std::format(
            "[Verify] {}: phase 1, {} tries from one save, {} must pass; phase 2, up to {} tries, the first pass saved",
            id, m_verify.tries, m_verify.min_pass, m_verify.reset_budget
        ), m_hooks);
        size_t passes = 0;
        for (size_t c = 1; c <= m_verify.tries; c++){
            const std::optional<bool> passed = try_once(/*phase=*/1, c, m_verify.tries);
            if (!passed){
                return std::move(m_result);
            }
            passes += *passed ? 1 : 0;
        }
        if (passes < m_verify.min_pass){
            return stop("verify_below_min_pass", std::format("phase 1 passed {} of {}, under {}", passes, m_verify.tries, m_verify.min_pass));
        }
        for (size_t c = 1; c <= m_verify.reset_budget; c++){
            const std::optional<bool> passed = try_once(/*phase=*/2, c, m_verify.reset_budget);
            if (!passed){
                return std::move(m_result);
            }
            if (*passed){
                m_run.passed = true;
                say(std::format("[Verify] {}: phase 2 try {} passed and saved", id, c), m_hooks);
                return std::move(m_result);
            }
        }
        return stop("verify_out_of_budget", std::format("no pass in {} phase 2 tries", m_verify.reset_budget));
    }

private:
    CheckpointReport stop(std::string failure, std::string detail){
        say(std::format("[Verify] {}: {}; stopping", m_row.spec.id, detail), m_hooks);
        m_result = failed_report(outcome_failed(std::move(failure), std::move(detail)));
        return std::move(m_result);
    }

    //  The checkpoint built fresh; phase 1 checks the pass in place of a
    //  save, phase 2 before the save.
    Checkpoint build(size_t phase){
        Checkpoint checkpoint = m_row.spec.build();
        if (phase == 1){
            checkpoint.save = nullptr;
        }else{
            set_save(m_row, m_policy, m_hooks, checkpoint);
        }
        checkpoint.save = [save = std::move(checkpoint.save), id = m_row.spec.id, &hooks = m_hooks](RunState& state) mutable{
            const Outcome passed = verify_pass(id, hooks, state);
            if (!passed.succeeded() || !save){
                return passed;
            }
            return save(state);
        };
        return checkpoint;
    }

    //  One try, reloading the start save first after the first; whether it
    //  passed, or none when the loop stops (no reload, the start check
    //  failed, the game is not running).
    std::optional<bool> try_once(size_t phase, size_t index, size_t of){
        const std::string& id = m_row.spec.id;
        if (m_run.attempts > 0){
            m_report.reloads++;
            Outcome reloaded = reload(m_state, m_hooks);
            if (!reloaded.succeeded()){
                m_result = failed_report(std::move(reloaded));
                return std::nullopt;
            }
        }
        m_run.attempts++;
        Checkpoint checkpoint = build(phase);
        EventLogger log = [&hooks = m_hooks](const std::string& line){ say(line, hooks); };
        m_result = run_checkpoint(m_state, checkpoint, log);
        if (m_result.completed){
            say(std::format("[Verify] {}: phase {} try {}/{} passed", id, phase, index, of), m_hooks);
            return true;
        }
        say(std::format(
            "[Verify] {}: phase {} try {}/{} failed{}: {}", id, phase, index, of,
            m_result.failed_name.empty() ? "" : std::format(" at {}", m_result.failed_name), to_string(m_result.failure)
        ), m_hooks);
        if (m_result.failure.failure == "start_failed" || m_result.failure.failure == GAME_NOT_RUNNING){
            say(std::format("[Verify] {}: stopping", id), m_hooks);
            return std::nullopt;
        }
        return false;
    }

    const Row& m_row;
    const StoryVerify& m_verify;
    const SegmentPolicy& m_policy;
    SegmentHooks& m_hooks;
    SegmentReport& m_report;
    CheckpointRun& m_run;
    RunState& m_state;
    CheckpointReport m_result;
};

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
    if (policy.save_mode != StorySaveMode::PER_CHECKPOINT){
        say(std::format("[Segment] {}: save mode {}", segment.id, story_save_mode_name(policy.save_mode)), hooks);
    }
    for (size_t c = selected.first; c <= selected.last; c++){
        CheckpointSpec& spec = segment.checkpoints[c];
        report.checkpoints_run++;
        report.runs.push_back(CheckpointRun{.id = spec.id});
        const Row row{.spec = spec, .first = c == selected.first, .last = c == selected.last};
        CheckpointReport result = spec.verify
            ? VerifyRow(row, policy, hooks, report, report.runs.back(), state).run()
            : run_row(segment, row, policy, hooks, report, report.runs.back(), state);
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

//  One verify run: the checkpoint built fresh. Its end save only if
//  `save_at_end` (AFTER_VERIFICATION's last run, every run before it
//  passed), after the postcondition check. Returns true if it passed.
bool verify_run(
    size_t run, const VerifyPolicy& policy, bool save_at_end,
    CheckpointSpec& spec, SegmentHooks& hooks, RunState& state
){
    Checkpoint checkpoint = spec.build();
    if (save_at_end){
        Act save = checkpoint.save ? std::move(checkpoint.save) : hook_save(hooks);
        checkpoint.save = checked_save(std::move(save), spec.id, hooks);
    }else{
        checkpoint.save = nullptr;
    }
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

//  AFTER_VERIFICATION saves at the end of the last run when every run
//  before it passed.
bool verify_saves_after(const VerifyReport& report, const VerifyPolicy& policy, size_t run){
    return policy.save_mode == StorySaveMode::AFTER_VERIFICATION && run == policy.runs && report.passes + 1 == run;
}

VerifyReport verify_summary(const VerifyPolicy& policy, SegmentHooks& hooks, VerifyReport& report){
    if (policy.save_mode == StorySaveMode::AFTER_VERIFICATION){
        say(std::format(
            "[Verify] {}: {}", report.checkpoint,
            report.saved ? "every run passed: saved at the end state" : "not every run passed: not saving"
        ), hooks);
    }
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
        const bool save_at_end = verify_saves_after(report, policy, run);
        const bool passed = verify_run(run, policy, save_at_end, spec, hooks, state);
        report.passes += passed ? 1 : 0;
        report.saved = save_at_end && passed;
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
