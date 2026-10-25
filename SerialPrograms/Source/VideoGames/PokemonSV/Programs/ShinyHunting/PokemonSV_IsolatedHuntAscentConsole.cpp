/*  Isolated Uphill Console Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#include <algorithm>
#include "CommonTools/Async/InferenceRoutines.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_SweatBubbleDetector.h"
#include "VideoGames/PokemonSV/Inference/Map/PokemonSV_DestinationMarkerDetector.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MarkerNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "PokemonSV_IsolatedHuntConsoleActions.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
bool ConsoleIsolatedHuntActions::reset_ascent(const IsolatedHuntRequest& request, const char* reason){
    m_env.console.log("Isolated ascent: stage=flight-reset, reason=" + std::string(reason) +
        ", leg=" + std::to_string(m_ascent_leg) + ", reset=" + std::to_string(m_ascent_reset));
    return travel_to_anchor(request);
}
bool ConsoleIsolatedHuntActions::clearing_active() const{
    const auto last = m_sensors.lets_go_kill.last_kill();
    return last != WallClock::min() && current_time() - last < Milliseconds(2000);
}
void ConsoleIsolatedHuntActions::set_deadlines(uint64_t power, uint64_t hunt){
    m_deadline = std::min(power, hunt);
}
IsolatedMarkerState ConsoleIsolatedHuntActions::marker_state(){
    if (m_marker_arrived){ return IsolatedMarkerState::ARRIVED; }
    const auto marker = read_destination_marker(m_env.console.video().snapshot());
    m_env.console.log("Isolated ascent: stage=marker, state=" + std::to_string((int)marker) +
        ", leg=" + std::to_string(m_ascent_leg) + ", reset=" + std::to_string(m_ascent_reset));
    switch (marker){
    case DestinationMarkerState::PRESENT: return IsolatedMarkerState::PRESENT;
    case DestinationMarkerState::ARRIVED: return IsolatedMarkerState::ARRIVED;
    case DestinationMarkerState::MISSING: return IsolatedMarkerState::MISSING;
    }
    return IsolatedMarkerState::MISSING;
}
bool ConsoleIsolatedHuntActions::realign_marker(){
    bool present = false;
    const bool success = guard("ascent reanchor: old-marker heading correction", Milliseconds(20000),
        [&](ProControllerContext& context){
            realign_to_existing_destination(m_env.program_info(), m_env.console, context);
            present = marker_state() == IsolatedMarkerState::PRESENT &&
                destination_marker_ahead(m_env.console.video().snapshot());
            m_env.console.log("Isolated ascent: stage=reanchor, marker ahead=" + std::to_string(present));
        });
    return success && present;
}
IsolatedHuntRound ConsoleIsolatedHuntActions::advance_ascent(const IsolatedHuntRequest& request,
    const IsolatedAscentLimits& limits
){
    const auto before = m_stats.m_kills.load();
    const auto started = now_ms();
    NormalBattleMenuWatcher battle(COLOR_RED);
    SweatBubbleWatcher refusal(COLOR_GREEN);
    DestinationMarkerWatcher arrival(COLOR_CYAN, {0.717, 0.165, 0.03, 0.061});
    // The deadline is also checked by policy between stages. Shorten this movement before queuing it.
    const auto remaining = m_deadline > started ? m_deadline - started : 0;
    if (remaining < (uint64_t)limits.segment_ms + limits.clearing_wait_ms + 4000){ return {.budget_exhausted = true}; }
    const int ret = run_until<ProControllerContext>(m_env.console, m_context,
        [&](ProControllerContext& context){
            walk_forward_while_clear_front_path(m_env.program_info(), m_env.console, context,
                Milliseconds(limits.segment_ms), -1, Milliseconds(limits.segment_ms + 1), Milliseconds(limits.clearing_wait_ms));
            // Observe recent defeats before another map operation or movement. Never wait without a bound.
            const auto quiet_deadline = std::min(m_deadline, now_ms() + 10000);
            do{
                context.wait_for(Milliseconds(250));
            }while (now_ms() < quiet_deadline && clearing_active());

        }, {battle, refusal, arrival});
    m_pending_battle = ret == 0;
    m_pending_refusal = ret == 1;
    m_marker_arrived = ret == 2;
    m_stats.passes++;
    if (m_pending_refusal){ m_stats.refusals++; }
    m_sensors.throw_if_no_sound();
    update_elapsed();
    const auto kills = m_stats.m_kills.load() - before;
    m_empty_rounds = kills == 0 ? m_empty_rounds + 1 : 0;
    m_env.console.log("Isolated ascent: stage=advance, target=" + request.pokemon + ", leg=" +
        std::to_string(++m_ascent_leg) + ", movement cap ms=" + std::to_string(limits.segment_ms) +
        ", elapsed ms=" + std::to_string(now_ms() - started) + ", detected defeats=" + std::to_string(kills) +
        ", no-progress=" + std::to_string(m_empty_rounds) + ", event=" + std::to_string(ret));
    return {.kills = kills, .event = m_pending_battle ? IsolatedHuntEvent::BATTLE :
        m_pending_refusal ? IsolatedHuntEvent::REFUSAL : IsolatedHuntEvent::CLEAR,
        .clearing_busy = ret < 0 && clearing_active()};
}
}
}
}
