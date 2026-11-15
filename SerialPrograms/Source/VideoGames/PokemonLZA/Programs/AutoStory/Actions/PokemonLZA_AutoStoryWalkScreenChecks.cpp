/*  PokemonLZA AutoStory - Walk screen checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include "PokemonLZA_AutoStoryWalkScreenChecks.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryFailures.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using EngineEvent = PokemonAutomation::ScreenEvent;


namespace{

//  How long each slice of the held last leg is, between looks for its end.
constexpr Milliseconds HOLD_SLICE = 250ms;
//  How long a walk waits for free control, and how often it looks.
constexpr Milliseconds FREE_CONTROL_WAIT = 15000ms;
constexpr Milliseconds FREE_CONTROL_LOOK = 250ms;

//  What no walk may start on, by name; empty when free.
std::string not_free_control(const ScreenObservation& look){
    if (look.readings.contains(std::string(MISSION_BANNER_READING))){
        return "a mission banner";
    }
    if (look.readings.contains(std::string(ADVANCE_ARROW_READING))){
        return "text waiting for A";
    }
    if (look.events.has(EngineEvent::CHOICE_BOX)){
        return "a choice";
    }
    if (look.events.has(EngineEvent::DIALOG_BOX)){
        return "a text box";
    }
    if (look.events.has(EngineEvent::BATTLE_STARTED)){
        return "a battle";
    }
    if (look.events.has(EngineEvent::FADE_BLACK) || look.events.has(EngineEvent::FADE_WHITE)){
        return "a fade";
    }
    return "";
}

} // namespace


//  Waits up to FREE_CONTROL_WAIT for two looks in a row on free control: a
//  mission banner or the last line of a scene can still be up when the step
//  before ends (chase_pancham's legs went in under the
//  MAIN MISSION 01 banner).
Pokemon::Outcome check_can_walk(PlzaBackend& backend, std::string_view walk){
    std::string busy;
    int free_looks = 0;
    for (Milliseconds waited = 0ms; waited <= FREE_CONTROL_WAIT; waited += FREE_CONTROL_LOOK){
        busy = not_free_control(backend.observe());
        free_looks = busy.empty() ? free_looks + 1 : 0;
        if (free_looks >= 2){
            return Pokemon::outcome_ok();
        }
        backend.wait(FREE_CONTROL_LOOK);
    }
    return plza_failed(
        "walk_not_free",
        std::format("{}: no free control within {} ms, last on {}", walk, FREE_CONTROL_WAIT.count(), busy)
    );
}

void finish_walk(PlzaBackend& backend, std::string_view walk){
    const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
    backend.log(std::format("[PLZA] {}: legs done, screen now: {}", walk, describe(look.events)));
}

Pokemon::Outcome walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, const PlzaReader& heading, EngineEvent end,
    Pokemon::StoryPausePoints* pause
){
    StoryMove head = move;
    Milliseconds hold = 0ms;
    if (!head.legs.empty()){
        hold = head.legs.back().walk;
        head.legs.back().walk = 0ms;
    }
    const Pokemon::Outcome walked = run_story_move(backend, head, heading, pause);
    if (!walked.succeeded()){
        return walked;
    }
    //  The slices stop the stick between them for as long as a look takes;
    //  the player barely slows.
    for (Milliseconds held = 0ms; held < hold;){
        //  The last slice holds only what is left, so the leg walks its time.
        const Milliseconds slice = std::min(HOLD_SLICE, hold - held);
        backend.hold(Pokemon::Direction::NORTH, slice, Pokemon::StoryPace::WALK);
        held += slice;
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (look.events.has(end)){
            backend.log(std::format("{}: the end event after {} ms of the last leg.", walk, held.count()));
            return Pokemon::outcome_ok();
        }
        //  A black fade is the scene at the walk's end taking over (03.11's
        //  Mable, the fade 1-6 s into the last leg,
        //  her box more than 5 s after it, past a camera pan): the step after
        //  the walk waits for what the scene shows.
        if (look.events.has(EngineEvent::FADE_BLACK)){
            backend.log(std::format("{}: a black fade after {} ms of the last leg: the scene takes over.", walk, held.count()));
            return Pokemon::outcome_ok();
        }
    }
    return plza_failed("walk_no_end", std::format("{}: no end event within {} ms of the last leg", walk, hold.count()));
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
