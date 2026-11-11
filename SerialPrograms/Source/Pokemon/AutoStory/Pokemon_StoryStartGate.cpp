/*  AutoStory Story Start Gate
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include "Pokemon_StoryStartGate.h"

namespace PokemonAutomation{
namespace Pokemon{


Outcome check_story_preflight(const StoryPreflight& preflight){
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
    add(preflight.video_alive, "no live video");
    add(preflight.controller_ready, "the controller is not ready");
    add(preflight.game_running, "the game is not running");
    if (failed.empty()){
        return outcome_ok();
    }
    return outcome_failed(std::string(STORY_PRECONDITION_FAILED), failed);
}

bool story_first_save_allowed(StoryStart start){
    return start == StoryStart::NEW_GAME;
}

std::string_view story_start_name(StoryStart start){
    switch (start){
    case StoryStart::NEW_GAME:
        return "a new game";
    case StoryStart::LIVE:
        return "the game as it is";
    case StoryStart::FROM_SAVE:
        return "its save";
    }
    return "?";
}



} // namespace Pokemon
} // namespace PokemonAutomation
