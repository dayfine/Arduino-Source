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

//  How close to a run's heading the minimap's arrow must read. 4, not 10:
//  03.09's walk took 272 for 264 and its 6.5 s leg ended right of the lab
//  gate (console B, 2026-11-17); turns land within 2 (2026-11-14 to 17).
constexpr double HEADING_TOLERANCE = 4;

//  The turns face_heading() takes at most.
constexpr int HEADING_TURNS = 4;

//  How a turn is read back: up to this many arrow reads, this far apart,
//  until the arrow has moved at least half the turn. The lab feed has run
//  0.4 s behind the game at 10 fps: the read right after a turn showed the
//  facing from before it, the same on every frame until the feed caught up.
constexpr int TURN_READS = 10;
constexpr Milliseconds TURN_READ_GAP = 100ms;

//  How far clockwise `heading` is of `now`, the short way round: in
//  (-180, 180].
double turn_to(double now, double heading){
    double turn = std::fmod(heading - now, 360.0);
    if (turn > 180){
        turn -= 360;
    }else if (turn <= -180){
        turn += 360;
    }
    return turn;
}

//  The arrow once the turn of `turn` degrees from `before` shows on the
//  feed: read until it has moved at least half the turn, TURN_READS reads
//  at most. The last read when it never does: the turn did not take.
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

Pokemon::Outcome face_heading(PlzaBackend& backend, const MinimapArrowReader& read_arrow, double heading){
    backend.observe();
    double now = read_arrow();
    //  Small turns overshoot: 03.08 asked for 5 and got 10, then -5 and got
    //  -10, back and forth around 254 (console B, 2026-11-18). Each time a
    //  turn lands past the heading, the next one asks for half as much.
    double gain = 1;
    double last_turn = 0;
    for (int turns = 0; turns <= HEADING_TURNS; turns++){
        const double turn = turn_to(now, heading);
        backend.log_detail(std::format("[PLZA] face_heading: the arrow reads {:.1f} deg, {:.0f} wanted: {:.1f} off", now, heading, turn));
        if (std::abs(turn) <= HEADING_TOLERANCE){
            backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
            return Pokemon::outcome_ok();
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
    return Pokemon::outcome_failed("heading_missed", std::format("not facing {:.0f} deg after {} turns", heading, HEADING_TURNS));
}

void story_turn(PlzaBackend& backend, double degrees){
    //  150 ms is the tap the story was recorded with.
    //  Short releases: the legs run back to back, as when played by hand.
    backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    backend.stick(degrees, /*duration=*/150ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
