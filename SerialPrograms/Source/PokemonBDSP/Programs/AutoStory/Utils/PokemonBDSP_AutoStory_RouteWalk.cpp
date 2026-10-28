/*  BDSP AutoStory - Route Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


namespace{

bool across(GridDirection direction){
    return direction == GridDirection::EAST || direction == GridDirection::WEST;
}

Milliseconds ms_per_tile(GridDirection direction){
    return across(direction) ? ROUTE_MS_PER_TILE_ACROSS : ROUTE_MS_PER_TILE_UP_DOWN;
}

//  The left stick for a direction. Row 0 is the top of a map, and the stick's
//  y is positive upward.
void stick_for(GridDirection direction, double& x, double& y){
    x = 0;
    y = 0;
    switch (direction){
    case GridDirection::NORTH:  y = 1; break;
    case GridDirection::EAST:   x = 1; break;
    case GridDirection::SOUTH:  y = -1; break;
    case GridDirection::WEST:   x = -1; break;
    }
}

std::string describe(const MapSet& maps, const std::vector<TilePos>& tiles){
    std::string out;
    for (const TilePos& pos : tiles){
        out += (out.empty() ? "" : ", ") + maps.describe(pos);
    }
    return out;
}

//  The tiles the player passes when walking `tiles` tiles from `from`, the
//  first one after `from` first. Stops early where the player is stopped.
std::vector<TilePos> leg_path(const TileGrid& grid, TilePos from, GridDirection direction, size_t tiles){
    std::vector<TilePos> path;
    size_t travelled = 0;
    while (travelled < tiles){
        const std::optional<TilePos> next = grid.step(from, direction);
        if (!next){
            break;
        }
        travelled += std::abs(next->x - from.x) + std::abs(next->y - from.y);
        from = *next;
        path.push_back(from);
    }
    return path;
}

}


Milliseconds route_open_hold(GridDirection direction, size_t tiles){
    const Milliseconds step = ms_per_tile(direction);
    return ROUTE_START_DELAY + step * tiles - step / 2;
}


std::vector<TilePos> walk_route(
    VideoStream& stream, ProControllerContext& context,
    const MapSet& maps,
    std::vector<TilePos> from,
    const std::function<bool(const TilePos&)>& goal,
    std::vector<TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label
){
    const TileGrid& grid = maps.world();
    for (size_t plans = 0; plans < 8; plans++){
        const std::optional<std::vector<PlannedLeg>> legs = plan_robust_route(grid, from, goal);
        if (!legs){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                label + ": no plan from " + describe(maps, from) + ".", stream);
        }
        std::string text = "[Route] " + label + ": plan from " + describe(maps, from) + ":";
        for (const PlannedLeg& leg : *legs){
            text += "\n    " + to_string(leg.leg) + " to " + describe(maps, leg.after);
        }
        stream.log(text);

        bool replanned = false;
        for (const PlannedLeg& planned : *legs){
            const TileLeg& leg = planned.leg;
            double x, y;
            stick_for(leg.direction, x, y);
            const std::string leg_label = label + ": " + to_string(leg);

            WalkResult result;
            if (leg.ends_at_wall){
                //  Long enough to reach the wall from the farthest tile, with room
                //  for a slow start; past the wall the player stands still.
                const Milliseconds limit = ROUTE_START_DELAY + ms_per_tile(leg.direction) * leg.tiles * 5 / 4 + 1000ms;
                result = walk_until(stream, context, x, y, limit, leg_label);
            }else{
                result = walk_for(stream, context, x, y, route_open_hold(leg.direction, leg.tiles), leg_label);
                if (result.wild_battles > 0){
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        leg_label + ": a wild battle cut an open leg short; position unknown.", stream);
                }
            }
            if (result.stop == WalkStop::TRANSITION){
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    leg_label + ": unexpected transition.", stream);
            }

            //  A trainer who sees us freezes the player and walks over; the
            //  dialogue may open after the walk has ended.
            const std::vector<std::vector<TilePos>> paths = [&]{
                std::vector<std::vector<TilePos>> out;
                for (const TilePos& pos : from){
                    out.push_back(leg_path(grid, pos, leg.direction,
                        leg.ends_at_wall ? SIZE_MAX : leg.tiles + 1));
                }
                return out;
            }();
            std::vector<size_t> watching;
            for (size_t t = 0; t < trainers.size(); t++){
                const std::vector<TilePos> sight = sight_tiles(grid, trainers[t]);
                for (const std::vector<TilePos>& path : paths){
                    if (std::any_of(path.begin(), path.end(), [&](const TilePos& pos){
                        return std::find(sight.begin(), sight.end(), pos) != sight.end();
                    })){
                        watching.push_back(t);
                        break;
                    }
                }
            }
            bool talking = result.stop == WalkStop::DIALOG;
            if (!talking){
                ShortDialogWatcher dialog(COLOR_YELLOW);
                const Milliseconds wait = watching.empty() ? 500ms : 4000ms;
                talking = wait_until(stream, context, wait, {{dialog}}) == 0;
            }
            if (!talking){
                from = planned.after;
                continue;
            }

            if (watching.empty()){
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    leg_label + ": a dialogue opened where no known trainer can see.", stream);
            }
            //  Each possible start tile was stopped on the first tile of its path
            //  inside a watching trainer's sight.
            const TrainerSight trainer = trainers[watching[0]];
            std::vector<TilePos> stopped;
            for (const std::vector<TilePos>& path : paths){
                for (const TilePos& pos : path){
                    bool seen = false;
                    for (size_t t : watching){
                        const std::vector<TilePos> sight = sight_tiles(grid, trainers[t]);
                        seen |= std::find(sight.begin(), sight.end(), pos) != sight.end();
                    }
                    if (seen){
                        stopped.push_back(pos);
                        break;
                    }
                }
            }
            if (stopped.empty()){
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    leg_label + ": " + trainer.name + " talked, but no path enters their sight.", stream);
            }
            stream.log("[Route] " + label + ": " + trainer.name + " stopped us at " + describe(maps, stopped) + ".");
            fight(trainer.name);
            trainers.erase(trainers.begin() + watching[0]);
            from = stopped;
            replanned = true;
            break;
        }
        if (!replanned){
            stream.log("[Route] " + label + ": arrived at " + describe(maps, from) + ".");
            return from;
        }
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": too many plans.", stream);
}



}
}
}
