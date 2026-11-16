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
    backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
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

//  StoryWalkStyle::HELD: L before leg `index` when it turns after an
//  earlier leg.
void recenter_before_held_leg(PlzaBackend& backend, size_t index, const StoryWalkLeg& leg){
    if (index > 0 && leg.turn != 0){
        backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    }
}

//  StoryWalkStyle::HELD: each leg's stick held at its angle for its time
//  (a turn-only leg a 150 ms tap); L before each later leg that turns.
void run_held_legs(PlzaBackend& backend, const std::vector<StoryWalkLeg>& legs){
    for (size_t index = 0; index < legs.size(); index++){
        const StoryWalkLeg& leg = legs[index];
        recenter_before_held_leg(backend, index, leg);
        if (leg.walk > 0ms){
            backend.stick(leg.turn, leg.walk, /*release=*/0ms, Pokemon::StoryPace::WALK);
        }else if (leg.turn != 0){
            backend.stick(leg.turn, /*duration=*/150ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
        }
    }
}

//  "leg 2/5 turn 90 walk 1200 ms": leg `index` (from 0) of `count`, for the
//  log. A leg with no walk leaves it out.
std::string story_walk_leg_label(size_t index, size_t count, const StoryWalkLeg& leg){
    std::string label = std::format("leg {}/{} turn {:g}", index + 1, count, leg.turn);
    if (leg.walk > Milliseconds(0)){
        label += std::format(" walk {} ms", leg.walk.count());
    }
    return label;
}

} // namespace


Pokemon::Outcome run_story_move(PlzaBackend& backend, const StoryMove& move){
    if (move.style == StoryWalkStyle::HELD){
        run_held_legs(backend, move.legs);
        return Pokemon::outcome_ok();
    }
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

Pokemon::Outcome run_story_move_but_last_walk(PlzaBackend& backend, const StoryMove& move){
    if (move.legs.empty()){
        return Pokemon::outcome_ok();
    }
    StoryMove head = move;
    if (move.style == StoryWalkStyle::HELD){
        if (move.legs.back().walk == 0ms){
            run_held_legs(backend, move.legs);
            return Pokemon::outcome_ok();
        }
        head.legs.pop_back();
        run_held_legs(backend, head.legs);
        recenter_before_held_leg(backend, head.legs.size(), move.legs.back());
        return Pokemon::outcome_ok();
    }
    head.legs.back().walk = 0ms;
    return run_story_move(backend, head);
}

void hold_last_leg(PlzaBackend& backend, const StoryMove& move, Milliseconds slice){
    if (move.style == StoryWalkStyle::HELD && !move.legs.empty()){
        backend.stick(move.legs.back().turn, slice, /*release=*/0ms, Pokemon::StoryPace::WALK);
    }else{
        backend.hold(Pokemon::Direction::NORTH, slice, Pokemon::StoryPace::WALK);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
