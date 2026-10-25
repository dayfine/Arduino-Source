#ifndef PokemonAutomation_PokemonSV_IsolatedHuntConsoleActions_H
#define PokemonAutomation_PokemonSV_IsolatedHuntConsoleActions_H

#include <functional>
#include <utility>
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
    virtual bool reset_start(const IsolatedHuntRequest& request) override;
    virtual bool prepare_sandwich(const IsolatedHuntRequest& request) override;
    using Verifier = std::function<bool(ProControllerContext&, const IsolatedHuntRequest&, IsolatedHuntVerification)>;
    void set_verifier(Verifier verifier){ m_verifier = std::move(verifier); }
    virtual bool verify_meal_power(const IsolatedHuntRequest& request) override;
    virtual bool travel_to_anchor(const IsolatedHuntRequest& request) override;
    virtual IsolatedHuntRound clear_round(const IsolatedHuntRequest& request) override;
    virtual bool refresh_spawns(const IsolatedHuntRequest& request) override;
    virtual bool acquire_refused_target(const IsolatedHuntRequest& request) override;
    virtual IsolatedHuntBattleResult handle_battle(const IsolatedHuntRequest& request) override;
    virtual bool battle_pending() const override{ return m_pending_battle; }
    virtual uint64_t now_ms() const override;
    virtual bool refusal_pending() const override{ return m_pending_refusal; }
    virtual IsolatedMarkerState marker_state() override;
    virtual bool realign_marker() override;
    virtual bool reset_ascent(const IsolatedHuntRequest& request, const char* reason) override;
    virtual IsolatedHuntRound advance_ascent(const IsolatedHuntRequest& request, const IsolatedAscentLimits& limits) override;
    virtual void set_deadlines(uint64_t power, uint64_t hunt) override;
    bool return_to_zero_gate(const IsolatedHuntRequest& request);
    void report(const IsolatedHuntRequest& request, const IsolatedHuntResult& result);
private:
    bool guard(const std::string& stage, Milliseconds timeout,
        const std::function<void(ProControllerContext&)>& command);
    void roam(ProControllerContext& context, IsolatedHuntLocation location);
    void phase(const std::string& stage);
    void update_elapsed();
    bool clearing_active() const;

    Verifier m_verifier;
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
    uint16_t m_empty_rounds = 0;
    bool m_initial_travel = true;
    bool m_pending_battle = false;
    bool m_pending_refusal = false;
    bool m_marker_arrived = false;
    uint64_t m_deadline = UINT64_MAX;
    uint16_t m_ascent_leg = 0;
    uint16_t m_ascent_reset = 0;
    uint64_t m_hunt_started = 0;
    uint64_t m_hunt_seconds_before;
};

}
}
}
#endif
