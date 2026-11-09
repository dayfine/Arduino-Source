/*  Scene Judges
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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
        out += (out.empty() ? "banner \"" : ", banner \"") + seen.banner + "\"";
    }
    return out;
}


SceneMove TalkOver::look(const ScreenEvents& look, WallClock now){
    if (look.has(ScreenEvent::BATTLE_STARTED)){
        m_quiet_since = WallClock::max();
        m_calm_since = WallClock::max();
        return SceneMove::BATTLE;
    }
    if (fade_in(look) != WarpFade::NONE){
        m_fade = fade_in(look);
    }
    m_banner |= look.has(ScreenEvent::LOCATION_BANNER);
    const bool box = look.has(ScreenEvent::DIALOG_BOX);
    const bool calm = !box && look.has(ScreenEvent::PLAYER_CAN_MOVE) && warped();
    const bool quiet = calm && look.has(ScreenEvent::SCREEN_STILL);
    if (!calm){
        m_calm_since = WallClock::max();
    }else if (m_calm_since == WallClock::max()){
        m_calm_since = now;
    }
    if (!quiet){
        m_quiet_since = WallClock::max();
    }else if (m_quiet_since == WallClock::max()){
        m_quiet_since = now;
    }
    const bool over =
        (quiet && now - m_quiet_since >= TALK_OVER_QUIET) ||
        (calm && now - m_calm_since >= TALK_OVER_CALM);
    if (over && m_rule.end_on_quiet){
        return SceneMove::DONE;
    }
    return box ? SceneMove::NEXT : SceneMove::B;
}

bool TalkOver::warped() const{
    return !m_rule.warp || m_fade != WarpFade::NONE || m_banner;
}

std::string TalkOver::describe() const{
    if (m_fade == WarpFade::NONE && !m_banner){
        return "no fade or banner";
    }
    std::string out = m_fade == WarpFade::NONE ? "" : warp_fade_name(m_fade);
    if (m_banner){
        out += out.empty() ? "banner read" : ", banner read";
    }
    return out;
}



} // namespace Pokemon
} // namespace PokemonAutomation
