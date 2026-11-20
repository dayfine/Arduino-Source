/*  PokemonLZA AutoStory - Battle on screen
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "PokemonLZA_AutoStoryActionNames.h"
#include "PokemonLZA_AutoStoryBattleOnScreen.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


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
