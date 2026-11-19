/*  PokemonLZA AutoStory - Signal probe
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStorySignalProbe.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

struct ProbedSignal{
    std::string_view name;
    bool (*shows)(const ScreenObservation& look);
};

bool has(const ScreenObservation& look, std::string_view reading){
    return look.readings.contains(reading);
}

constexpr std::array<ProbedSignal, PROBED_SIGNALS> SIGNALS{{
    {"party_bar", [](const ScreenObservation& look){ return look.events.has(ScreenEvent::PLAYER_CAN_MOVE); }},
    {"hint", [](const ScreenObservation& look){ return has(look, CONTROLS_HINT_READING); }},
    {"prompt", [](const ScreenObservation& look){ return look.events.has(ScreenEvent::INTERACT_PROMPT); }},
    {"arrow", [](const ScreenObservation& look){ return has(look, ADVANCE_ARROW_READING); }},
    {SHADOW_DIALOG_BOX_READING, [](const ScreenObservation& look){ return has(look, SHADOW_DIALOG_BOX_READING); }},
    {"autosave", [](const ScreenObservation& look){ return has(look, AUTOSAVE_READING); }},
    {"banner", [](const ScreenObservation& look){ return has(look, MISSION_BANNER_READING); }},
    {"choice", [](const ScreenObservation& look){ return look.events.has(ScreenEvent::CHOICE_BOX); }},
    {"fade", [](const ScreenObservation& look){ return fade_shows(look); }},
}};

std::string since(const std::optional<WallClock>& when, WallClock start){
    if (!when){
        return "-";
    }
    return std::to_string(std::chrono::duration_cast<Milliseconds>(*when - start).count());
}

} // namespace


void SignalProbe::look(const ScreenObservation& look){
    m_start = m_start.value_or(look.when);
    m_last = look.when;
    for (size_t index = 0; index < PROBED_SIGNALS; index++){
        if (!m_first[index] && SIGNALS[index].shows(look)){
            m_first[index] = look.when;
        }
    }
    if (text_shows(look) || fade_shows(look) || look.events.has(ScreenEvent::CHOICE_BOX)){
        m_last_busy = look.when;
    }
    m_dialog_box_last = has(look, SHADOW_DIALOG_BOX_READING);
}

void SignalProbe::restart(){
    *this = SignalProbe{};
}

std::string SignalProbe::line(std::string_view where, std::string_view decision, std::string_view by) const{
    const WallClock start = m_start.value_or(m_last);
    std::string first;
    for (size_t index = 0; index < PROBED_SIGNALS; index++){
        first += std::format(" {}@{}", SIGNALS[index].name, since(m_first[index], start));
    }
    return std::format(
        "[Signals] {} {} by={} after={} first:{} last_busy@{} shadow.dialog_box_at_decision={}",
        where, decision, by, since(m_last, start), first, since(m_last_busy, start), m_dialog_box_last ? 1 : 0
    );
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
