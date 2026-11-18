/*  Scripted Offline Executor for the Existing Story Engine
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_PokemonFRLG_StoryReplay_Executor_H
#define PokemonAutomation_PokemonFRLG_StoryReplay_Executor_H
#include <optional>
#include <set>
#include <string>
#include <utility>
#include "Common/Cpp/Json/JsonArray.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/Navigation/Pokemon_GatedTypes.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
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
//  The replay lets a failed step's exception through: nothing reloads.
class ReplayStepGuard : public Pokemon::StepGuard{
public:
    virtual bool stops_on_failure(const StoryStep& step) const override;
};
//  A synchronous scripted adapter; production engine/navigation are reused.
//  It deliberately binds strict observation/stop policy, not live executor defaults.
class ReplayExecutor : public Pokemon::StoryStepExecutor{
public:
    //  Inputs must outlive this synchronous replay; validation occurs before actions.
    ReplayExecutor(const ReplayData& data, const JsonValue& scenario);
    //  Its table's actions point at it.
    ReplayExecutor(const ReplayExecutor&) = delete;
    ReplayExecutor& operator=(const ReplayExecutor&) = delete;
    //  FRLG's story action table (story_action_rows()) with ordered authored
    //  completions and lost-pose handling, and FRLG's interrupts between
    //  steps (step_interrupts()) on the scenario's scripted screens.
    virtual Pokemon::Outcome run(const StoryStep& step, Pokemon::RunState& state) override;
    //  Build the existing checkpoint, keeping its from_save initializer explicit.
    Pokemon::Checkpoint build();
    //  Serialize independently computed effects/actions and raw engine/walker diagnostics.
    JsonValue report(const Pokemon::CheckpointReport& result, const Pokemon::RunState& state, const JsonArray& logs) const;
private:
    //  The replay's action for `id`: a ROUTE, STEP or MASH; every other
    //  action is unsupported.
    Pokemon::StoryAction replay_action(StoryActionId id);
    //  A ROUTE or STEP reads the maps through the run's gates, and a
    //  route's open_gate and press_switch commands change them.
    Pokemon::Outcome replay_route(const StoryStep& step, Pokemon::RunState& state);
    Pokemon::Outcome replay_step(const StoryStep& step, const Pokemon::RunState& state);
    Pokemon::Outcome replay_scene(const StoryStep& step);
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
    ReplayStepGuard m_guard;
    Pokemon::StoryActionTable m_table;
    Pokemon::TableExecutor m_executor;
    Pokemon::StoryInterrupts m_interrupts;
    std::set<std::string> m_shown;
};
} // namespace StoryReplayInternal
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
