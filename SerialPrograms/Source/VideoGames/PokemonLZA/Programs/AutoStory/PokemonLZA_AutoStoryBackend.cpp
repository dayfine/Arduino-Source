/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <cstdlib>
#include <format>
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  Turns land within 2; at 8 off, a 6.5 s leg misses a gate.
constexpr double HEADING_TOLERANCE = 4;

constexpr int HEADING_TURNS = 4;

//  The lab feed has run 0.4 s behind the game: covers that lag.
constexpr int TURN_READS = 10;
constexpr Milliseconds TURN_READ_GAP = 100ms;

//  In (-180, 180].
double turn_to(double now, double heading){
    double turn = std::fmod(heading - now, 360.0);
    if (turn > 180){
        turn -= 360;
    }else if (turn <= -180){
        turn += 360;
    }
    return turn;
}

//  The last read when the arrow never moves half the turn: the turn did not take.
double read_after_turn(PlzaBackend& backend, const MinimapArrowReader& read_arrow, double before, double turn){
    backend.observe();
    double now = read_arrow();
    for (int reads = 1; reads < TURN_READS; reads++){
        if (std::abs(turn_to(before, now)) >= std::abs(turn) / 2){
            break;
        }
        backend.wait(TURN_READ_GAP);
        backend.observe();
        now = read_arrow();
    }
    return now;
}

} // namespace

std::optional<Pokemon::StoryFailure> face_heading(PlzaBackend& backend, const MinimapArrowReader& read_arrow, double heading){
    backend.observe();
    double now = read_arrow();
    //  Small turns overshoot (5 asked, 10 turned): after each overshoot the
    //  next turn asks for half as much.
    double gain = 1;
    double last_turn = 0;
    for (int turns = 0; turns <= HEADING_TURNS; turns++){
        const double turn = turn_to(now, heading);
        backend.log_detail(std::format("[PLZA] face_heading: the arrow reads {:.1f} deg, {:.0f} wanted: {:.1f} off", now, heading, turn));
        if (std::abs(turn) <= HEADING_TOLERANCE){
            backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
            return std::nullopt;
        }
        if (turns < HEADING_TURNS){
            if (last_turn * turn < 0){
                gain /= 2;
            }
            last_turn = turn;
            const double asked = turn * gain;
            story_turn(backend, asked);
            now = read_after_turn(backend, read_arrow, now, asked);
        }
    }
    return Pokemon::StoryFailure{.code = "heading_missed", .detail = std::format("not facing {:.0f} deg after {} turns", heading, HEADING_TURNS)};
}

void story_turn(PlzaBackend& backend, double degrees){
    //  The tap the story was recorded with; short releases run legs back to back.
    backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    backend.stick(degrees, /*duration=*/150ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
