/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <utility>
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
    RunnerExecutor(StoryStepRunner<PoseType>& runner, const StoryGame<PoseType>& game)
        : m_runner(runner)
        , m_game(game)
    {}

    virtual void begin_attempt(const StoryCheckpoint& data) override{
        if (m_game.begin_attempt){
            m_game.begin_attempt(data);
        }
    }
    virtual Outcome run(RunState& state, const StoryStep& step) override{
        return m_runner.act(state, step);
    }
    virtual bool holds(std::string_view condition, RunState& state) override{
        return m_runner.holds(condition, state);
    }

private:
    StoryStepRunner<PoseType>& m_runner;
    const StoryGame<PoseType>& m_game;
};

//  `game` with its SAVE step "save" saving only while `mode` is
//  PER_CHECKPOINT: a checkpoint under test or verification must not save
//  part-way.
template <StoryPose PoseType>
StoryGame<PoseType> with_gated_save_step(StoryGame<PoseType> game, const StorySaveMode& mode){
    StoryActionHandler<PoseType> gated = [&mode, save = game.save](StoryBackend<PoseType>& backend, RunState&, const StoryActionArgs&){
        if (mode != StorySaveMode::PER_CHECKPOINT){
            backend.log(std::format("[Save] save step skipped: save mode {}", story_save_mode_name(mode)));
            return outcome_unchanged();
        }
        return save ? save(backend) : outcome_failed("unsupported_step", "save needs StoryGame::save");
    };
    game.actions.push_back({.spec = {.kind = StoryStepKind::SAVE, .name = "save", .args = {}}, .handler = std::move(gated)});
    return game;
}


} // namespace


ObservedState observed_state(const Observation<GridPose>& observation){
    constexpr std::string_view BAG = "bag.";
    constexpr std::string_view FLAG = "flag.";
    ObservedState observed;
    if (observation.pose){
        observed.map = observation.pose->map;
        if (observation.pose->tiles.size() == 1){
            observed.tile = observation.pose->tiles[0];
        }
        observed.facing = observation.pose->facing;
    }
    for (const auto& [name, value] : observation.readings){
        if (name.starts_with(BAG)){
            observed.items[name.substr(BAG.size())] = value < 0 ? 0 : (size_t)value;
        }else if (name.starts_with(FLAG)){
            observed.flags[name.substr(FLAG.size())] = value != 0;
        }
    }
    return observed;
}


template <StoryPose PoseType>
StoryEngine<PoseType>::StoryEngine(const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend)
    : m_game(with_gated_save_step(game, m_save_mode))
    , m_backend(backend)
    , m_registry(m_game)
    , m_runner(m_registry, backend)
{}

template <StoryPose PoseType>
std::string StoryEngine<PoseType>::steps_hash(const StoryCheckpoint& checkpoint) const{
    return story_steps_hash(checkpoint, m_registry.specs());
}

template <StoryPose PoseType>
StoryEngineReport StoryEngine<PoseType>::run_checkpoint(const StoryCheckpoint& checkpoint, RunState& state, SavePoint point){
    StoryEngineReport report;
    const std::vector<std::string> problems = m_registry.lint(checkpoint);
    if (!problems.empty()){
        for (const std::string& problem : problems){
            m_backend.log("[Lint] " + problem);
        }
        report.run.failure = outcome_failed("lint_failed", problems.front());
        return report;
    }
    report.save = decide_story_save(checkpoint, steps_hash(checkpoint));
    m_backend.log(report.save.reason);
    m_save_mode = report.save.mode;
    report.run = run_steps(checkpoint, state);
    m_save_mode = StorySaveMode::NONE;
    if (!report.run.completed){
        m_backend.log(std::format("[Save] {}: run failed: not saved", checkpoint.id));
        return report;
    }
    report.postconditions = check_postconditions(checkpoint);
    report.verdict = report.postconditions->count(PostconditionVerdict::FAILED) == 0
        ? CheckpointVerdict::PASSED
        : CheckpointVerdict::FAILED;
    save_if_allowed(report, checkpoint, point);
    return report;
}

template <StoryPose PoseType>
CheckpointReport StoryEngine<PoseType>::run_steps(const StoryCheckpoint& checkpoint, RunState& state){
    RunnerExecutor<PoseType> executor(m_runner, m_game);
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
void StoryEngine<PoseType>::save_if_allowed(StoryEngineReport& report, const StoryCheckpoint& checkpoint, SavePoint point){
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
    const Outcome saved = m_game.save(m_backend);
    report.saved = saved.succeeded();
    m_backend.log(std::format("[Save] {}: {}", checkpoint.id, report.saved ? "saved" : "save failed: " + to_string(saved)));
}

template <StoryPose PoseType>
PostconditionReport StoryEngine<PoseType>::check_postconditions(const StoryCheckpoint& checkpoint){
    Observation<PoseType> observation = m_backend.observe();
    for (const auto& [name, reader] : m_game.readers){
        if (reader){
            reader(m_backend, observation);
        }
    }
    PostconditionReport report = Pokemon::check_postconditions(checkpoint, observed_state(observation));
    for (const std::string& line : postcondition_log_lines(report)){
        m_backend.log(line);
    }
    return report;
}


template class StoryEngine<GridPose>;




} // namespace Pokemon
} // namespace PokemonAutomation
