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
//  Walking this long in a wild battle fails the walk. Away from the
//  Pokemon toward the gate the battle ends; not measured on console.
constexpr Milliseconds GATE_BATTLE_LIMIT(20000);
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

//  Logs the walk's end at the A prompt, with the heading there by `arrow`:
//  the street's way through the gate, for a plan's `heading` (no arrow
//  reader: -1).
void log_gate_prompt(PlzaBackend& backend, const MinimapArrowReader& arrow, Milliseconds walked, std::string_view readings){
    backend.log(std::format(
        "[PLZA] walk_to_zone_gate_entrance: the A prompt shows after {} ms ({}), heading {:.0f}",
        walked.count(), readings, arrow ? arrow() : -1.0
    ));
}

//  A gate walk's time in a wild battle, walked on through at the walk's
//  angle.
class BattleWalk{
public:
    //  One slice on at `angle` (`walked` the walk's time outside battles,
    //  for the log); once GATE_BATTLE_LIMIT is spent, none and a
    //  "wild_battle" failure with the walk's `readings` (gate_readings()).
    Pokemon::Outcome step(PlzaBackend& backend, double angle, Milliseconds walked, std::string_view readings){
        if (m_time == Milliseconds(0)){
            backend.log(std::format("[PLZA] walk_to_zone_gate_entrance: a wild battle after {} ms: walking on at {} deg", walked.count(), angle));
        }
        if (m_time >= GATE_BATTLE_LIMIT){
            return Pokemon::outcome_failed("wild_battle", std::format(
                "walk_to_zone_gate_entrance: still in a wild battle after {} ms of walking on, {}", m_time.count(), readings
            ));
        }
        backend.stick(angle, GATE_WALK_SLICE, /*release=*/0ms, Pokemon::StoryPace::WALK);
        m_time += GATE_WALK_SLICE;
        return Pokemon::outcome_ok();
    }

private:
    Milliseconds m_time = Milliseconds(0);
};

//  Sets a look's ZONE_GATE_X_READING and ZONE_GATE_WIDTH_READING from its
//  frame (read_zone_gate()); nothing on a look with no frame or no gate.
LookReader zone_gate_reader(){
    return [](Pokemon::Observation<Pokemon::ZonePose>& look){
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

} // namespace


Pokemon::Outcome walk_to_zone_gate_entrance(
    PlzaBackend& backend, const MinimapArrowReader& arrow, const LookReader& gate, const GateWalk& walk
){
    const Pokemon::Outcome faced = walk.heading && arrow ? face_heading(backend, arrow, *walk.heading) : Pokemon::outcome_ok();
    if (!faced.succeeded()){
        return faced;
    }
    Milliseconds walked(0);
    double angle = 0;
    size_t seen = 0;
    std::optional<GateRead> last;
    BattleWalk battle;
    for (size_t looks = 1; ; looks++){
        backend.scope().throw_if_cancelled();
        Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (gate){
            gate(look);
        }
        if (look.events.has(PokemonAutomation::ScreenEvent::INTERACT_PROMPT)){
            log_gate_prompt(backend, arrow, walked, gate_readings(seen, looks, last));
            return Pokemon::outcome_ok();
        }
        if (look.events.has(PokemonAutomation::ScreenEvent::BATTLE_STARTED)){
            const Pokemon::Outcome walked_on = battle.step(backend, angle, walked, gate_readings(seen, looks, last));
            if (!walked_on.succeeded()){
                return walked_on;
            }
            continue;
        }
        const std::optional<GateRead> read = gate_on(look);
        if (read){
            seen++;
            last = read;
            angle = gate_steer_degrees(read->x / 1000.0);
        }
        if (!last && walked >= GATE_FIRST_SEEN_WITHIN){
            return Pokemon::outcome_failed("gate_not_seen", std::format(
                "walk_to_zone_gate_entrance: {} in {} ms of walking", gate_readings(seen, looks, last), walked.count()
            ));
        }
        if (walked >= walk.timeout){
            return Pokemon::outcome_failed("no_prompt", std::format(
                "walk_to_zone_gate_entrance: no A prompt in {} ms of walking, {}", walk.timeout.count(), gate_readings(seen, looks, last)
            ));
        }
        backend.stick(angle, GATE_WALK_SLICE, /*release=*/0ms, Pokemon::StoryPace::WALK);
        walked += GATE_WALK_SLICE;
    }
}

Pokemon::Outcome walk_to_zone_gate(StoryEnv& env, PlzaBackend& backend, const GateWalk& walk){
    return walk_to_zone_gate_entrance(backend, minimap_arrow(env), zone_gate_reader(), walk);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
