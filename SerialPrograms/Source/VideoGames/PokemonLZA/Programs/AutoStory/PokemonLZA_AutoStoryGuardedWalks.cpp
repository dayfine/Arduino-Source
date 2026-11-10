/*  PokemonLZA AutoStory - Guarded walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include "PokemonLZA_AutoStoryGuardedWalks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using EngineEvent = PokemonAutomation::ScreenEvent;


namespace{

//  How long each slice of the held last leg is, between looks for its end.
constexpr Milliseconds HOLD_SLICE = 250ms;

//  What no walk may start on, by name; empty when free.
std::string not_free_control(const ScreenObservation& look){
    if (has_choice(look)){
        return "a choice";
    }
    if (look.events.has(EngineEvent::TEXT_BOX)){
        return "a text box";
    }
    if (look.events.has(EngineEvent::BATTLE)){
        return "a battle";
    }
    if (look.events.has(EngineEvent::FADE_BLACK) || look.events.has(EngineEvent::FADE_WHITE)){
        return "a fade";
    }
    return "";
}

} // namespace


Pokemon::Outcome check_can_walk(PlzaStoryBackend& backend, std::string_view walk){
    for (int read = 0; read < 2; read++){
        if (read > 0){
            backend.wait(150ms);
        }
        const std::string busy = not_free_control(backend.observe());
        if (!busy.empty()){
            return Pokemon::outcome_failed("walk_not_free", std::format("{}: the walk would start on {}, not free control", walk, busy));
        }
    }
    return Pokemon::outcome_ok();
}

Pokemon::Outcome walk_until(
    PlzaStoryBackend& backend, std::string_view walk, const StoryMove& move, EngineEvent end
){
    StoryMove head = move;
    Milliseconds hold = 0ms;
    if (!head.legs.empty()){
        hold = head.legs.back().walk;
        head.legs.back().walk = 0ms;
    }
    const Pokemon::Outcome walked = run_story_move(backend, head);
    if (!walked.succeeded()){
        return walked;
    }
    //  The slices stop the stick between them for as long as a look takes;
    //  the player barely slows.
    for (Milliseconds held = 0ms; held < hold; held += HOLD_SLICE){
        backend.hold(Pokemon::Direction::NORTH, HOLD_SLICE, Pokemon::StoryPace::WALK);
        if (backend.observe().events.has(end)){
            backend.log(std::format("{}: the end event after {} ms of the last leg.", walk, (held + HOLD_SLICE).count()));
            return Pokemon::outcome_ok();
        }
    }
    return Pokemon::outcome_failed("walk_no_end", std::format("{}: no end event within {} ms of the last leg", walk, hold.count()));
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
