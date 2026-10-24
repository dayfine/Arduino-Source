#ifndef PokemonAutomation_PokemonSV_IsolatedHuntConsoleActions_H
#define PokemonAutomation_PokemonSV_IsolatedHuntConsoleActions_H

#include <functional>
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldSensors.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_IsolatedHuntSandwichOption.h"
#include "PokemonSV_IsolatedHuntLoop.h"
#include "PokemonSV_IsolatedHuntStats.h"
#include "PokemonSV_IsolatedHuntEncounter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

class ConsoleIsolatedHuntActions : public IsolatedHuntActions{
public:
    ConsoleIsolatedHuntActions(SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        IsolatedHuntStats& stats, Language language, IsolatedHuntSandwichOption& sandwich,
        const EncounterBotCommonOptions& encounter_options, const IsolatedHuntRequest& request);
    virtual bool prepare_sandwich(const IsolatedHuntRequest& request) override;
    virtual bool travel_to_anchor(const IsolatedHuntRequest& request) override;
    virtual IsolatedHuntRound clear_round(const IsolatedHuntRequest& request) override;
    virtual bool refresh_spawns(const IsolatedHuntRequest& request) override;
    virtual bool acquire_refused_target(const IsolatedHuntRequest& request) override;
    virtual IsolatedHuntBattleResult handle_battle(const IsolatedHuntRequest& request) override;
    virtual bool battle_pending() const override{ return m_pending_battle; }
    virtual uint64_t now_ms() const override;
    bool return_to_zero_gate(const IsolatedHuntRequest& request);
    void report(const IsolatedHuntRequest& request, const IsolatedHuntResult& result);
private:
    bool guard(const std::string& stage, Milliseconds timeout,
        const std::function<void(ProControllerContext&)>& command);
    void roam(ProControllerContext& context, IsolatedHuntLocation location);
    void phase(const std::string& stage);
    void update_elapsed();

    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
    IsolatedHuntStats& m_stats;
    Language m_language;
    IsolatedHuntSandwichOption& m_sandwich;
    std::unique_ptr<EncounterBotCommonOptions> m_options;
    // Audio inference must stop before destroying the tracker captured in its callback.
    std::unique_ptr<LetsGoEncounterBotTracker> m_encounters;
    OverworldSensors m_sensors;
    OverworldBattleTracker m_battle_tracker;
    bool m_initial_travel = true;
    bool m_pending_battle = false;
    uint64_t m_hunt_started = 0;
    uint64_t m_hunt_seconds_before;
};

}
}
}
#endif
