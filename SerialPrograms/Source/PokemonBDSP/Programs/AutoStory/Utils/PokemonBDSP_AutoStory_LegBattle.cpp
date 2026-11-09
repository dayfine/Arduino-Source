/*  BDSP AutoStory - Leg Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "PokemonBDSP_AutoStory_LegBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::InterruptKind;
using Pokemon::ScreenEvent;
using Pokemon::TilePos;


const LegBattleTiming& leg_battle_timing(){
    static const LegBattleTiming TIMING{
        .lags = {
            {ScreenEvent::FADE_WHITE, Milliseconds(500)},
            {ScreenEvent::BATTLE_STARTED, Milliseconds(4500)},
        },
    };
    return TIMING;
}

namespace{

//  The tile of the leg (from 1) that a step begun at `step` reached: the
//  taps begin `tap_time` apart from `started`. At most `tiles`.
size_t tile_reached(WallClock step, WallClock started, Milliseconds tap_time, size_t tiles){
    if (step < started || tap_time.count() <= 0){
        return 0;
    }
    const auto since = std::chrono::duration_cast<Milliseconds>(step - started);
    return std::min(tiles, (size_t)(since / tap_time) + 1);
}

//  Why a battle on the leg was not placed.
std::string not_placed(const WalkResult& walk, const LegBattlePlace& place){
    if (walk.wild_battles != 1){
        return std::format("{} wild battles", walk.wild_battles);
    }
    switch (place.verdict.kind){
    case InterruptKind::UNSETTLED:
        return "a fade too recent to tell a battle from a warp";
    case InterruptKind::WILD_ENCOUNTER:
        return "no timed event";
    case InterruptKind::NONE:
        return "no events";
    default:
        return std::string("not a wild battle");
    }
}

} // namespace

LegBattlePlace place_leg_battle(const WalkResult& walk, size_t tiles, const LegBattleTiming& timing){
    LegBattlePlace out;
    const WallClock end = walk.events.last_frame().value_or(walk.started);
    out.verdict = Pokemon::classify_interrupt(walk.events, {walk.started, end}, timing.interrupts);
    if (walk.wild_battles != 1 || out.verdict.kind != InterruptKind::WILD_ENCOUNTER){
        return out;
    }
    const std::optional<WallClock> step = Pokemon::interrupt_step_time(out.verdict, timing.lags);
    if (!step){
        return out;
    }
    const size_t reached = tile_reached(*step, walk.started, timing.tap_time, tiles);
    const size_t at = std::min(tiles, reached + walk.moved_after_battle);
    out.moved = TileSpan{at > timing.slack ? at - timing.slack : 0, std::min(tiles, at + timing.slack)};
    return out;
}

std::string leg_battle_line(const std::string& label, const WalkResult& walk, const LegBattlePlace& place){
    std::string out = std::format("{}: battle on the leg: {}", label, Pokemon::describe(place.verdict, walk.started));
    if (place.moved){
        return out + std::format("; {} to {} tiles in.", place.moved->min, place.moved->max);
    }
    return out + "; not placed (" + not_placed(walk, place) + "): any tile of the leg.";
}

std::vector<TilePos> tiles_moved(
    const StoryWorld& world, const std::vector<TilePos>& from,
    Direction direction, TileSpan span
){
    std::vector<TilePos> out;
    for (TilePos at : from){
        for (size_t count = 0; count <= span.max; count++){
            const auto next = count < span.max ? step(world, at, direction) : std::nullopt;
            const bool kept = std::find(out.begin(), out.end(), at) != out.end();
            if ((count >= span.min || !next) && !kept){
                out.push_back(at);
            }
            if (!next){
                break;
            }
            at = *next;
        }
    }
    return out;
}

void record_tap_battle(WalkResult& walk, size_t tap, const LegBattleTiming& timing){
    const WallClock step = walk.started + timing.tap_time * tap;
    WallClock last = walk.events.last_frame().value_or(walk.started);
    std::vector<Pokemon::EventLag> lags = timing.lags;
    std::sort(lags.begin(), lags.end(), [](const Pokemon::EventLag& x, const Pokemon::EventLag& y){
        return x.lag < y.lag;
    });
    for (const Pokemon::EventLag& lag : lags){
        last = std::max(last, step + lag.lag);
        walk.events.record({lag.event}, last);
    }
    walk.events.record({}, last + timing.escape_time);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
