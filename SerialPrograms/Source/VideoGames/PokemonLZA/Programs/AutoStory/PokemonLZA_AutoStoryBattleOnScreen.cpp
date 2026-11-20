/*  PokemonLZA AutoStory - Battle on screen
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <string>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "PokemonLZA_AutoStoryActionNames.h"
#include "PokemonLZA_AutoStoryBattleOnScreen.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


FlickerFilter::FlickerFilter(size_t hits, Milliseconds window, Milliseconds off_after)
    : m_hits(hits)
    , m_window(window)
    , m_off_after(off_after)
{}

bool FlickerFilter::on_with(bool seen, WallClock when) const{
    if (seen){
        const size_t recent = (size_t)std::count_if(m_seen.begin(), m_seen.end(), [&](WallClock at){
            return when - at <= m_window;
        });
        return m_on || recent + 1 >= m_hits;
    }
    return m_on && !m_seen.empty() && when - m_seen.back() < m_off_after;
}

void FlickerFilter::record(bool seen, WallClock when){
    m_on = on_with(seen, when);
    if (seen){
        m_seen.push_back(when);
    }
    while (m_seen.size() > 1 && when - m_seen.front() > m_window){
        m_seen.pop_front();
    }
}

FlickerFilter battle_filter(){
    return FlickerFilter(/*hits=*/2, /*window=*/2s, /*off_after=*/3s);
}

Pokemon::StoryFailure battle_on_screen_failure(){
    return Pokemon::StoryFailure{
        .code = "battle_on_screen",
        .detail = "battle on screen",
        .resume_at_action = std::string(action_name(PlzaAction::STORY_BATTLE)),
    };
}

bool battle_showing(PlzaBackend& backend){
    FlickerFilter battle = battle_filter();
    for (size_t looks = 0; looks < 8; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        const bool seen = look.events.has(ScreenEvent::BATTLE_STARTED);
        if (battle.on_with(seen, look.when)){
            return true;
        }
        if (looks == 0 && !seen){
            return false;
        }
        battle.record(seen, look.when);
        backend.wait(250ms);
    }
    return false;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
