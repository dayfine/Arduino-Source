/*  Story Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <exception>
#include <format>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_StoryEvents.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string certainty_name(PoseCertainty certainty){
    switch (certainty){
    case PoseCertainty::TILE:
        return "tile";
    case PoseCertainty::COLUMN:
        return "column";
    case PoseCertainty::ROW:
        return "row";
    case PoseCertainty::SET:
        return "set";
    case PoseCertainty::UNKNOWN:
        return "unknown";
    }
    return "?";
}

bool certainty_covers(PoseCertainty have, PoseCertainty need){
    switch (need){
    case PoseCertainty::UNKNOWN:
        return true;
    case PoseCertainty::SET:
        return have != PoseCertainty::UNKNOWN;
    case PoseCertainty::COLUMN:
    case PoseCertainty::ROW:
        return have == need || have == PoseCertainty::TILE;
    case PoseCertainty::TILE:
        return have == PoseCertainty::TILE;
    }
    return false;
}

std::string to_string(const Pose& pose){
    std::string text = pose.map.empty() ? "?" : pose.map;
    if (pose.where){
        const std::string where = pose.where->describe();
        if (!where.empty()){
            text += std::format(" {}", where);
        }
    }
    return std::format("{} ({})", text, certainty_name(pose.certainty));
}


bool Outcome::succeeded() const{
    return failure.empty();
}
Outcome outcome_ok(std::optional<Pose> pose){
    Outcome outcome;
    outcome.pose = std::move(pose);
    return outcome;
}

Outcome outcome_unchanged(){
    Outcome outcome;
    outcome.changed_nothing = true;
    return outcome;
}

Outcome outcome_failed(std::string failure, std::string detail){
    Outcome outcome;
    outcome.failure = std::move(failure);
    outcome.detail = std::move(detail);
    return outcome;
}
std::string to_string(const Outcome& outcome){
    return outcome.detail.empty() ? outcome.failure : std::format("{}: {}", outcome.failure, outcome.detail);
}

Outcome outcome_game_not_running(){
    return outcome_failed(std::string(GAME_NOT_RUNNING), "the game is not running: the Switch HOME menu is showing");
}

Condition condition(std::string name, Check check){
    return Condition{std::move(name), std::move(check)};
}

Condition pose_known(std::string map, PoseCertainty need){
    std::string name = std::format("pose {}{}", certainty_name(need), map.empty() ? "" : std::format(" on {}", map));
    return condition(std::move(name), [map = std::move(map), need](RunState& state){
        return (map.empty() || state.pose.map == map) && certainty_covers(state.pose.certainty, need);
    });
}

std::string recovery_name(RecoveryKind kind){
    switch (kind){
    case RecoveryKind::WAIT:
        return "wait";
    case RecoveryKind::REANCHOR:
        return "re-anchor";
    case RecoveryKind::STOP:
        return "stop";
    }
    return "?";
}


namespace{

//  Runs an action; if it succeeded knowing where it left the player, that
//  becomes the run's pose.
Outcome run_act(Act& act, RunState& state){
    Outcome outcome = act(state);
    if (outcome.succeeded() && outcome.pose){
        state.pose = *outcome.pose;
    }
    return outcome;
}

//  The name of the first condition that does not hold.
std::optional<std::string> first_failing(std::vector<Condition>& conditions, RunState& state){
    for (Condition& item : conditions){
        if (!item.check(state)){
            return item.name;
        }
    }
    return std::nullopt;
}

bool already_done(Event& event, RunState& state){
    return !event.skip_if.empty() && !first_failing(event.skip_if, state);
}

Outcome check_expects(Event& event, RunState& state){
    std::optional<std::string> failing = first_failing(event.expect, state);
    if (failing){
        return outcome_failed("expect_failed", *failing);
    }
    if (event.pose_learned){
        state.pose = *event.pose_learned;
    }
    return outcome_ok();
}

Outcome attempt_event(Event& event, RunState& state){
    std::optional<std::string> failing = first_failing(event.require, state);
    if (failing){
        return outcome_failed("require_failed", *failing);
    }
    Outcome outcome = run_act(event.act, state);
    if (!outcome.succeeded()){
        return outcome;
    }
    return check_expects(event, state);
}

} // namespace

void apply_interrupt_pose(InterruptPose effect, Pose& pose){
    switch (effect){
    case InterruptPose::UNCHANGED:
        return;
    case InterruptPose::WITHIN_SET:
        if (pose.certainty != PoseCertainty::UNKNOWN){
            pose.certainty = PoseCertainty::SET;
        }
        return;
    case InterruptPose::LOST:
        pose.where.reset();
        pose.certainty = PoseCertainty::UNKNOWN;
        return;
    }
}

namespace{


//  One event being run, with what it has used up.
struct EventRun{
    Checkpoint& checkpoint;
    size_t number;
    Event& event;
    RunState& state;
    EventLogger& log;
    std::vector<size_t> recoveries_used;
    size_t interrupts = 0;
};

void log_status(const std::string& status, EventRun& run){
    run.log(event_log_line(run.checkpoint.id, run.number, run.event.name, status));
}


//  What the interrupts showing after an action ask for.
struct InterruptResult{
    bool fired = false;
    ResumePolicy resume = ResumePolicy::RECHECK_EXPECT;
    Outcome outcome;
};

//  The policy that asks for more: STOP over RETRY_EVENT over RECHECK_EXPECT.
ResumePolicy stronger(ResumePolicy x, ResumePolicy y){
    if (x == ResumePolicy::STOP || y == ResumePolicy::STOP){
        return ResumePolicy::STOP;
    }
    if (x == ResumePolicy::RETRY_EVENT || y == ResumePolicy::RETRY_EVENT){
        return ResumePolicy::RETRY_EVENT;
    }
    return ResumePolicy::RECHECK_EXPECT;
}

//  Clears every interrupt showing.
InterruptResult clear_interrupts(EventRun& run){
    InterruptResult result;
    for (Interrupt& interrupt : run.checkpoint.interrupts){
        if (!interrupt.detect(run.state)){
            continue;
        }
        run.interrupts++;
        log_status(std::format("interrupt: {}", interrupt.name), run);
        if (run.interrupts > run.checkpoint.max_interrupts_per_event){
            result.outcome = outcome_failed("too_many_interrupts", interrupt.name);
            return result;
        }
        if (interrupt.handle){
            Outcome handled = run_act(interrupt.handle, run.state);
            if (!handled.succeeded()){
                result.outcome = outcome_failed("interrupt_failed", std::format("{}: {}", interrupt.name, to_string(handled)));
                return result;
            }
        }
        apply_interrupt_pose(interrupt.pose, run.state.pose);
        result.resume = result.fired ? stronger(result.resume, interrupt.resume) : interrupt.resume;
        result.fired = true;
    }
    return result;
}


//  The next recovery for `failure`, using up one of its attempts.
Recovery* next_recovery(const std::string& failure, EventRun& run){
    std::vector<Recovery>& recoveries = run.event.recoveries;
    for (size_t c = 0; c < recoveries.size(); c++){
        Recovery& recovery = recoveries[c];
        if (!recovery.on.empty() && recovery.on != failure){
            continue;
        }
        if (recovery.kind != RecoveryKind::STOP && run.recoveries_used[c] >= recovery.attempts){
            continue;
        }
        run.recoveries_used[c]++;
        return &recovery;
    }
    return nullptr;
}

//  Answers `failure` with the event's recoveries. Returns nothing to try the
//  event again, or the failure that stops it. A recovery that fails itself
//  is answered in turn, so escalation goes on from there.
std::optional<Outcome> recover(Outcome failure, EventRun& run){
    while (true){
        if (failure.failure == GAME_NOT_RUNNING){
            return failure;
        }
        Recovery* recovery = next_recovery(failure.failure, run);
        if (recovery == nullptr){
            return failure;
        }
        std::string line = std::format("recover: {} ({})", recovery_name(recovery->kind), failure.failure);
        log_status(recovery->why.empty() ? line : std::format("{}: {}", line, recovery->why), run);
        if (recovery->kind == RecoveryKind::STOP){
            return failure;
        }
        if (!recovery->prepare){
            return std::nullopt;
        }
        Outcome prepared = run_act(recovery->prepare, run.state);
        if (prepared.succeeded()){
            return std::nullopt;
        }
        failure = std::move(prepared);
    }
}


enum class Next{
    DONE,
    RETRY,
    RECOVER,
    STOP,
};

//  After one attempt: what the interrupts showing and the outcome call for.
//  `outcome` becomes the failure to recover from or stop on.
Next after_attempt(EventRun& run, Outcome& outcome){
    InterruptResult interrupted = clear_interrupts(run);
    if (!interrupted.outcome.succeeded()){
        outcome = std::move(interrupted.outcome);
        return Next::STOP;
    }
    if (interrupted.fired){
        switch (interrupted.resume){
        case ResumePolicy::STOP:
            outcome = outcome_failed("interrupted", "stopped by an interrupt");
            return Next::STOP;
        case ResumePolicy::RETRY_EVENT:
            return Next::RETRY;
        case ResumePolicy::RECHECK_EXPECT:
            outcome = check_expects(run.event, run.state);
            return outcome.succeeded() ? Next::DONE : Next::RETRY;
        }
    }
    return outcome.succeeded() ? Next::DONE : Next::RECOVER;
}

//  Runs one event to success or to the failure that stops it.
Outcome run_event(EventRun& run){
    log_status("start", run);
    for (bool first = true; ; first = false){
        if (!first && already_done(run.event, run.state)){
            log_status("done: already showing", run);
            return outcome_ok();
        }
        Outcome outcome = attempt_event(run.event, run.state);
        Next next = after_attempt(run, outcome);
        if (next == Next::DONE){
            log_status("done", run);
            return outcome_ok();
        }
        if (next == Next::RETRY){
            continue;
        }
        if (next == Next::RECOVER){
            std::optional<Outcome> stop = recover(std::move(outcome), run);
            if (!stop){
                continue;
            }
            outcome = std::move(*stop);
        }
        log_status(std::format("failed: {}", to_string(outcome)), run);
        return outcome;
    }
}


//  What the exception being handled says, for a log line.
std::string current_exception_text(){
    try{
        throw;
    }catch (Exception& e){
        return std::format("{}: {}", e.name(), e.message());
    }catch (std::exception& e){
        return e.what();
    }catch (...){
        return "unknown exception";
    }
}

//  run_event(), logging the event and its checkpoint as failed before an
//  exception from the game's steps leaves the runner. The exception goes
//  on: the game decides what it means.
Outcome run_event_logging_throws(EventRun& run){
    try{
        return run_event(run);
    }catch (...){
        const std::string what = current_exception_text();
        log_status(std::format("failed: {}", what), run);
        run.log(std::format("[Checkpoint] {}: failed at {} {}: {}", run.checkpoint.id, run.number, run.event.name, what));
        throw;
    }
}

CheckpointReport stop_checkpoint(EventLogger& log, const std::string& id, Outcome failure, CheckpointReport& report){
    std::string where = report.failed_event == 0
        ? ""
        : std::format(" at {} {}", report.failed_event, report.failed_name);
    log(std::format("[Checkpoint] {}: failed{}: {}", id, where, to_string(failure)));
    report.failure = std::move(failure);
    return report;
}

Outcome finish_checkpoint(RunState& state, Checkpoint& checkpoint){
    std::optional<std::string> failing = first_failing(checkpoint.end, state);
    if (failing){
        return outcome_failed("end_failed", *failing);
    }
    if (!checkpoint.save){
        return outcome_ok();
    }
    Outcome saved = run_act(checkpoint.save, state);
    return saved.succeeded() ? saved : outcome_failed("save_failed", to_string(saved));
}

} // namespace


CheckpointReport run_checkpoint(RunState& state, Checkpoint& checkpoint, EventLogger& log){
    CheckpointReport report;
    const std::string& id = checkpoint.id;
    log(checkpoint_start_line(id, checkpoint.events.size()));
    std::optional<std::string> failing = first_failing(checkpoint.start, state);
    if (failing){
        return stop_checkpoint(log, id, outcome_failed("start_failed", *failing), report);
    }
    for (size_t c = 0; c < checkpoint.events.size(); c++){
        Event& event = checkpoint.events[c];
        EventRun run{checkpoint, c + 1, event, state, log, std::vector<size_t>(event.recoveries.size(), 0), 0};
        if (already_done(event, state)){
            log_status("skipped", run);
            report.events_skipped++;
            continue;
        }
        report.events_run++;
        Outcome outcome = run_event_logging_throws(run);
        if (!outcome.succeeded()){
            report.failed_event = c + 1;
            report.failed_name = event.name;
            return stop_checkpoint(log, id, std::move(outcome), report);
        }
    }
    Outcome finished = finish_checkpoint(state, checkpoint);
    if (!finished.succeeded()){
        return stop_checkpoint(log, id, std::move(finished), report);
    }
    report.completed = true;
    log(std::format("[Checkpoint] {}: done, {} run, {} skipped", id, report.events_run, report.events_skipped));
    return report;
}


namespace{

void lint_conditions(const std::string& where, const std::vector<Condition>& conditions, std::vector<std::string>& problems){
    for (const Condition& item : conditions){
        if (item.name.empty() || !item.check){
            problems.emplace_back(std::format("{}: a condition without a name or a check", where));
        }
    }
}

void lint_recoveries(const Event& event, std::vector<std::string>& problems){
    RecoveryKind last = RecoveryKind::WAIT;
    for (const Recovery& recovery : event.recoveries){
        if ((int)recovery.kind < (int)last){
            problems.emplace_back(std::format(
                "{}: recovery {} after {}", event.name, recovery_name(recovery.kind), recovery_name(last)
            ));
        }
        if (recovery.kind != RecoveryKind::STOP && recovery.attempts == 0){
            problems.emplace_back(std::format("{}: recovery {} with no attempts", event.name, recovery_name(recovery.kind)));
        }
        last = recovery.kind;
    }
}

void lint_event(const Event& event, std::set<std::string>& names, std::vector<std::string>& problems){
    if (event.name.empty()){
        problems.emplace_back("an event without a name");
    }else if (!names.insert(event.name).second){
        problems.emplace_back(std::format("{}: two events with this name", event.name));
    }
    if (!event.act){
        problems.emplace_back(std::format("{}: no action", event.name));
    }
    lint_conditions(std::format("{} skip_if", event.name), event.skip_if, problems);
    lint_conditions(std::format("{} require", event.name), event.require, problems);
    lint_conditions(std::format("{} expect", event.name), event.expect, problems);
    lint_recoveries(event, problems);
}

} // namespace


std::vector<std::string> lint_checkpoint(const Checkpoint& checkpoint){
    std::vector<std::string> problems;
    if (checkpoint.id.empty()){
        problems.emplace_back("a checkpoint without an id");
    }
    if (checkpoint.events.empty()){
        problems.emplace_back(std::format("{}: no events", checkpoint.id));
    }
    std::set<std::string> names;
    for (const Event& event : checkpoint.events){
        lint_event(event, names, problems);
    }
    for (const Interrupt& interrupt : checkpoint.interrupts){
        if (interrupt.name.empty() || !interrupt.detect){
            problems.emplace_back("an interrupt without a name or a detector");
        }
    }
    lint_conditions(std::format("{} start", checkpoint.id), checkpoint.start, problems);
    lint_conditions(std::format("{} end", checkpoint.id), checkpoint.end, problems);
    return problems;
}


namespace{

std::string names_of(const std::vector<Condition>& conditions){
    std::string text;
    for (const Condition& item : conditions){
        text += (text.empty() ? "" : ", ") + item.name;
    }
    return text;
}

void describe_line(const std::string& indent, const std::string& label, const std::string& value, std::string& text){
    if (!value.empty()){
        text += std::format("{}{}: {}\n", indent, label, value);
    }
}

void describe_event(size_t number, const Event& event, std::string& text){
    text += std::format("  event {} {}{}\n", number, event.name, event.why.empty() ? "" : std::format(": {}", event.why));
    describe_line("    ", "skip_if", names_of(event.skip_if), text);
    describe_line("    ", "require", names_of(event.require), text);
    describe_line("    ", "expect", names_of(event.expect), text);
    if (event.pose_learned){
        describe_line("    ", "pose_learned", to_string(*event.pose_learned), text);
    }
    for (const Recovery& recovery : event.recoveries){
        text += std::format(
            "    recovery: {} on {}{}{}\n",
            recovery_name(recovery.kind),
            recovery.on.empty() ? "any failure" : recovery.on,
            recovery.kind == RecoveryKind::STOP ? "" : std::format(" x{}", recovery.attempts),
            recovery.why.empty() ? "" : std::format(": {}", recovery.why)
        );
    }
}

} // namespace


std::string describe(const Checkpoint& checkpoint){
    std::string text = std::format("checkpoint {}: {} events\n", checkpoint.id, checkpoint.events.size());
    describe_line("  ", "start", names_of(checkpoint.start), text);
    std::string interrupts;
    for (const Interrupt& interrupt : checkpoint.interrupts){
        interrupts += (interrupts.empty() ? "" : ", ") + interrupt.name;
    }
    describe_line("  ", "interrupts", interrupts, text);
    for (size_t c = 0; c < checkpoint.events.size(); c++){
        describe_event(c + 1, checkpoint.events[c], text);
    }
    describe_line("  ", "end", names_of(checkpoint.end), text);
    if (checkpoint.save){
        text += "  saves\n";
    }
    return text;
}



} // namespace Pokemon
} // namespace PokemonAutomation
