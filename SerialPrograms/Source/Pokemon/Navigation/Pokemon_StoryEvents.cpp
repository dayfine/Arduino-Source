/*  Story Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon_StoryEvents.h"

namespace PokemonAutomation{
namespace Pokemon{


const char* certainty_name(PoseCertainty certainty){
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
            text += " " + where;
        }
    }
    return text + " (" + certainty_name(pose.certainty) + ")";
}


bool Outcome::succeeded() const{
    return failure.empty();
}
Outcome outcome_ok(){
    return Outcome{};
}
Outcome outcome_failed(std::string failure, std::string detail){
    return Outcome{std::move(failure), std::move(detail)};
}

Condition condition(std::string name, Check check){
    return Condition{std::move(name), std::move(check)};
}

Condition pose_known(std::string map, PoseCertainty need){
    std::string name = std::string("pose ") + certainty_name(need) + (map.empty() ? "" : " on " + map);
    return condition(std::move(name), [map = std::move(map), need](RunState& state){
        return (map.empty() || state.pose.map == map) && certainty_covers(state.pose.certainty, need);
    });
}

const char* recovery_name(RecoveryKind kind){
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


std::string event_log_line(const std::string& checkpoint, size_t number, const std::string& event, const std::string& status){
    return std::format("[Event] {}/{} {}: {}", checkpoint, number, event, status);
}


namespace{

std::string describe(const Outcome& outcome){
    return outcome.detail.empty() ? outcome.failure : outcome.failure + ": " + outcome.detail;
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
    Outcome outcome = event.act(state);
    if (!outcome.succeeded()){
        return outcome;
    }
    return check_expects(event, state);
}

void apply_interrupt_pose(Pose& pose, InterruptPose effect){
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

void log_status(EventRun& run, const std::string& status){
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
        log_status(run, "interrupt: " + interrupt.name);
        if (run.interrupts > run.checkpoint.max_interrupts_per_event){
            result.outcome = outcome_failed("too_many_interrupts", interrupt.name);
            return result;
        }
        if (interrupt.handle){
            Outcome handled = interrupt.handle(run.state);
            if (!handled.succeeded()){
                result.outcome = outcome_failed("interrupt_failed", interrupt.name + ": " + describe(handled));
                return result;
            }
        }
        apply_interrupt_pose(run.state.pose, interrupt.pose);
        result.resume = result.fired ? stronger(result.resume, interrupt.resume) : interrupt.resume;
        result.fired = true;
    }
    return result;
}


//  The next recovery for `failure`, using up one of its attempts.
Recovery* next_recovery(EventRun& run, const std::string& failure){
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
std::optional<Outcome> recover(EventRun& run, Outcome failure){
    while (true){
        Recovery* recovery = next_recovery(run, failure.failure);
        if (recovery == nullptr){
            return failure;
        }
        std::string line = std::string("recover: ") + recovery_name(recovery->kind) + " (" + failure.failure + ")";
        log_status(run, recovery->why.empty() ? line : line + ": " + recovery->why);
        if (recovery->kind == RecoveryKind::STOP){
            return failure;
        }
        if (!recovery->prepare){
            return std::nullopt;
        }
        Outcome prepared = recovery->prepare(run.state);
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
    log_status(run, "start");
    for (bool first = true; ; first = false){
        if (!first && already_done(run.event, run.state)){
            log_status(run, "done: already showing");
            return outcome_ok();
        }
        Outcome outcome = attempt_event(run.event, run.state);
        Next next = after_attempt(run, outcome);
        if (next == Next::DONE){
            log_status(run, "done");
            return outcome_ok();
        }
        if (next == Next::RETRY){
            continue;
        }
        if (next == Next::RECOVER){
            std::optional<Outcome> stop = recover(run, std::move(outcome));
            if (!stop){
                continue;
            }
            outcome = std::move(*stop);
        }
        log_status(run, "failed: " + describe(outcome));
        return outcome;
    }
}


CheckpointReport stop_checkpoint(CheckpointReport& report, EventLogger& log, const std::string& id, Outcome failure){
    std::string where = report.failed_event == 0
        ? ""
        : std::format(" at {} {}", report.failed_event, report.failed_name);
    log(std::format("[Checkpoint] {}: failed{}: {}", id, where, describe(failure)));
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
    Outcome saved = checkpoint.save(state);
    return saved.succeeded() ? saved : outcome_failed("save_failed", describe(saved));
}

}


CheckpointReport run_checkpoint(RunState& state, Checkpoint& checkpoint, EventLogger& log){
    CheckpointReport report;
    const std::string& id = checkpoint.id;
    log(std::format("[Checkpoint] {}: start, {} events", id, checkpoint.events.size()));
    std::optional<std::string> failing = first_failing(checkpoint.start, state);
    if (failing){
        return stop_checkpoint(report, log, id, outcome_failed("start_failed", *failing));
    }
    for (size_t c = 0; c < checkpoint.events.size(); c++){
        Event& event = checkpoint.events[c];
        EventRun run{checkpoint, c + 1, event, state, log, std::vector<size_t>(event.recoveries.size(), 0), 0};
        if (already_done(event, state)){
            log_status(run, "skipped");
            report.events_skipped++;
            continue;
        }
        report.events_run++;
        Outcome outcome = run_event(run);
        if (!outcome.succeeded()){
            report.failed_event = c + 1;
            report.failed_name = event.name;
            return stop_checkpoint(report, log, id, std::move(outcome));
        }
    }
    Outcome finished = finish_checkpoint(state, checkpoint);
    if (!finished.succeeded()){
        return stop_checkpoint(report, log, id, std::move(finished));
    }
    report.completed = true;
    log(std::format("[Checkpoint] {}: done, {} run, {} skipped", id, report.events_run, report.events_skipped));
    return report;
}


namespace{

void lint_conditions(std::vector<std::string>& problems, const std::string& where, const std::vector<Condition>& conditions){
    for (const Condition& item : conditions){
        if (item.name.empty() || !item.check){
            problems.emplace_back(where + ": a condition without a name or a check");
        }
    }
}

void lint_recoveries(std::vector<std::string>& problems, const Event& event){
    RecoveryKind last = RecoveryKind::WAIT;
    for (const Recovery& recovery : event.recoveries){
        if ((int)recovery.kind < (int)last){
            problems.emplace_back(event.name + ": recovery " + recovery_name(recovery.kind) + " after " + recovery_name(last));
        }
        if (recovery.kind != RecoveryKind::STOP && recovery.attempts == 0){
            problems.emplace_back(event.name + ": recovery " + recovery_name(recovery.kind) + " with no attempts");
        }
        last = recovery.kind;
    }
}

void lint_event(std::vector<std::string>& problems, std::set<std::string>& names, const Event& event){
    if (event.name.empty()){
        problems.emplace_back("an event without a name");
    }else if (!names.insert(event.name).second){
        problems.emplace_back(event.name + ": two events with this name");
    }
    if (!event.act){
        problems.emplace_back(event.name + ": no action");
    }
    lint_conditions(problems, event.name + " skip_if", event.skip_if);
    lint_conditions(problems, event.name + " require", event.require);
    lint_conditions(problems, event.name + " expect", event.expect);
    lint_recoveries(problems, event);
}

}


std::vector<std::string> lint_checkpoint(const Checkpoint& checkpoint){
    std::vector<std::string> problems;
    if (checkpoint.id.empty()){
        problems.emplace_back("a checkpoint without an id");
    }
    if (checkpoint.events.empty()){
        problems.emplace_back(checkpoint.id + ": no events");
    }
    std::set<std::string> names;
    for (const Event& event : checkpoint.events){
        lint_event(problems, names, event);
    }
    for (const Interrupt& interrupt : checkpoint.interrupts){
        if (interrupt.name.empty() || !interrupt.detect){
            problems.emplace_back("an interrupt without a name or a detector");
        }
    }
    lint_conditions(problems, checkpoint.id + " start", checkpoint.start);
    lint_conditions(problems, checkpoint.id + " end", checkpoint.end);
    return problems;
}



}
}
