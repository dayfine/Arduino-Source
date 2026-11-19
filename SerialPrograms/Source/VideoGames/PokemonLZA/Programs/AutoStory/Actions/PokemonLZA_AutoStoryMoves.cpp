/*  PokemonLZA AutoStory - Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include "PokemonLZA_AutoStoryHeading.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  A dropped turn or hold leaves the screen still. A walk into a wall reads
//  still too, and plays its leg twice.
bool screen_still(PlzaBackend& backend){
    return backend.observe().events.has(ScreenEvent::SCREEN_STILL);
}

//  L since the walk goes where the camera looks.
void turn_and_recenter(PlzaBackend& backend, double degrees){
    story_turn(backend, degrees);
    backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
}

template <typename Input>
void play_checked(PlzaBackend& backend, std::string_view what, Input&& input){
    input();
    if (screen_still(backend)){
        backend.log(std::format("[PLZA] walk: the screen stayed still after {}; once more", what));
        input();
    }
}

void recenter_before_held_leg(PlzaBackend& backend, size_t index, const StoryWalkLeg& leg){
    if (index > 0 && leg.turn != 0){
        backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    }
}

//  A turn-only leg is a 150 ms tap.
void run_held_legs(PlzaBackend& backend, const std::vector<StoryWalkLeg>& legs){
    for (size_t index = 0; index < legs.size(); index++){
        const StoryWalkLeg& leg = legs[index];
        recenter_before_held_leg(backend, index, leg);
        if (leg.walk > 0ms){
            backend.stick(leg.turn, leg.walk, /*release=*/0ms, Pokemon::StoryPace::RUN);
        }else if (leg.turn != 0){
            backend.stick(leg.turn, /*duration=*/150ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
        }
    }
}

//  "leg 2/5 turn 90 walk 1200 ms"
std::string story_walk_leg_label(size_t index, size_t count, const StoryWalkLeg& leg){
    std::string label = std::format("leg {}/{} turn {:g}", index + 1, count, leg.turn);
    if (leg.walk > Milliseconds(0)){
        label += std::format(" walk {} ms", leg.walk.count());
    }
    return label;
}

} // namespace


std::optional<Pokemon::StoryFailure> run_story_move(PlzaBackend& backend, const StoryMove& move){
    if (move.style == StoryWalkStyle::HELD){
        run_held_legs(backend, move.legs);
        return std::nullopt;
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
                backend.stick(/*degrees=*/0, leg.walk, /*release=*/0ms, Pokemon::StoryPace::RUN);
            });
        }
    }
    return std::nullopt;
}

std::optional<Pokemon::StoryFailure> run_story_move_but_last_walk(PlzaBackend& backend, const StoryMove& move){
    if (move.legs.empty()){
        return std::nullopt;
    }
    StoryMove head = move;
    if (move.style == StoryWalkStyle::HELD){
        if (move.legs.back().walk == 0ms){
            run_held_legs(backend, move.legs);
            return std::nullopt;
        }
        head.legs.pop_back();
        run_held_legs(backend, head.legs);
        recenter_before_held_leg(backend, head.legs.size(), move.legs.back());
        return std::nullopt;
    }
    head.legs.back().walk = 0ms;
    return run_story_move(backend, head);
}

void hold_last_leg(PlzaBackend& backend, const StoryMove& move, Milliseconds slice){
    if (move.style == StoryWalkStyle::HELD && !move.legs.empty()){
        backend.stick(move.legs.back().turn, slice, /*release=*/0ms, Pokemon::StoryPace::RUN);
    }else{
        backend.stick(/*degrees=*/0, slice, /*release=*/0ms, Pokemon::StoryPace::RUN);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
