/*  Scripted Offline Executor for the Existing Story Engine
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_PokemonFRLG_StoryReplay_Executor_H
#define PokemonAutomation_PokemonFRLG_StoryReplay_Executor_H
#include "Common/Cpp/Json/JsonArray.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecovery.h"
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
//  A synchronous scripted adapter; production engine/navigation are reused.
//  It deliberately binds strict observation/stop policy, not live executor defaults.
class ReplayExecutor : public Pokemon::StoryStepExecutor{
public:
    //  Inputs must outlive this synchronous replay; validation occurs before actions.
    ReplayExecutor(const ReplayData& data, const JsonValue& scenario);
    //  Existing engine dispatch with ordered authored completions and lost-pose handling.
    virtual Pokemon::Outcome run(Pokemon::RunState& state, const StoryStep& step) override;
    //  Build the existing checkpoint, keeping its from_save initializer explicit.
    Pokemon::Checkpoint build();
    //  Serialize independently computed effects/actions and raw engine/walker diagnostics.
    JsonValue report(const Pokemon::CheckpointReport& result, const Pokemon::RunState& state, const JsonArray& logs) const;
protected:
    virtual Pokemon::Outcome route(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome step(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome mash(Pokemon::RunState& state, const StoryStep& step) override;
private:
    const JsonObject* signal() const;
    const JsonObject& parameters(const StoryStep& step) const;
    ReplayLeg prepare_leg(const JsonValue& command, OverworldPose& current) const;
    std::vector<RouteStep> prepare_route(const StoryStep& step);
    StepOutcome walk_leg(const RouteStep& step);
    void default_plan(const StoryStep& step);
    void timed_plan(const StoryStep& step, const std::vector<RouteStep>& route);
    void lose_pose(std::string basis);
    void observe_interruption(const OverworldPose& predicted);
    const ReplayData* m_data;
    const JsonObject* m_scenario;
    OverworldPose m_pose;
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
};
} // namespace StoryReplayInternal
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
