/*  Overworld Late Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_OverworldLateBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


const std::vector<Pokemon::EventLag>& wild_battle_lags(){
    static const std::vector<Pokemon::EventLag> LAGS{
        {ScreenEvent::BATTLE_STARTED, OverworldWalkSteps::LATE_WILD_BATTLE_DELAY},
    };
    return LAGS;
}

int tile_reached(Milliseconds since_hold, Milliseconds tile_time, int tiles){
    if (tile_time.count() <= 0){
        return tiles;
    }
    if (since_hold.count() < 0){
        return 0;
    }
    const int begun = (int)((since_hold + tile_time / 2) / tile_time) + 1;
    return std::clamp(begun, 0, tiles);
}

LateStop place_late_stop(
    const RouteStep& step, Milliseconds tile_time,
    const ScreenEventLog& log, WallClock hold_start, WallClock now
){
    LateStop out;
    out.verdict = Pokemon::classify_interrupt(log, {hold_start, now}, {});
    if (out.verdict.kind == Pokemon::InterruptKind::DIALOG && step.sign_ahead){
        //  The sign past the end: only a hold run on from there reads it.
        out.walked = WalkedTiles{.min = step.tiles, .max = step.tiles};
        return out;
    }
    if (out.verdict.kind != Pokemon::InterruptKind::WILD_ENCOUNTER){
        return out;
    }
    const std::optional<WallClock> pressed = Pokemon::interrupt_step_time(out.verdict, wild_battle_lags());
    if (!pressed){
        return out;
    }
    const Milliseconds since_hold = std::chrono::duration_cast<Milliseconds>(*pressed - hold_start);
    out.walked = walked_near(step, tile_reached(since_hold, tile_time, step.tiles), tile_time);
    return out;
}

std::string late_stop_line(const RouteStep& step, const LateStop& stop, WallClock hold_start){
    std::string out = std::format(
        "Route: seen after the hold of {}: {}",
        to_string(step), Pokemon::describe(stop.verdict, hold_start)
    );
    if (stop.walked){
        return out + std::format("; {} to {} tiles in.", stop.walked->min, stop.walked->max);
    }
    return out + "; not placed.";
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
