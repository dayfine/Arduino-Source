/*  PokemonLZA AutoStory - Walk to a wild zone's gate
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <string>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryZoneGateReader.h"
#include "PokemonLZA_AutoStoryGateWalk.h"
#include "PokemonLZA_AutoStoryMoves.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryFailures.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  One slice of the walk between looks, as walk_to_prompt()'s.
constexpr Milliseconds GATE_WALK_SLICE(250);
//  Walking this long without a look at the gate fails the walk. From
//  Wild Zone 1's catch spot at 60 the far gate is small at first: 1 s
//  missed it once (console B, 2026-11-14).
constexpr Milliseconds GATE_FIRST_SEEN_WITHIN(3000);
//  The screen's width in degrees of view, for the steer: a guess.
constexpr double VIEW_DEGREES = 60;
//  Closer to the middle than this, the gate is dead ahead.
constexpr double DEAD_AHEAD = 0.02;

//  The gate's readings on a look, if any.
struct GateRead{
    int x = 0;
    int width = 0;
};

std::optional<GateRead> gate_on(const Pokemon::Observation<Pokemon::ZonePose>& look){
    const auto x = look.readings.find(ZONE_GATE_X_READING);
    const auto width = look.readings.find(ZONE_GATE_WIDTH_READING);
    if (x == look.readings.end() || width == look.readings.end()){
        return std::nullopt;
    }
    return GateRead{.x = x->second, .width = width->second};
}

//  "gate on 3 of 5 looks, last at x = 0.520 width 0.150", or "no gate on 5 looks".
std::string gate_readings(size_t seen, size_t looks, const std::optional<GateRead>& last){
    if (!last){
        return std::format("no gate on {} looks", looks);
    }
    return std::format("gate on {} of {} looks, last at x = {:.3f} width {:.3f}", seen, looks, last->x / 1000.0, last->width / 1000.0);
}

//  The stick's angle, degrees clockwise of straight on, toward a gate at
//  `x` (0 to 1) on screen.
double gate_steer_degrees(double x){
    const double off = x - 0.5;
    if (std::abs(off) < DEAD_AHEAD){
        return 0;
    }
    return std::round(off * VIEW_DEGREES);
}

//  Logs the walk's end at the A prompt, with the heading there: the street's
//  way through the gate, for a plan's `heading` (none read: -1).
void log_gate_prompt(
    PlzaBackend& backend,
    const PlzaReader& heading,
    Pokemon::Observation<Pokemon::ZonePose>& look,
    Milliseconds walked,
    std::string_view readings
){
    if (heading){
        heading(backend, look);
    }
    backend.log(std::format(
        "[PLZA] walk_to_zone_gate_entrance: the A prompt shows after {} ms ({}), heading {}",
        walked.count(), readings, Pokemon::story_heading(look).value_or(-1)
    ));
}

} // namespace


PlzaReader zone_gate_reader(){
    return [](PlzaBackend&, Pokemon::Observation<Pokemon::ZonePose>& look){
        if (!look.frame || !look.frame->frame){
            return;
        }
        const std::optional<ZoneGateSighting> gate = read_zone_gate(*look.frame->frame);
        if (!gate){
            return;
        }
        look.readings[std::string(ZONE_GATE_X_READING)] = (int)std::lround(gate->x * 1000);
        look.readings[std::string(ZONE_GATE_WIDTH_READING)] = (int)std::lround(gate->width * 1000);
    };
}

Pokemon::Outcome walk_to_zone_gate_entrance(
    PlzaBackend& backend, const PlzaReader& heading, const PlzaReader& gate, const GateWalk& walk
){
    if (walk.heading){
        const Pokemon::Outcome faced = face_heading(backend, heading, *walk.heading, 10.0);
        if (!faced.succeeded()){
            return faced;
        }
    }
    Milliseconds walked(0);
    double angle = 0;
    size_t looks = 0;
    size_t seen = 0;
    std::optional<GateRead> last;
    while (true){
        backend.scope().throw_if_cancelled();
        Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (gate){
            gate(backend, look);
        }
        looks++;
        if (look.events.has(PokemonAutomation::ScreenEvent::INTERACT_PROMPT)){
            log_gate_prompt(backend, heading, look, walked, gate_readings(seen, looks, last));
            return Pokemon::outcome_ok();
        }
        const std::optional<GateRead> read = gate_on(look);
        if (read){
            seen++;
            last = read;
            angle = gate_steer_degrees(read->x / 1000.0);
        }
        if (!last && walked >= GATE_FIRST_SEEN_WITHIN){
            return plza_failed("gate_not_seen", std::format(
                "walk_to_zone_gate_entrance: {} in {} ms of walking", gate_readings(seen, looks, last), walked.count()
            ));
        }
        if (walked >= walk.timeout){
            return plza_failed("no_prompt", std::format(
                "walk_to_zone_gate_entrance: no A prompt in {} ms of walking, {}", walk.timeout.count(), gate_readings(seen, looks, last)
            ));
        }
        backend.stick(angle, GATE_WALK_SLICE, 0ms, Pokemon::StoryPace::WALK);
        walked += GATE_WALK_SLICE;
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
