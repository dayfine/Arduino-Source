/*  AutoStory Row Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's story on the simulation, running through #648 StoryEngine
 *  (make_sim_engine_checkpoint()) and OverworldStepSim.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRowSim_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRowSim_H

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/Navigation/Grid/Planning/Pokemon_GatedTypes.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRowSimStubs.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


struct StorySimKnobs{
    std::function<bool(const std::string& row, const std::string& step)> dialog_before = nullptr;
    bool clear_interrupts = true;
    const std::vector<MapData>* maps = nullptr;
    Pokemon::StoryCheckLevel checks = Pokemon::StoryCheckLevel::TESTING;
};

//  Runs FRLG's story on the story maps without a walker, executing through
//  #648 StoryEngine::run_checkpoints() via make_checkpoint().
class StoryMapsSim{
public:
    explicit StoryMapsSim(StorySimKnobs knobs = {});
    StoryMapsSim(const StoryMapsSim&) = delete;
    void operator=(const StoryMapsSim&) = delete;

    virtual ~StoryMapsSim() = default;
    virtual void begin_attempt(const StoryCheckpoint& data);
    virtual std::optional<Pokemon::StoryFailure> run_step(const StoryStep& step, FrlgRunState& state);

    //  Builds a #648 Pokemon::StoryCheckpoint driven by this StoryMapsSim.
    Pokemon::StoryCheckpoint make_checkpoint(
        const StoryCheckpoint& data, FrlgRunState& state,
        SavePoseReader start_pose_reader = {}, StoryLog start_log = {}
    );

    const FrlgActionTable& actions() const{
        return m_table;
    }
    const SimStubs& stubs() const{
        return m_stubs;
    }
    const std::map<std::string, int>& bag() const{
        return m_bag;
    }
    const std::vector<std::string>& dialog_notes() const{
        return m_dialog_notes;
    }

private:
    FrlgStepAction action(StoryActionId id);
    const std::vector<MapData>& maps(const FrlgRunState& state) const;
    std::optional<Pokemon::StoryFailure> walk_to(FrlgRunState& state, const StoryStep& step) const;
    std::optional<Pokemon::StoryFailure> legs(FrlgRunState& state, const StoryStep& step) const;
    std::optional<Pokemon::StoryFailure> step_ahead(FrlgRunState& state, const StoryStep& step) const;
    std::optional<Pokemon::StoryFailure> shop(const StoryStep& step);
    Pokemon::ScreenLook look();
    StepInterruptActions sim_interrupt_actions();

    SimStubs m_stubs;
    std::map<std::string, int> m_bag;
    FrlgActionTable m_table;
    StorySimKnobs m_knobs;
    Pokemon::StoryInterrupts m_interrupts;
    std::string m_row;
    bool m_dialog_up = false;
    bool m_dialog_rose = false;
    std::vector<std::string> m_dialog_notes;
};


std::optional<std::vector<RouteStep>> walked_route(
    const StoryPose& at, const StoryStep& step,
    const std::vector<MapData>& maps = story_maps(),
    const Pokemon::GateState& gates = Pokemon::GateState(),
    const Pokemon::GateRules& rules = Pokemon::GateRules()
);

class OverworldStepSim{
public:
    OverworldStepSim(OverworldSim& sim, std::string row, StoryPose at);
    OverworldStepSim(const OverworldStepSim&) = delete;
    void operator=(const OverworldStepSim&) = delete;

    std::optional<Pokemon::StoryFailure> run(const StoryStep& step, FrlgRunState& state);

    const StoryPose& at() const{
        return m_at;
    }
    const FrlgActionTable& actions() const{
        return m_table;
    }
    const SimStubs& stubs() const{
        return m_stubs;
    }

private:
    FrlgStepAction action(StoryActionId id);
    FrlgStepAction step_action(StoryActionId id);
    std::string walk(const StoryStep& step);
    std::string step_ahead(const StoryStep& step);
    std::string step_onto(const StoryStep& step);
    std::string step_onto_warp(const RouteStep& warp);
    std::string cut(const StoryStep& step);
    std::string push_boulder(const StoryStep& step);
    std::string battle(const StoryStep& step);
    FrlgStepAction hideout_or_stub();
    std::string hideout(const StoryStep& step);
    std::string ended_on(const StoryStep& step);

    OverworldSim& m_sim;
    StoryPose m_at;
    std::set<std::string> m_flags;
    std::set<std::string> m_beaten;
    SimStubs m_stubs;
    FrlgActionTable m_table;
};

std::string row_step_on_sim(
    OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id,
    const std::set<std::string>& run_flags = std::set<std::string>()
);
std::string row_step_carrying_flags(
    OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id,
    std::set<std::string>& run_flags
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
