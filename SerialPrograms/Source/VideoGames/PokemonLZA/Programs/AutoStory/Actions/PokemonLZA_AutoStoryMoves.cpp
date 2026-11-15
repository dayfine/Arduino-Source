/*  PokemonLZA AutoStory - Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  Whether the screen on a fresh look has not changed since the look
//  before (SCREEN_STILL): the input in between did nothing, a dropped turn
//  or a dropped hold. A walk into a wall that already stops the player
//  reads still too, and plays its leg twice.
bool screen_still(PlzaBackend& backend){
    return backend.observe().events.has(ScreenEvent::SCREEN_STILL);
}

//  A recorded turn: story_turn(), then L to recenter the camera, since the
//  walk goes where it looks.
void turn_and_recenter(PlzaBackend& backend, double degrees){
    story_turn(backend, degrees);
    backend.press(BUTTON_L, 100ms, 150ms);
}

//  `input`, played again once when the screen stays still after it.
template <typename Input>
void play_checked(PlzaBackend& backend, std::string_view what, Input&& input){
    input();
    if (screen_still(backend)){
        backend.log(std::format("[PLZA] walk: the screen stayed still after {}; once more", what));
        input();
    }
}

} // namespace


std::string story_walk_leg_label(size_t index, size_t count, const StoryWalkLeg& leg){
    std::string label = std::format("leg {}/{} turn {:g}", index + 1, count, leg.turn);
    if (leg.walk > Milliseconds(0)){
        label += std::format(" walk {} ms", leg.walk.count());
    }
    return label;
}

Pokemon::Outcome run_story_move(PlzaBackend& backend, const StoryMove& move){
    //  The look the first input is checked against.
    backend.observe();
    for (size_t index = 0; index < move.legs.size(); index++){
        const StoryWalkLeg& leg = move.legs[index];
        const std::string label = story_walk_leg_label(index, move.legs.size(), leg);
        if (leg.turn != 0){
            play_checked(backend, std::format("the turn of {}", label), [&]{
                turn_and_recenter(backend, leg.turn);
            });
        }
        if (leg.walk > Milliseconds(0)){
            play_checked(backend, std::format("the walk of {}", label), [&]{
                backend.hold(Pokemon::Direction::NORTH, leg.walk, Pokemon::StoryPace::WALK);
            });
        }
    }
    return Pokemon::outcome_ok();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
