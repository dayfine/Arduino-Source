/*  AutoStory Story Preflight
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include "Pokemon_StoryPreflight.h"

namespace PokemonAutomation{
namespace Pokemon{


Outcome StoryPreflight::check() const{
    std::string failed;
    auto add = [&failed](bool ok, std::string_view what){
        if (ok){
            return;
        }
        if (!failed.empty()){
            failed += "; ";
        }
        failed += what;
    };
    add(video_alive, "no live video");
    add(controller_ready, "the controller is not ready");
    add(game_running, "the game is not running");
    if (failed.empty()){
        return outcome_ok();
    }
    return outcome_failed(std::string(FAILURE), failed);
}



} // namespace Pokemon
} // namespace PokemonAutomation
