/*  PokemonLZA AutoStory - Walk to a wild zone's gate
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <string>
#include <utility>
#include "PokemonLZA_AutoStoryGateWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  One slice of the walk between looks.
constexpr Milliseconds GATE_WALK_SLICE(250);
//  Walking this long without a look at the gate fails the walk. From
//  Wild Zone 1's catch spot at 60 the far gate is small at first: 1 s
//  missed it once (console B, 2026-11-14).
constexpr Milliseconds GATE_FIRST_SEEN_WITHIN(3000);
//  Walking this long in a wild battle fails the walk. Away from the
//  Pokemon toward the gate the battle ends; not measured on console.
constexpr Milliseconds GATE_BATTLE_LIMIT(20000);
//  Walking on in a battle reads the map pin this often, to tell a wall.
constexpr Milliseconds GATE_PIN_EVERY(2000);
//  A pin that moved less than this many map pixels in GATE_PIN_EVERY did
//  not get on: the leg to the 60 spot covered 7 a second (console B,
//  2026-11-16).
constexpr double GATE_PIN_STILL = 2;
//  This many looks in a row without the lead's panel: out of the zone.
constexpr size_t OUT_OF_ZONE_LOOKS = 2;
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

//  Where a gate walk's looks put the player by the lead's panel
//  (ZONE_PANEL_READING): in the zone once a look read it, and how many
//  looks in a row since then read it gone.
struct ZonePresence{
    bool inside = false;
    size_t without_panel = 0;
};

ZonePresence presence_on(ZonePresence presence, const Pokemon::Observation<Pokemon::ZonePose>& look){
    const auto panel = look.readings.find(ZONE_PANEL_READING);
    const bool shows = panel != look.readings.end() && panel->second != 0;
    const bool gone = panel != look.readings.end() && panel->second == 0;
    presence.inside = presence.inside || shows;
    presence.without_panel = presence.inside && gone ? presence.without_panel + 1 : 0;
    return presence;
}

//  A gate walk's time in a wild battle, walked on through at the walk's
//  angle, the map pin read every GATE_PIN_EVERY by `pin` (if set).
class BattleWalk{
public:
    explicit BattleWalk(PinReader pin)
        : m_pin(std::move(pin))
    {}

    //  Whether the pin stood still on a read: walking on runs into a wall.
    bool stalled() const{
        return m_stalled;
    }

    //  One slice on at `angle` (`walked` the walk's time outside battles,
    //  for the log), or none when the pin read before it has not moved
    //  (stalled()); once GATE_BATTLE_LIMIT is spent, none and a
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
        if (m_pin && m_time % GATE_PIN_EVERY == Milliseconds(0) && pin_still(backend)){
            m_stalled = true;
            return Pokemon::outcome_ok();
        }
        backend.stick(angle, GATE_WALK_SLICE, /*release=*/0ms, Pokemon::StoryPace::WALK);
        m_time += GATE_WALK_SLICE;
        return Pokemon::outcome_ok();
    }

private:
    //  Reads the pin; whether it moved under GATE_PIN_STILL since the last
    //  read. An unread pin tells nothing.
    bool pin_still(PlzaBackend& backend){
        const std::optional<MapPoint> now = m_pin();
        if (!now){
            return false;
        }
        const bool still = m_last && std::hypot(now->x - m_last->x, now->y - m_last->y) < GATE_PIN_STILL;
        if (still){
            backend.log(std::format(
                "[PLZA] walk_to_zone_gate_entrance: the pin stood at ({:.0f}, {:.0f}) over {} ms of walking on: a wall, re-aiming",
                now->x, now->y, GATE_PIN_EVERY.count()
            ));
        }
        m_last = now;
        return still;
    }

    PinReader m_pin;
    Milliseconds m_time = Milliseconds(0);
    std::optional<MapPoint> m_last;
    bool m_stalled = false;
};

//  What a gate walk's looks have seen of the gate, and the stick's angle
//  toward it.
struct GateSight{
    size_t seen = 0;
    std::optional<GateRead> last;
    double angle = 0;
};

//  `sight` turned toward the gate when `look` shows it.
GateSight seen_on(GateSight sight, const Pokemon::Observation<Pokemon::ZonePose>& look){
    const std::optional<GateRead> read = gate_on(look);
    if (read){
        sight.seen++;
        sight.last = read;
        sight.angle = gate_steer_degrees(read->x / 1000.0);
    }
    return sight;
}

//  The walk's end on `look`, if it is one: the gate's A prompt, or out of
//  the zone (`without_panel` looks in a row without the lead's panel).
std::optional<Pokemon::Outcome> walk_end(
    PlzaBackend& backend, const MinimapArrowReader& arrow, const Pokemon::Observation<Pokemon::ZonePose>& look,
    size_t without_panel, Milliseconds walked, std::string_view readings
){
    if (look.events.has(PokemonAutomation::ScreenEvent::INTERACT_PROMPT)){
        log_gate_prompt(backend, arrow, walked, readings);
        return Pokemon::outcome_ok();
    }
    if (without_panel >= OUT_OF_ZONE_LOOKS){
        backend.log(std::format(
            "[PLZA] walk_to_zone_gate_entrance: out of the zone after {} ms, the lead's panel gone ({})",
            walked.count(), readings
        ));
        return Pokemon::outcome_ok();
    }
    return std::nullopt;
}

//  The walk's failure before its next slice, if any: no gate seen in
//  GATE_FIRST_SEEN_WITHIN, or no prompt by the timeout.
std::optional<Pokemon::Outcome> walk_failure(const GateSight& sight, Milliseconds walked, Milliseconds timeout, std::string_view readings){
    if (!sight.last && walked >= GATE_FIRST_SEEN_WITHIN){
        return Pokemon::outcome_failed("gate_not_seen", std::format(
            "walk_to_zone_gate_entrance: {} in {} ms of walking", readings, walked.count()
        ));
    }
    if (walked >= timeout){
        return Pokemon::outcome_failed("no_prompt", std::format(
            "walk_to_zone_gate_entrance: no A prompt in {} ms of walking, {}", timeout.count(), readings
        ));
    }
    return std::nullopt;
}

//  Faces `walk`'s heading by `arrow` (face_heading()), when both are set.
Pokemon::Outcome face_walk_heading(PlzaBackend& backend, const MinimapArrowReader& arrow, const GateWalk& walk){
    return walk.heading && arrow ? face_heading(backend, arrow, *walk.heading) : Pokemon::outcome_ok();
}

} // namespace


Pokemon::Outcome walk_to_zone_gate_entrance(
    PlzaBackend& backend, const MinimapArrowReader& arrow, const LookReader& gate, const PinReader& pin,
    const GateWalk& walk
){
    const Pokemon::Outcome faced = face_walk_heading(backend, arrow, walk);
    if (!faced.succeeded()){
        return faced;
    }
    Milliseconds walked(0);
    GateSight sight;
    ZonePresence zone;
    BattleWalk battle(pin);
    for (size_t looks = 1; ; looks++){
        backend.scope().throw_if_cancelled();
        Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (gate){
            gate(look);
        }
        zone = presence_on(zone, look);
        const std::optional<Pokemon::Outcome> end = walk_end(
            backend, arrow, look, zone.without_panel, walked, gate_readings(sight.seen, looks, sight.last)
        );
        if (end){
            return *end;
        }
        if (zone.without_panel == 0 && !battle.stalled() && look.events.has(PokemonAutomation::ScreenEvent::BATTLE_STARTED)){
            const Pokemon::Outcome walked_on = battle.step(backend, sight.angle, walked, gate_readings(sight.seen, looks, sight.last));
            if (!walked_on.succeeded()){
                return walked_on;
            }
            if (!battle.stalled()){
                continue;
            }
            const Pokemon::Outcome refaced = face_walk_heading(backend, arrow, walk);
            if (!refaced.succeeded()){
                return refaced;
            }
        }
        sight = seen_on(sight, look);
        const std::optional<Pokemon::Outcome> failure = walk_failure(
            sight, walked, walk.timeout, gate_readings(sight.seen, looks, sight.last)
        );
        if (failure){
            return *failure;
        }
        backend.stick(sight.angle, GATE_WALK_SLICE, /*release=*/0ms, Pokemon::StoryPace::WALK);
        walked += GATE_WALK_SLICE;
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
