/*  Scripted Offline Executor for the Shared Story Engine (#648)
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_PokemonFRLG_StoryReplay_Executor_H
#define PokemonAutomation_PokemonFRLG_StoryReplay_Executor_H
#include <optional>
#include <set>
#include <string>
#include <utility>
#include "Common/Cpp/Json/JsonArray.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/Navigation/Grid/Planning/Pokemon_GatedTypes.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"
#include "PokemonFRLG_StoryReplay_IO.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryReplayInternal{
//  A supplied semantic command and its endpoint checked by native movement rules.
struct ReplayLeg{
    RouteStep step;
    JsonValue action;
};
//  A synchronous scripted adapter; production engine (#648) and navigation are reused.
//  It deliberately binds strict observation/stop policy, not live executor defaults.
class ReplayExecutor{
public:
    //  Inputs must outlive this synchronous replay; validation occurs before actions.
    ReplayExecutor(const ReplayData& data, const JsonValue& scenario);
    ReplayExecutor(const ReplayExecutor&) = delete;
    ReplayExecutor& operator=(const ReplayExecutor&) = delete;
    //  Runs one step with ordered authored completions, lost-pose handling,
    //  and FRLG's interrupts between steps on the scenario's scripted screens.
    std::optional<Pokemon::StoryFailure> run(const StoryStep& step, FrlgRunState& state);
    //  Build a #648 StoryCheckpoint bound to `state`.
    Pokemon::StoryCheckpoint build(FrlgRunState& state);
    //  Serialize independently computed effects/actions and raw engine/walker diagnostics.
    JsonValue report(
        const std::optional<Pokemon::CheckpointFailure>& failure,
        const FrlgRunState& state,
        const JsonArray& logs
    ) const;
private:
    FrlgStepAction replay_action(StoryActionId id);
    std::optional<Pokemon::StoryFailure> replay_route(const StoryStep& step, FrlgRunState& state);
    std::optional<Pokemon::StoryFailure> replay_step(const StoryStep& step, FrlgRunState& state);
    std::optional<Pokemon::StoryFailure> replay_scene(const StoryStep& step, FrlgRunState& state);
    const JsonObject* signal() const;
    const JsonObject& parameters(const StoryStep& step) const;
    //  `gates`: the gates before the command, and after it.
    ReplayLeg prepare_leg(const JsonValue& command, OverworldPose& current, Pokemon::GateState& gates) const;
    ReplayLeg prepare_gate_leg(const JsonValue& command, const OverworldPose& current, Pokemon::GateState& gates) const;
    //  The maps as `gates` make them: a closed gate is a wall.
    const std::vector<MapData>& maps_through(const Pokemon::GateState& gates) const;
    std::vector<RouteStep> prepare_route(const StoryStep& step);
    StepOutcome walk_leg(const RouteStep& step);
    StepOutcome gate_leg(const RouteStep& step);
    void default_plan(const StoryStep& step);
    void timed_plan(const StoryStep& step, const std::vector<RouteStep>& route);
    void lose_pose(std::string basis);
    void observe_interruption(const OverworldPose& predicted);
    //  The screen before the active step: the scenario's interrupt before
    //  it (between_steps), shown once, else nothing.
    Pokemon::ScreenLook look();
    //  Each kind recorded as an action; a warp loses the pose.
    StepInterruptActions interrupt_actions();
    const ReplayData* m_data;
    const JsonObject* m_scenario;
    OverworldPose m_pose;
    //  The gates open and switches on, and what the player may open, as
    //  the run state says at the start of each step.
    Pokemon::GateState m_gates;
    Pokemon::GateRules m_rules;
    mutable std::optional<std::pair<Pokemon::GateState, std::vector<MapData>>> m_resolved;
    bool m_known = true;
    std::string m_basis = "scripted-observation";
    std::string m_active;
    JsonArray m_actions;
    JsonArray m_completed;
    JsonArray m_candidates;
    JsonObject m_received;
    JsonArray m_route_logs;
    JsonObject m_native_plans;
    JsonObject m_timed_plans;
    std::vector<ReplayLeg> m_legs;
    size_t m_next_leg = 0;
    size_t m_moves = 0;
    bool m_interrupted = false;
    FrlgActionTable m_table;
    Pokemon::StoryInterrupts m_interrupts;
    std::set<std::string> m_shown;
};
} // namespace StoryReplayInternal
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
