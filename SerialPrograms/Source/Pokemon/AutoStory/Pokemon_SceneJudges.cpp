/*  Scene Judges
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <format>
#include "CommonTools/ScreenEvents/ScreenFades.h"
#include "Pokemon_SceneJudges.h"

namespace PokemonAutomation{
namespace Pokemon{


WarpSeen note_warp(WarpSeen seen, const ScreenEvents& events, const std::string& banner_read){
    if (seen.fade == WarpFade::NONE){
        seen.fade = fade_in(events);
    }
    if (seen.banner.empty() && events.has(ScreenEvent::LOCATION_BANNER)){
        seen.banner = banner_read;
    }
    return seen;
}

std::string describe(const WarpSeen& seen){
    if (!seen.any()){
        return "nothing";
    }
    std::string out = seen.fade == WarpFade::NONE ? "" : warp_fade_name(seen.fade);
    if (!seen.banner.empty()){
        out += std::format("{}banner \"{}\"", out.empty() ? "" : ", ", seen.banner);
    }
    return out;
}


SceneMove TalkOver::look(const ScreenEvents& events, WallClock now){
    if (events.has(ScreenEvent::BATTLE_STARTED)){
        m_quiet_since.reset();
        m_calm_since.reset();
        return SceneMove::BATTLE;
    }
    m_fade_showing = fade_in(events);
    if (m_fade_showing != WarpFade::NONE){
        m_fade = m_fade_showing;
    }
    m_banner |= events.has(ScreenEvent::LOCATION_BANNER);
    const bool box = events.has(ScreenEvent::DIALOG_BOX);
    //  A fade is never calm: BDSP's overworld reads a black frame as no
    //  box, and black stays still, so a closing fade counted as quiet and
    //  ended the scene as it went down.
    const bool calm = !box && m_fade_showing == WarpFade::NONE &&
        events.has(ScreenEvent::PLAYER_CAN_MOVE) && warped();
    const bool quiet = calm && events.has(m_rule.still);
    if (!calm){
        m_calm_since.reset();
    }else if (!m_calm_since.has_value()){
        m_calm_since = now;
    }
    if (!quiet){
        m_quiet_since.reset();
    }else if (!m_quiet_since.has_value()){
        m_quiet_since = now;
    }
    const bool over =
        (quiet && now - *m_quiet_since >= m_rule.quiet) ||
        (calm && now - *m_calm_since >= m_rule.calm);
    if (over && m_rule.end_on_quiet){
        return SceneMove::DONE;
    }
    return box ? SceneMove::NEXT : SceneMove::B;
}

bool TalkOver::warped() const{
    return !m_rule.warp || m_fade != WarpFade::NONE || m_banner;
}

std::optional<Milliseconds> TalkOver::left(WallClock now) const{
    if (!m_rule.end_on_quiet){
        return std::nullopt;
    }
    std::optional<Milliseconds> out;
    const auto take = [&](const std::optional<WallClock>& since, Milliseconds need){
        if (!since.has_value()){
            return;
        }
        const Milliseconds held = std::chrono::duration_cast<Milliseconds>(now - *since);
        const Milliseconds rest = std::max(need - held, Milliseconds(0));
        if (!out.has_value() || rest < *out){
            out = rest;
        }
    };
    take(m_quiet_since, m_rule.quiet);
    take(m_calm_since, m_rule.calm);
    return out;
}

std::string TalkOver::describe() const{
    if (m_fade == WarpFade::NONE && !m_banner){
        return "no fade or banner";
    }
    std::string out = m_fade == WarpFade::NONE ? "" : warp_fade_name(m_fade);
    if (m_fade != WarpFade::NONE){
        out += m_fade_showing == WarpFade::NONE ? " back up" : " still down";
    }
    if (m_banner){
        out += out.empty() ? "banner read" : ", banner read";
    }
    return out;
}



} // namespace Pokemon
} // namespace PokemonAutomation
