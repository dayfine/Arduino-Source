#include <array>
#include <chrono>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/Exceptions/UnexpectedBattleException.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_SweatBubbleDetector.h"
#include "VideoGames/PokemonSV/Programs/Battles/PokemonSV_Battles.h"
#include "VideoGames/PokemonSV/Programs/Eggs/PokemonSV_EggRoutines.h"
#include "VideoGames/PokemonSV/Programs/Sandwiches/PokemonSV_SandwichRoutines.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "PokemonSV_IsolatedHuntTravel.h"
#include "PokemonSV_IsolatedHuntConsoleActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

class StageDeadline : public VisualInferenceCallback{
public:
    StageDeadline(Milliseconds timeout)
        : VisualInferenceCallback("IsolatedStageDeadline"), m_deadline(current_time() + timeout)
    {}
    virtual void make_overlays(VideoOverlaySet&) const override{}
    virtual bool process_frame(const ImageViewRGB32&, WallClock) override{
        return current_time() >= m_deadline;
    }
private:
    WallClock m_deadline;
};

const char* stop_name(IsolatedHuntStop stop){
    switch (stop){
    case IsolatedHuntStop::ROUND_LIMIT: return "round limit";
    case IsolatedHuntStop::START_FAILED: return "start flight failed";
    case IsolatedHuntStop::INVALID_PLAN: return "invalid plan";
    case IsolatedHuntStop::SETUP_FAILED: return "setup failed";
    case IsolatedHuntStop::TRAVEL_FAILED: return "travel failed";
    case IsolatedHuntStop::UNRESOLVED_REFUSAL: return "unresolved refusal";
    case IsolatedHuntStop::BATTLE_POLICY: return "battle policy stop";
    case IsolatedHuntStop::BATTLE_FAILED: return "battle failed";
    case IsolatedHuntStop::TIME_LIMIT: return "time limit";
    case IsolatedHuntStop::NO_PROGRESS: return "no progress";
    case IsolatedHuntStop::REFRESH_FAILED: return "refresh failed";
    case IsolatedHuntStop::POWER_EXPIRED: return "sandwich power expired";
    }
    return "unknown";
}

}

ConsoleIsolatedHuntActions::ConsoleIsolatedHuntActions(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context, IsolatedHuntStats& stats, Language language,
    IsolatedHuntSandwichOption& sandwich, const EncounterBotCommonOptions& encounter_options,
    const IsolatedHuntRequest& request
)
    : m_env(env), m_context(context), m_stats(stats), m_language(language), m_sandwich(sandwich)
    , m_options(make_isolated_encounter_options(encounter_options, request))
    , m_sensors(env.logger(), env.console, context)
    , m_battle_tracker(env.logger(), m_sensors)
    , m_hunt_seconds_before(stats.hunt_seconds.load())
{
    m_encounters = std::make_unique<LetsGoEncounterBotTracker>(env, env.console, stats, m_sensors.lets_go_kill);
}

uint64_t ConsoleIsolatedHuntActions::now_ms() const{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void ConsoleIsolatedHuntActions::phase(const std::string& stage){
    m_env.console.log("Isolated hunt state: " + stage);
    m_env.console.overlay().add_log(stage, COLOR_BLUE);
}

void ConsoleIsolatedHuntActions::update_elapsed(){
    if (m_hunt_started != 0){
        m_stats.hunt_seconds.store(m_hunt_seconds_before + (now_ms() - m_hunt_started) / 1000);
    }
    m_env.update_stats();
}

bool ConsoleIsolatedHuntActions::guard(const std::string& stage, Milliseconds timeout,
    const std::function<void(ProControllerContext&)>& command
){
    phase(stage);
    m_pending_battle = false;
    NormalBattleMenuWatcher battle(COLOR_RED);
    StageDeadline deadline(timeout);
    try{
        const int result = run_until<ProControllerContext>(
            m_env.console, m_context,
            [&](ProControllerContext& context){ command(context); }, {battle, deadline}
        );
        m_pending_battle = result == 0;
        if (result == 1){
            m_env.console.log("Isolated stage timed out: " + stage, COLOR_RED);
        }
        return result < 0;
    }catch (UnexpectedBattleException&){
        m_pending_battle = true;
    }catch (ProgramFinishedException&){
        m_pending_battle = true;
    }catch (OperationFailedException& error){
        m_env.console.log(stage + ": " + error.message(), COLOR_RED);
    }
    return false;
}

bool ConsoleIsolatedHuntActions::reset_start(const IsolatedHuntRequest&){
    bool arrived = false;
    const bool success = guard("re-fly Zero Gate starting anchor", 90000ms,
        [&](ProControllerContext& context){
            // The full-hunt start contract is outdoors at Zero Gate, including after a picnic retry.
            // Select only the overlapping anchor; never substitute a nearby unrelated fly point.
            open_map_from_overworld(m_env.program_info(), m_env.console, context);
            arrived = fly_to_overworld_from_map(m_env.program_info(), m_env.console, context, true);
        }
    );
    return success && arrived;
}

bool ConsoleIsolatedHuntActions::prepare_sandwich(const IsolatedHuntRequest& request){
    m_sandwich.configure_for(request.type);
    const bool success = guard("save and make " + request.pokemon + " sandwich", 240000ms,
        [&](ProControllerContext& context){
            save_game_from_overworld(m_env.program_info(), m_env.console, context);
            picnic_at_zero_gate(m_env.program_info(), m_env.console, context);
            pbf_move_left_joystick(context, {0, +1}, 560ms, 0ms);
            if (!enter_sandwich_recipe_list(m_env.program_info(), m_env.console, context)){
                throw UserSetupError(m_env.logger(), "Could not open sandwich recipes. Inspect ingredients and picnic setup.");
            }
            make_sandwich_option(m_env, m_env.console, context, m_sandwich);
            m_stats.sandwiches++;
            leave_picnic(m_env.program_info(), m_env.console, context);
        }
    );
    m_env.update_stats();
    return success;
}

bool ConsoleIsolatedHuntActions::travel_to_anchor(const IsolatedHuntRequest& request){
    bool arrived = false;
    const bool success = guard("travel to " + request.pokemon + " patch", 240000ms,
        [&](ProControllerContext& context){
            arrived = travel_to_isolated_hunt(m_env.program_info(), m_env.console, context,
                request.location, m_initial_travel);
            if (arrived){
                approach_isolated_patch(m_env.console, context, request.location);
            }
        }
    );
    if (success && arrived && m_initial_travel){
        m_hunt_started = now_ms();
        m_initial_travel = false;
    }
    return success && arrived;
}

void ConsoleIsolatedHuntActions::roam(ProControllerContext& context, IsolatedHuntLocation location){
    const auto duration = location == IsolatedHuntLocation::CHARGESTONE_CAVE ? 700ms : 1400ms;
    const std::array<std::array<double, 2>, 4> directions{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
    for (const auto& direction : directions){
        pbf_press_button(context, BUTTON_R, 160ms, 1500ms);
        pbf_move_left_joystick(context, {direction[0], direction[1]}, duration, 200ms);
    }
    pbf_wait(context, 2000ms);
    context.wait_for_all_requests();
}

IsolatedHuntRound ConsoleIsolatedHuntActions::clear_round(const IsolatedHuntRequest& request){
    phase("clearing " + request.pokemon);
    const auto before = m_stats.m_kills.load();
    NormalBattleMenuWatcher battle(COLOR_RED);
    SweatBubbleWatcher refusal(COLOR_GREEN);
    const int result = run_until<ProControllerContext>(
        m_env.console, m_context,
        [&](ProControllerContext& context){ roam(context, request.location); },
        {battle, refusal}
    );
    m_pending_battle = result == 0;
    m_stats.passes++;
    if (result == 1){
        m_stats.refusals++;
    }
    m_sensors.throw_if_no_sound();
    update_elapsed();
    const auto event = result == 0 ? IsolatedHuntEvent::BATTLE :
        result == 1 ? IsolatedHuntEvent::REFUSAL : IsolatedHuntEvent::CLEAR;
    return {.kills = m_stats.m_kills.load() - before, .event = event};
}

bool ConsoleIsolatedHuntActions::refresh_spawns(const IsolatedHuntRequest& request){
    bool arrived = false;
    const bool success = guard("refly and refresh " + request.pokemon, 90000ms,
        [&](ProControllerContext& context){
            arrived = travel_to_isolated_hunt(m_env.program_info(), m_env.console, context, request.location, false);
            if (arrived){
                approach_isolated_patch(m_env.console, context, request.location);
            }
        }
    );
    if (success && arrived){
        m_stats.refreshes++;
        update_elapsed();
    }
    return success && arrived;
}

bool ConsoleIsolatedHuntActions::acquire_refused_target(const IsolatedHuntRequest&){
    phase("refusal: attempt battle entry; do not reset spawns");
    NormalBattleMenuWatcher battle(COLOR_RED);
    const int result = run_until<ProControllerContext>(
        m_env.console, m_context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_ZR, 5000ms);
            pbf_wait(context, 10000ms);
        },
        {battle}
    );
    m_pending_battle = result == 0;
    return m_pending_battle;
}

IsolatedHuntBattleResult ConsoleIsolatedHuntActions::handle_battle(const IsolatedHuntRequest& request){
    phase("shared encounter handling: " + request.pokemon);
    m_pending_battle = false;
    m_sensors.throw_if_no_sound();
    m_stats.m_encounters++;
    m_encounters->encounter_rate_tracker().report_encounter();
    bool caught = false, should_save = false;
    try{
        process_battle(caught, should_save, m_env, *m_options, m_env.console, m_context,
            m_battle_tracker, m_encounters->encounter_frequencies(), m_stats.m_shinies, m_language);
    }catch (ProgramFinishedException&){
        update_elapsed();
        return IsolatedHuntBattleResult::STOP;
    }
    if (should_save){
        save_game_from_overworld(m_env.program_info(), m_env.console, m_context);
    }
    update_elapsed();
    return IsolatedHuntBattleResult::CONTINUE;
}

bool ConsoleIsolatedHuntActions::return_to_zero_gate(const IsolatedHuntRequest& request){
    bool arrived = false;
    const bool success = guard("return to Zero Gate", 180000ms,
        [&](ProControllerContext& context){
            arrived = return_from_isolated_hunt(m_env.program_info(), m_env.console, context, request.location);
            if (arrived){
                save_game_from_overworld(m_env.program_info(), m_env.console, context);
            }
        }
    );
    return success && arrived;
}

void ConsoleIsolatedHuntActions::report(const IsolatedHuntRequest& request, const IsolatedHuntResult& result){
    update_elapsed();
    const auto seconds = m_hunt_started == 0 ? 0 : (now_ms() - m_hunt_started) / 1000;
    const auto rate = seconds == 0 ? 0.0 : 60.0 * (double)result.kills / (double)seconds;
    m_env.console.log("Isolated result: target=" + request.pokemon + ", stop=" +
        stop_name(result.stop) + ", detected kills=" + std::to_string(result.kills) +
        ", encounters=" + std::to_string(result.encounters) + ", refusals=" + std::to_string(result.refusals) +
        ", refreshes=" + std::to_string(result.refreshes) + ", hunt seconds=" + std::to_string(seconds) +
        ", kills/min=" + std::to_string(rate));
}

}
}
}
