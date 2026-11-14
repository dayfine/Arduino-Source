/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_CheckpointAttempts.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon_StoryEngine.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The old executor interface over the step runner, so the engine builds
//  its events with build_story_checkpoint() (from_save, once_per_save).
//  Goes once StoryStepExecutor is deleted and that code moves here.
template <StoryPose PoseType>
class RunnerExecutor : public StoryStepExecutor{
public:
    RunnerExecutor(StoryStepRunner<PoseType>& runner, const StoryGame<PoseType>& game, StoryPausePoints& pause)
        : m_runner(runner)
        , m_game(game)
        , m_pause(pause)
    {}

    virtual void begin_attempt(const StoryCheckpoint& data) override{
        m_checkpoint = &data;
        if (m_game.begin_attempt){
            m_game.begin_attempt(data);
        }
    }
    virtual Outcome run(const StoryStep& step, RunState& state) override{
        m_pause.before_step(pause_label(step));
        return m_runner.act(step, state);
    }
    virtual bool holds(std::string_view condition, RunState& state) override{
        return m_runner.holds(condition, state);
    }

private:
    //  The step's place in its checkpoint (step names are unique, which the
    //  lint checks).
    std::string pause_label(const StoryStep& step) const{
        size_t index = 0;
        if (m_checkpoint != nullptr){
            while (index < m_checkpoint->steps.size() && m_checkpoint->steps[index].name != step.name){
                index++;
            }
        }
        return story_step_pause_label(m_checkpoint == nullptr ? "" : m_checkpoint->id, index, step.name);
    }

    StoryStepRunner<PoseType>& m_runner;
    const StoryGame<PoseType>& m_game;
    StoryPausePoints& m_pause;
    const StoryCheckpoint* m_checkpoint = nullptr;
};

//  `game` with its SAVE step "save" saving `current` only while `mode` is
//  PER_CHECKPOINT: a checkpoint under test or verification must not save
//  part-way.
template <StoryPose PoseType>
StoryGame<PoseType> with_gated_save_step(
    StoryGame<PoseType> game, const StorySaveMode& mode, const StoryCheckpoint* const& current
){
    StoryActionHandler<PoseType> gated = [&mode, &current, save = game.save](
        StoryBackend<PoseType>& backend, const StoryActionCall&, RunState&
    ){
        if (mode != StorySaveMode::PER_CHECKPOINT){
            backend.log(std::format("[Save] save step skipped: save mode {}", story_save_mode_name(mode)));
            return outcome_unchanged();
        }
        if (!save || current == nullptr){
            return outcome_failed("unsupported_step", "save needs StoryGame::save");
        }
        return save(backend, *current);
    };
    game.actions.push_back({.spec = {.kind = StoryStepKind::SAVE, .name = "save", .args = {}}, .handler = std::move(gated)});
    return game;
}

//  A failure every attempt would repeat: no reload for it.
bool repeats_on_reload(const Outcome& failure){
    return failure.failure == GAME_NOT_RUNNING || failure.failure == CONSOLE_ONLY || failure.failure == "lint_failed";
}

//  What stopped `report`'s run: its steps' failure, or its postconditions.
Outcome run_failure(const StoryEngineReport& report){
    if (!report.run.completed){
        return report.run.failure;
    }
    return outcome_failed("postconditions_failed");
}


//  The items ("bag.<ITEM>") and flags ("flag.<name>", non-zero is set) of
//  `observation`'s readings.
ObservedState observed_readings(const ScreenObservation& observation){
    constexpr std::string_view BAG = "bag.";
    constexpr std::string_view FLAG = "flag.";
    ObservedState observed;
    for (const auto& [name, value] : observation.readings){
        if (name.starts_with(BAG)){
            observed.items[name.substr(BAG.size())] = value < 0 ? 0 : (size_t)value;
        }else if (name.starts_with(FLAG)){
            observed.flags[name.substr(FLAG.size())] = value != 0;
        }
    }
    return observed;
}


//  What checking `checkpoint`'s postconditions needs read: "pose" for an
//  end position, "bag.<ITEM>" per item, "flag.<name>" per flag.
std::vector<std::string> postcondition_needs(const StoryCheckpoint& checkpoint){
    std::vector<std::string> needs;
    if (checkpoint_end_pose(checkpoint)){
        needs.emplace_back("pose");
    }
    for (const StoryItem& item : checkpoint.postconditions.items){
        needs.push_back(std::format("bag.{}", item.item));
    }
    for (const std::string& flag : checkpoint.postconditions.flags){
        needs.push_back(std::format("flag.{}", flag));
    }
    return needs;
}

//  Whether the postcondition check of a checkpoint that `needs` these runs
//  `reader` on a backend of `kind`.
template <StoryPose PoseType>
bool runs_after_checkpoint(const StoryGameReader<PoseType>& reader, const std::vector<std::string>& needs, StoryBackendKind kind){
    if (!reader.read || reader.use != StoryReaderUse::AFTER_CHECKPOINT){
        return false;
    }
    if (reader.console_only && kind != StoryBackendKind::CONSOLE){
        return false;
    }
    for (const std::string& need : needs){
        if (story_reader_supplies(reader.supplies, need)){
            return true;
        }
    }
    return false;
}


} // namespace


ObservedState observed_state(const Observation<GridPose>& observation){
    ObservedState observed = observed_readings(observation);
    if (observation.pose){
        observed.map = observation.pose->map;
        if (observation.pose->tiles.size() == 1){
            observed.tile = observation.pose->tiles[0];
        }
        observed.facing = observation.pose->facing;
    }
    return observed;
}

ObservedState observed_state(const Observation<ZonePose>& observation){
    ObservedState observed = observed_readings(observation);
    if (observation.pose){
        observed.map = observation.pose->zone;
    }
    return observed;
}


template <StoryPose PoseType>
StoryEngine<PoseType>::StoryEngine(const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend)
    : m_game(with_gated_save_step(game, m_save_mode, m_checkpoint))
    , m_backend(backend)
    , m_registry(m_game)
    , m_runner(m_registry, backend)
{
    m_registry.pause_points().set_log([&backend](std::string_view line){
        backend.log(line);
    });
}

template <StoryPose PoseType>
std::string StoryEngine<PoseType>::steps_hash(const StoryCheckpoint& checkpoint) const{
    return story_steps_hash(checkpoint);
}

template <StoryPose PoseType>
StoryEngineReport StoryEngine<PoseType>::run_checkpoint(const StoryCheckpoint& checkpoint, SavePoint point, RunState& state){
    return run_once(checkpoint, point, SaveGate::BY_PHASE, state);
}

template <StoryPose PoseType>
StoryEngineReport StoryEngine<PoseType>::run_once(
    const StoryCheckpoint& checkpoint, SavePoint point, SaveGate gate, RunState& state
){
    StoryEngineReport report;
    const std::vector<std::string> problems = m_registry.lint(checkpoint);
    if (!problems.empty()){
        for (const std::string& problem : problems){
            m_backend.log(std::format("[Lint] {}", problem));
        }
        report.run.failure = outcome_failed("lint_failed", problems.front());
        return report;
    }
    report.save = decide_story_save(checkpoint, steps_hash(checkpoint));
    if (gate == SaveGate::NEVER){
        report.save.mode = StorySaveMode::NONE;
        report.save.reason = std::format("[Save] {}: verify run: save mode none", checkpoint.id);
    }
    m_backend.log(report.save.reason);
    m_save_mode = report.save.mode;
    m_checkpoint = &checkpoint;
    report.run = run_steps(checkpoint, state);
    m_save_mode = StorySaveMode::NONE;
    m_checkpoint = nullptr;
    if (!report.run.completed){
        m_backend.log(std::format("[Save] {}: run failed: not saved", checkpoint.id));
        return report;
    }
    report.postconditions = check_postconditions(checkpoint);
    report.verdict = report.postconditions->count(PostconditionVerdict::FAILED) == 0
        ? CheckpointVerdict::PASSED
        : CheckpointVerdict::FAILED;
    save_if_allowed(checkpoint, point, report);
    return report;
}

template <StoryPose PoseType>
Outcome StoryEngine<PoseType>::reload(RunState& state){
    state = RunState{};
    if (!m_game.reload){
        return outcome_failed("reload_failed", "the game has no reload");
    }
    const Outcome reloaded = m_game.reload(m_backend);
    if (!reloaded.succeeded()){
        return outcome_failed("reload_failed", to_string(reloaded));
    }
    if (reloaded.pose){
        state.pose = *reloaded.pose;
    }
    return outcome_ok();
}

template <StoryPose PoseType>
StoryEngineReport StoryEngine<PoseType>::run_with_retries(
    const StoryCheckpoint& checkpoint, SavePoint point, size_t attempts, RunState& state, CheckpointRun& run
){
    StoryEngineReport report;
    const CheckpointAttempts loop{
        .max_attempts = attempts,
        .run = [&](size_t attempt){
            run.attempts = attempt;
            report = run_checkpoint(checkpoint, point, state);
            if (report.verdict == CheckpointVerdict::PASSED){
                return AttemptResult::PASSED;
            }
            return repeats_on_reload(report.run.failure) ? AttemptResult::FINAL : AttemptResult::FAILED;
        },
        .reset = [&](size_t next_attempt){
            m_backend.log(checkpoint_reset_line(checkpoint.id, next_attempt, attempts));
            const Outcome reloaded = reload(state);
            if (!reloaded.succeeded()){
                report.run.completed = false;
                report.run.failed_name.clear();
                report.run.failure = reloaded;
                return false;
            }
            return true;
        },
        .log = [this](const std::string& line){
            m_backend.log(line);
        },
    };
    run.passed = run_checkpoint_attempts(checkpoint.id, loop).passed;
    return report;
}

template <StoryPose PoseType>
SegmentReport StoryEngine<PoseType>::run_segment(
    std::string_view id, std::span<const StoryCheckpoint> checkpoints, size_t attempts_per_checkpoint,
    RunState& state
){
    SegmentReport report;
    m_backend.log(std::format("[Segment] {}: start, {} checkpoints", id, checkpoints.size()));
    //  Whether a reload returns to the save the next checkpoint starts from.
    bool start_saved = true;
    for (size_t c = 0; c < checkpoints.size(); c++){
        const StoryCheckpoint& checkpoint = checkpoints[c];
        const SavePoint point = c + 1 == checkpoints.size() ? SavePoint::RUN_END : SavePoint::CHECKPOINT_END;
        const size_t attempts = start_saved ? attempts_per_checkpoint : 1;
        if (!start_saved && attempts_per_checkpoint > 1){
            m_backend.log(std::format("[Segment] {}: {}: no save at its start, so no reload if it fails", id, checkpoint.id));
        }
        report.checkpoints_run++;
        report.runs.push_back(CheckpointRun{.id = checkpoint.id});
        const StoryEngineReport result = run_with_retries(checkpoint, point, attempts, state, report.runs.back());
        report.reloads += report.runs.back().attempts - 1;
        if (result.verdict != CheckpointVerdict::PASSED){
            report.failed_checkpoint = checkpoint.id;
            report.failed_event = result.run.failed_name;
            report.failure = run_failure(result);
            m_backend.log(std::format("[Segment] {}: failed at {}: {}", id, checkpoint.id, to_string(report.failure)));
            return report;
        }
        start_saved = result.saved;
    }
    report.completed = true;
    m_backend.log(std::format("[Segment] {}: done, {} checkpoints", id, report.checkpoints_run));
    return report;
}

template <StoryPose PoseType>
VerifyReport StoryEngine<PoseType>::verify_checkpoint(const StoryCheckpoint& checkpoint, const VerifyPolicy& policy, RunState& state){
    VerifyReport report;
    report.checkpoint = checkpoint.id;
    if (policy.runs == 0 || policy.passes_needed > policy.runs){
        report.failure = outcome_failed("bad_verify_policy", std::format("{} of {} runs", policy.passes_needed, policy.runs));
        m_backend.log(std::format("[Verify] {}: {}", checkpoint.id, to_string(report.failure)));
        return report;
    }
    m_backend.log(std::format("[Verify] {}: start, {} runs from one save, {} must pass", checkpoint.id, policy.runs, policy.passes_needed));
    for (size_t run = 1; run <= policy.runs; run++){
        if (run > 1){
            Outcome reloaded = reload(state);
            if (!reloaded.succeeded()){
                m_backend.log(std::format("[Verify] {}: no reload before run {}: {}", checkpoint.id, run, to_string(reloaded)));
                report.failure = std::move(reloaded);
                break;
            }
        }
        report.runs++;
        //  Only the last run may save, and only when every run before it passed.
        const bool may_save = run == policy.runs && report.passes + 1 == run;
        const StoryEngineReport result = run_once(
            checkpoint, SavePoint::RUN_END, may_save ? SaveGate::BY_PHASE : SaveGate::NEVER, state
        );
        const bool passed = result.verdict == CheckpointVerdict::PASSED;
        report.passes += passed ? 1 : 0;
        report.saved = result.saved;
        m_backend.log(std::format(
            "[Verify] {}: run {}/{} {}", checkpoint.id, run, policy.runs,
            passed ? std::string("passed") : std::format("failed: {}", to_string(run_failure(result)))
        ));
        if (repeats_on_reload(result.run.failure)){
            report.failure = result.run.failure;
            break;
        }
    }
    report.verified = report.failure.succeeded() && report.passes >= policy.passes_needed;
    m_backend.log(std::format(
        "[Verify] {}: {}/{} passed, {} needed: {}", checkpoint.id, report.passes, policy.runs, policy.passes_needed,
        report.verified ? "verified" : "not verified"
    ));
    return report;
}

template <StoryPose PoseType>
CheckpointReport StoryEngine<PoseType>::run_steps(const StoryCheckpoint& checkpoint, RunState& state){
    RunnerExecutor<PoseType> executor(m_runner, m_game, m_registry.pause_points());
    Checkpoint events = build_story_checkpoint(checkpoint, executor, &m_prepared);
    if (m_game.interrupts){
        events.interrupts = m_game.interrupts(m_backend);
    }
    EventLogger log = [this](const std::string& line){
        m_backend.log(line);
    };
    return Pokemon::run_checkpoint(state, events, log);
}

template <StoryPose PoseType>
void StoryEngine<PoseType>::save_if_allowed(const StoryCheckpoint& checkpoint, SavePoint point, StoryEngineReport& report){
    const bool passed = report.verdict == CheckpointVerdict::PASSED;
    if (!story_save_allowed(report.save.mode, report.verdict, point)){
        m_backend.log(std::format(
            "[Save] {}: not saved: save mode {}, {}", checkpoint.id,
            story_save_mode_name(report.save.mode), passed ? "passed" : "postconditions failed"
        ));
        return;
    }
    if (!m_game.save){
        m_backend.log(std::format("[Save] {}: not saved: the game has no save", checkpoint.id));
        return;
    }
    const Outcome saved = m_game.save(m_backend, checkpoint);
    report.saved = saved.succeeded();
    m_backend.log(std::format("[Save] {}: {}", checkpoint.id, report.saved ? "saved" : std::format("save failed: {}", to_string(saved))));
}

template <StoryPose PoseType>
PostconditionReport StoryEngine<PoseType>::check_postconditions(const StoryCheckpoint& checkpoint){
    const std::vector<std::string> needs = postcondition_needs(checkpoint);
    Observation<PoseType> observation = m_backend.observe();
    for (const StoryGameReader<PoseType>& reader : m_game.readers){
        if (runs_after_checkpoint(reader, needs, m_backend.kind())){
            reader.read(m_backend, observation);
        }
    }
    PostconditionReport report = Pokemon::check_postconditions(checkpoint, observed_state(observation));
    for (const std::string& line : postcondition_log_lines(report)){
        m_backend.log(line);
    }
    return report;
}


template class StoryEngine<GridPose>;
template class StoryEngine<ZonePose>;




} // namespace Pokemon
} // namespace PokemonAutomation
