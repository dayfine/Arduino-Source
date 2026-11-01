/*  BDSP AutoStory - Route Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include <format>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Navigation/Pokemon_RouteLog.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"
#include "PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_PokemonCenter.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


namespace{

bool across(Direction direction){
    return direction == Direction::EAST || direction == Direction::WEST;
}

Milliseconds ms_per_tile(Direction direction){
    return across(direction) ? ROUTE_MS_PER_TILE_ACROSS : ROUTE_MS_PER_TILE_UP_DOWN;
}

//  The left stick for a direction. Row 0 is the top of a map, and the stick's
//  y is positive upward.
void stick_for(Direction direction, double& x, double& y){
    x = 0;
    y = 0;
    switch (direction){
    case Direction::NORTH:  y = 1; break;
    case Direction::EAST:   x = 1; break;
    case Direction::SOUTH:  y = -1; break;
    case Direction::WEST:   x = -1; break;
    }
}

//  World tiles as tiles of the named maps, for the shared route log
//  (Pokemon_RouteLog.h); a tile no map holds is a "world" tile.
std::vector<RouteTile> route_tiles(const MapSet& maps, const std::vector<TilePos>& tiles){
    std::vector<RouteTile> out;
    for (const TilePos& pos : tiles){
        const std::optional<MapTile> local = maps.to_local(pos);
        out.push_back(local ? RouteTile{local->map, local->pos} : RouteTile{"world", pos});
    }
    return out;
}

std::string describe(const MapSet& maps, const std::vector<TilePos>& tiles){
    return Pokemon::describe(route_tiles(maps, tiles));
}

RouteLogLeg route_log_leg(const TileLeg& leg){
    return {.action = RouteLegAction::WALK, .direction = leg.direction, .tiles = leg.tiles,
        .end = leg.ends_at_wall ? RouteLegEnd::WALL : RouteLegEnd::OPEN};
}

PlannedRouteLeg planned_route_leg(const MapSet& maps, const PlannedLeg& leg){
    return {.leg = route_log_leg(leg.leg), .after = route_tiles(maps, leg.after)};
}

void log_plan(VideoStream& stream, const MapSet& maps, const std::string& label, const std::vector<TilePos>& from, const std::vector<PlannedLeg>& legs){
    std::vector<PlannedRouteLeg> logged;
    for (const PlannedLeg& leg : legs){
        logged.push_back(planned_route_leg(maps, leg));
    }
    stream.log(ROUTE_LOG_TAG + route_plan_line(label, route_tiles(maps, from), logged));
}

//  Logs a leg that ended on its planned tiles, and how that is known: an
//  open leg counts its taps through a wild battle, so its end is assumed; a
//  leg to a wall is walked again to its wall.
void log_leg_done(VideoStream& stream, const MapSet& maps, const std::string& label, const PlannedLeg& planned, size_t wild_battles){
    const bool assumed = !planned.leg.ends_at_wall && wild_battles > 0;
    stream.log(ROUTE_LOG_TAG + route_leg_end_line(label, route_log_leg(planned.leg), route_tiles(maps, planned.after),
        assumed ? EndBasis::ASSUMED : EndBasis::WALKED, assumed ? std::to_string(wild_battles) + " wild battle(s) on the way" : ""));
}

//  The tiles the player passes when walking `tiles` tiles from `from`, the
//  first one after `from` first. Stops early where the player is stopped.
std::vector<TilePos> leg_path(const TileGrid& grid, TilePos from, Direction direction, size_t tiles){
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

//  Holding toward a wall with people or Pokemon moving beside us never reads
//  as blocked (8 to 16 per 400 ms in the Oreburgh Mine, s03d1), but walking
//  scrolls the view far more (35 to 90). True if holding toward (x, y) for
//  1.6 s changes the screen less than walking would: we stand at a wall.
bool holding_against_wall(VideoStream& stream, ProControllerContext& context, double x, double y, const std::string& label){
    constexpr double WALKING_CHANGE = 25.0;
    context.wait_for_all_requests();
    VideoSnapshot before = stream.video().snapshot();
    double most = 0;
    for (size_t chunk = 0; chunk < 4 && before; chunk++){
        pbf_move_left_joystick(context, {x, y}, 400ms, 0ms);
        context.wait_for_all_requests();
        VideoSnapshot after = stream.video().snapshot();
        if (!after){
            return false;
        }
        most = std::max(most, walk_screen_change(*before.frame, *after.frame));
        before = std::move(after);
    }
    stream.log("[Walk] " + label + ": most screen change holding on " + std::to_string(most) +
        (most < WALKING_CHANGE ? ", less than walking: at the wall." : ", as much as walking: still moving."));
    return before && most < WALKING_CHANGE;
}

//  The map all of `tiles` are on, or empty if they are on several or none.
std::string map_of(const MapSet& maps, const std::vector<TilePos>& tiles){
    std::string name;
    for (const TilePos& pos : tiles){
        const std::optional<MapTile> tile = maps.to_local(pos);
        if (!tile || (!name.empty() && tile->map != name)){
            return "";
        }
        name = tile->map;
    }
    return name;
}

}


bool route_wall_readable(const TileGrid& grid, const std::vector<TilePos>& after){
    return std::none_of(after.begin(), after.end(), [&](const TilePos& pos){
        return grid.at(pos) == TileType::TALL_GRASS;
    });
}

void route_status(VideoStream& stream, const std::string& text){
    stream.log("[Route] " + text, COLOR_BLUE);
    add_wrapped_log(stream.overlay(), text, COLOR_WHITE);
}

Milliseconds route_open_hold(Direction direction, size_t tiles){
    const Milliseconds step = ms_per_tile(direction);
    return step * tiles - step / 2;
}

void route_turn(VideoStream& stream, ProControllerContext& context, Direction direction){
    double x, y;
    stick_for(direction, x, y);
    stream.log(std::string("[Route] Turning ") + direction_name(direction) + ".");
    pbf_move_left_joystick(context, {x, y}, ROUTE_TURN_TAP, 300ms);
    context.wait_for_all_requests();
}

std::optional<Direction> talk_to_adjacent_npc(
    VideoStream& stream, ProControllerContext& context,
    const std::vector<Direction>& directions,
    const std::string& label, Milliseconds wait
){
    for (Direction direction : directions){
        route_turn(stream, context, direction);
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        context.wait_for_all_requests();
        ShortDialogWatcher dialog(COLOR_CYAN);
        if (wait_until(stream, context, wait, {{dialog}}) == 0){
            stream.log("[Route] " + label + ": answered from the " + direction_name(direction) + ".");
            return direction;
        }
    }
    stream.log("[Route] " + label + ": nobody answered next to us.");
    return std::nullopt;
}

bool talk_with_nudges(
    VideoStream& stream, ProControllerContext& context,
    Direction facing, const std::string& label, Milliseconds wait,
    const std::function<bool(const ImageViewRGB32& before, const ImageViewRGB32& after)>& accept
){
    const bool vertical = facing == Direction::NORTH || facing == Direction::SOUTH;
    const Direction minus = vertical ? Direction::WEST : Direction::NORTH;
    const Direction plus = vertical ? Direction::EAST : Direction::SOUTH;
    int at = 0;
    for (int offset : {0, -1, 1, -2, 2}){
        const Direction side = offset < at ? minus : plus;
        for (; at != offset; at += offset < at ? -1 : 1){
            double x, y;
            stick_for(side, x, y);
            pbf_move_left_joystick(context, {x, y}, ROUTE_NUDGE_TAP, 300ms);
        }
        if (offset != 0){
            route_turn(stream, context, facing);
            //  A tap in a cave or in grass can start a wild battle, after
            //  which we are not where the nudges think: let the caller line up again.
            if (run_from_late_encounter(stream, context, label + ": nudge")){
                return false;
            }
        }
        context.wait_for_all_requests();
        const VideoSnapshot before = stream.video().snapshot();
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        context.wait_for_all_requests();
        ShortDialogWatcher dialog(COLOR_CYAN);
        if (wait_until(stream, context, wait, {{dialog}}) == 0){
            const VideoSnapshot after = stream.video().snapshot();
            if (!accept || (before && after && accept(*before.frame, *after.frame))){
                stream.log("[Route] " + label + ": answered " + std::to_string(offset) + " nudge(s) from where we stood.");
                return true;
            }
            stream.log("[Route] " + label + ": something else answered " + std::to_string(offset) + " nudge(s) from where we stood; closing it.");
            for (size_t press = 0; press < 3; press++){
                pbf_press_button(context, BUTTON_B, 80ms, 700ms);
            }
            context.wait_for_all_requests();
        }
    }
    stream.log("[Route] " + label + ": nobody answered within two nudges either side.");
    return false;
}

std::vector<TilePos> walk_route(
    VideoStream& stream, ProControllerContext& context,
    const MapSet& maps,
    std::vector<TilePos> from,
    const std::function<bool(const TilePos&)>& goal,
    std::vector<TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label,
    const std::vector<TilePos>& avoid,
    const RobustPlanCosts& costs
){
    //  Our own copy, so things found in the way can be added to it. Tiles to
    //  avoid are ground the game lets us walk on (a trainer's line of sight),
    //  so they are off the map, not walls: a leg must not end "at" one, or
    //  the walk would run on through it.
    TileGrid grid = maps.world();
    for (const TilePos& pos : avoid){
        if (std::find(from.begin(), from.end(), pos) == from.end()){
            grid.set(pos, TileType::UNKNOWN);
        }
    }
    //  Which way the player faces, once a leg has set it.
    std::optional<Direction> facing;
    //  Every banner read on the way, to check the map we arrive on against.
    const std::string map_start = map_of(maps, from);
    std::vector<std::string> all_banners;
    for (size_t plans = 0; plans < 8; plans++){
        std::optional<std::vector<PlannedLeg>> legs = plan_robust_route(grid, from, goal, costs);
        if (!legs){
            //  From a few possible tiles the exact-open-leg costs found no
            //  plan where the default ones did (r240a, three tiles on Route
            //  204's row 27); the default plan is longer but sound.
            stream.log("[Route] " + label + ": no plan with the route costs from " + describe(maps, from) +
                "; trying the default costs.", COLOR_ORANGE);
            legs = plan_robust_route(grid, from, goal, RobustPlanCosts());
        }
        if (!legs){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                label + ": no plan from " + describe(maps, from) + ".", stream);
        }
        route_status(stream, label + ": " + std::to_string(legs->size()) + " legs planned");
        log_plan(stream, maps, label, from, *legs);

        bool replanned = false;
        size_t leg_number = 0;
        for (const PlannedLeg& planned : *legs){
            const TileLeg& leg = planned.leg;
            leg_number++;
            route_status(stream, route_leg_line(leg_number, legs->size(), planned_route_leg(maps, planned)));
            double x, y;
            stick_for(leg.direction, x, y);
            const std::string leg_label = label + ": " + to_string(leg);

            //  Note the location banners the walk passes, to check map changes.
            //  A wild battle or a second walk to the wall may follow the
            //  banner: any banner seen during the leg counts.
            const std::string map_before = map_of(maps, from);
            const std::string map_after = map_of(maps, planned.after);
            std::vector<std::string> banners;
            auto banner_ok = [&]{
                return std::any_of(banners.begin(), banners.end(), [&](const std::string& read){
                    return location_banner_matches(read, map_after);
                });
            };
            auto watch_banner = [&](const ImageViewRGB32& screen){
                if (banners.empty() || (!map_after.empty() && !banner_ok() && banners.size() < 8)){
                    const std::string read = read_location_banner(screen);
                    if (!read.empty() && (banners.empty() || banners.back() != read)){
                        banners.push_back(read);
                    }
                }
                return false;
            };

            WalkResult result;
            //  Tiles the first tap of an open leg moved (see below).
            size_t turn_moved = 0;
            if (leg.ends_at_wall){
                //  Run (B held) on the D-pad: a leg to a wall ends at the wall
                //  however fast we get there, and the D-pad keeps to the row.
                const Milliseconds per_tile = ms_per_tile(leg.direction) / ROUTE_RUN_SPEEDUP;
                //  Long enough to reach the wall from the farthest tile, with room
                //  for a slow start; past the wall the player stands still.
                const Milliseconds limit = ROUTE_START_DELAY + per_tile * leg.tiles * 5 / 4 + 1000ms;
                result = walk_until_dpad(stream, context, x, y, limit, leg_label, watch_banner, true);
                //  A wild battle may end the walk early. Walking to a wall again
                //  ends in the same place, so finish the leg.
                const bool interrupted = result.wild_battles > 0;
                for (size_t again = 0; again < 3 && result.wild_battles > 0 &&
                    (result.stop == WalkStop::LIMIT || result.stop == WalkStop::BLOCKED); again++
                ){
                    stream.log("[Route] " + leg_label + ": a wild battle on the way; walking to the wall again.");
                    result = walk_until_dpad(stream, context, x, y, limit, leg_label, watch_banner, true);
                }
                //  A wall well before the nearest one the map has: something the
                //  map does not know is in the way. Work out where we stopped from
                //  the time walked, put it on our map and plan again from there.
                size_t nearest = SIZE_MAX;
                for (const TilePos& pos : from){
                    nearest = std::min(nearest, walk_to_wall(grid, pos, leg.direction).tiles);
                }
                auto tiles_moved = [&](const WalkResult& walk){
                    const Milliseconds moving = walk.walked - ROUTE_START_DELAY -
                        (walk.stop == WalkStop::BLOCKED ? 800ms : 0ms);
                    return moving.count() <= 0 ? (size_t)0 : (size_t)(moving / per_tile);
                };
                size_t walked_tiles = tiles_moved(result);
                //  A stick walk can catch a corner when we stand part of a tile
                //  off centre, where a D-pad tap, which moves whole tiles, gets
                //  through: on Route 203's stairs the walk west stopped nine
                //  times in a row and two taps crossed (r225a, 2026-10-31).
                //  Tap twice, then walk on to the wall.
                if (!interrupted && result.stop == WalkStop::BLOCKED && walked_tiles + 2 < nearest){
                    const DpadPosition dpad =
                        leg.direction == Direction::NORTH ? DPAD_UP :
                        leg.direction == Direction::SOUTH ? DPAD_DOWN :
                        leg.direction == Direction::WEST ? DPAD_LEFT : DPAD_RIGHT;
                    stream.log("[Route] " + leg_label + ": stopped after about " + std::to_string(walked_tiles) +
                        " tiles, short of the wall; two D-pad taps, then on to the wall.", COLOR_ORANGE);
                    for (size_t tap = 0; tap < 2; tap++){
                        pbf_press_dpad(context, dpad, 160ms, 640ms);
                    }
                    context.wait_for_all_requests();
                    result = walk_until_dpad(stream, context, x, y, limit, leg_label, watch_banner, true);
                    walked_tiles += tiles_moved(result);
                }
                //  Someone walking about may stand in the way for a moment: wait
                //  and walk to the wall again before calling it an obstacle.
                for (size_t again = 0; !interrupted && again < 2 &&
                    result.stop == WalkStop::BLOCKED && walked_tiles + 2 < nearest; again++
                ){
                    stream.log("[Route] " + leg_label + ": stopped after about " + std::to_string(walked_tiles) +
                        " tiles, short of the wall; waiting for the way to clear.", COLOR_ORANGE);
                    pbf_wait(context, 2500ms);
                    context.wait_for_all_requests();
                    result = walk_until_dpad(stream, context, x, y, limit, leg_label, watch_banner, true);
                    walked_tiles += tiles_moved(result);
                }
                //  Any stop short of the wall's distance: someone who stays put
                //  is talked to, and the leg is planned again from where we are.
                //  Within the time count's slack this may be the wall itself,
                //  where A does nothing and the new plan walks no further (r239a:
                //  a wanderer 3 tiles short passed as "about 8 of 10", and every
                //  later leg was off by those tiles).
                if (!interrupted && result.stop == WalkStop::BLOCKED && walked_tiles < nearest){
                    //  Someone who stays put: talk to them. A trainer we walked up
                    //  to from the side starts the battle this way.
                    pbf_press_button(context, BUTTON_A, 80ms, 200ms);
                    {
                        ShortDialogWatcher dialog(COLOR_YELLOW);
                        if (wait_until(stream, context, 2000ms, {{dialog}}) == 0){
                            route_status(stream, leg_label + ": someone is in the way; talking to them.");
                            fight("someone in the way");
                        }
                    }
                    std::vector<TilePos> stopped;
                    for (const TilePos& pos : from){
                        const std::vector<TilePos> path = leg_path(grid, pos, leg.direction, SIZE_MAX);
                        for (size_t k = walked_tiles == 0 ? 0 : walked_tiles - 1; k <= walked_tiles + 1 && k <= path.size(); k++){
                            stopped.push_back(k == 0 ? pos : path[k - 1]);
                        }
                    }
                    std::sort(stopped.begin(), stopped.end(), [](const TilePos& a, const TilePos& b){
                        return a.y != b.y ? a.y < b.y : a.x < b.x;
                    });
                    stopped.erase(std::unique(stopped.begin(), stopped.end()), stopped.end());
                    if (stopped.empty()){
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            leg_label + ": stopped early, and no tile fits.", stream);
                    }
                    if (stopped.size() == 1){
                        const TilePos blocker = stopped[0].next(leg.direction);
                        grid.set(blocker, TileType::BLOCKED);
                        stream.log("[Route] " + leg_label + ": something the map lacks is at " +
                            maps.describe(blocker) + "; marked it.", COLOR_ORANGE);
                    }
                    stream.log(ROUTE_LOG_TAG + route_replan_line(label, route_log_leg(leg),
                        "stopped early after about " + std::to_string(walked_tiles) + " tiles", route_tiles(maps, stopped)), COLOR_ORANGE);
                    facing = leg.direction;
                    from = stopped;
                    replanned = true;
                    break;
                }
                //  A leg to the wall that ran out of time is not at its wall:
                //  the open leg before it stopped short, and it ran along
                //  another row (r127a went west along Sandgem's row 13 for row
                //  14, and the next leg north walked into the lab). Out of tall
                //  grass a stop reads, so walk on: a player still moving is
                //  not where the plan has him.
                if (!interrupted && result.stop == WalkStop::LIMIT && route_wall_readable(grid, planned.after)){
                    const WalkResult check = walk_until_dpad(stream, context, x, y, 2000ms, leg_label + ": at the wall?", nullptr, true);
                    if (check.stop == WalkStop::DIALOG){
                        //  A trainer who saw us froze the walk (the "!") until
                        //  its time ran out, then walked over (r241a: Aroma Lady
                        //  Taylor's line crosses Route 204's corridor). Battle,
                        //  then plan again from where a sight line crosses the leg.
                        route_status(stream, leg_label + ": a trainer stopped us on the way; battling.");
                        fight("a trainer on the way");
                        //  Where they saw us from is not reliable (BDSP's trainers
                        //  stand a few tiles from Platinum's, r242a), so do not
                        //  guess the tile: a leg to a wall ends at its wall however
                        //  far it got. Walk on to it.
                        stream.log("[Route] " + leg_label + ": on to the wall after the battle.", COLOR_ORANGE);
                        result = walk_until_dpad(stream, context, x, y, limit, leg_label + ": on to the wall", watch_banner, true);
                    }
                    if (check.stop != WalkStop::BLOCKED &&
                        !(check.stop == WalkStop::LIMIT && holding_against_wall(stream, context, x, y, leg_label + ": at the wall?"))
                    ){
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            leg_label + ": ran out of time short of the wall, and walking on gets " +
                            walk_stop_name(check.stop) + ": not where the plan has us.", stream);
                    }
                }
            }else{
                //  Whole tiles on the D-pad, a tap each: a tap moves exactly
                //  one tile, while a stick hold timed for three moved one in
                //  the Ravaged Path (r235b) and the belief drifted four rows.
                //  The first tap of a leg was meant to turn, but a 160 ms
                //  D-pad tap turns and steps whichever way the player faced
                //  (r248a: all seven did; r246a: "south 1" out of the cave
                //  went two tiles, onto the Youngster's row), so the tap is
                //  measured like any other and a step counts for the leg.
                if (facing != leg.direction){
                    const DpadPosition dpad = x < 0 ? DPAD_LEFT : x > 0 ? DPAD_RIGHT : y > 0 ? DPAD_UP : DPAD_DOWN;
                    context.wait_for_all_requests();
                    VideoSnapshot before = stream.video().snapshot();
                    pbf_press_dpad(context, dpad, 160ms, 700ms);
                    context.wait_for_all_requests();
                    VideoSnapshot after = stream.video().snapshot();
                    if (before && after && walk_screen_change(*before.frame, *after.frame) > WALK_TAP_BLOCKED_CHANGE){
                        turn_moved = 1;
                        stream.log("[Route] " + leg_label + ": the turn tap moved a tile (already facing that way); it counts.", COLOR_ORANGE);
                    }
                }
                if (leg.tiles > turn_moved){
                    result = walk_tiles_dpad(stream, context, x, y, leg.tiles - turn_moved, leg_label);
                }else{
                    result = WalkResult{WalkStop::LIMIT, 0ms, 0};
                }
                //  A wild battle starts once the step onto the grass is done,
                //  so the taps still count; the walk went on after it.
                if (result.wild_battles > 0){
                    stream.log("[Route] " + leg_label + ": " + std::to_string(result.wild_battles) +
                        " wild battle(s) on the way; the tap count holds.", COLOR_ORANGE);
                }
                //  Taps that moved nothing: someone stands in the way (the
                //  Youngster below the Ravaged Path wanders across the row
                //  under the cliff, r239a, r240a). Two still taps end the walk,
                //  so the tiles moved are the taps less those two. Wait for
                //  them to move on and tap the rest; someone who stays is
                //  talked to, put on the map, and the leg planned again from
                //  the tile we stand on, which the taps give exactly.
                auto taps_moved = [](const WalkResult& walk){ return walk.moved_chunks; };
                size_t moved = std::min(leg.tiles, taps_moved(result) + turn_moved);
                //  One wait: a wanderer in the way steps off in a few seconds;
                //  anything still there after that is talked to (r246a lost
                //  20 s per block on three waits).
                for (size_t again = 0; again < 1 && result.stop == WalkStop::BLOCKED && moved < leg.tiles; again++){
                    stream.log("[Route] " + leg_label + ": blocked after " + std::to_string(moved) + " of " +
                        std::to_string(leg.tiles) + " tiles; waiting for the way to clear.", COLOR_ORANGE);
                    pbf_wait(context, 2500ms);
                    context.wait_for_all_requests();
                    const WalkResult rest = walk_tiles_dpad(stream, context, x, y, leg.tiles - moved, leg_label + ": the rest");
                    moved = std::min(leg.tiles, moved + taps_moved(rest));
                    result.stop = rest.stop;
                    result.walked += rest.walked;
                    result.wild_battles += rest.wild_battles;
                }
                if (result.stop == WalkStop::BLOCKED && moved < leg.tiles){
                    pbf_press_button(context, BUTTON_A, 80ms, 200ms);
                    {
                        ShortDialogWatcher dialog(COLOR_YELLOW);
                        if (wait_until(stream, context, 2000ms, {{dialog}}) == 0){
                            route_status(stream, leg_label + ": someone is in the way; talking to them.");
                            fight("someone in the way");
                        }
                    }
                    std::vector<TilePos> stopped;
                    for (const TilePos& pos : from){
                        const std::vector<TilePos> path = leg_path(grid, pos, leg.direction, moved);
                        stopped.push_back(moved == 0 || path.empty() ? pos : path[std::min(moved, path.size()) - 1]);
                    }
                    std::sort(stopped.begin(), stopped.end(), [](const TilePos& a, const TilePos& b){
                        return a.y != b.y ? a.y < b.y : a.x < b.x;
                    });
                    stopped.erase(std::unique(stopped.begin(), stopped.end()), stopped.end());
                    if (stopped.size() == 1){
                        const TilePos blocker = stopped[0].next(leg.direction);
                        grid.set(blocker, TileType::BLOCKED);
                        stream.log("[Route] " + leg_label + ": something the map lacks is at " +
                            maps.describe(blocker) + "; marked it.", COLOR_ORANGE);
                    }
                    stream.log(ROUTE_LOG_TAG + route_replan_line(label, route_log_leg(leg),
                        "blocked after " + std::to_string(moved) + " of " + std::to_string(leg.tiles) + " tiles", route_tiles(maps, stopped)), COLOR_ORANGE);
                    facing = leg.direction;
                    from = stopped;
                    replanned = true;
                    break;
                }
            }
            facing = leg.direction;
            {
                context.wait_for_all_requests();
                if (VideoSnapshot screen = stream.video().snapshot()){
                    watch_banner(*screen.frame);
                }
            }
            std::string banner_text;
            for (const std::string& read : banners){
                banner_text += (banner_text.empty() ? "\"" : ", \"") + read + "\"";
            }
            all_banners.insert(all_banners.end(), banners.begin(), banners.end());
            //  Entering another map shows its name; a missing or wrong name means
            //  the walk did not end where the plan says. From tiles on several
            //  maps (a leg not confirmed) the banner may already have shown.
            if (!map_before.empty() && !map_after.empty() && map_after != map_before && !banner_ok()){
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    leg_label + ": expected to enter " + map_after + ", but the location banner read " +
                    (banner_text.empty() ? "nothing" : banner_text) + ".", stream);
            }
            if (!banner_text.empty()){
                stream.log("[Route] " + leg_label + ": location banner " + banner_text + ".");
            }
            //  Where each leg ended, to compare with the plan after a run.
            if (VideoSnapshot screen = stream.video().snapshot()){
                const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
                Filesystem::create_directories(Filesystem::Path(folder));
                screen.frame->save(folder + now_to_filestring() + "-" + normalize_location_name(leg_label) + ".png");
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
                log_leg_done(stream, maps, label, planned, result.wild_battles);
                continue;
            }

            if (watching.empty()){
                //  A trainer the map does not have. Fight, then plan again from
                //  anywhere the leg could have stopped.
                std::vector<TilePos> anywhere = from;
                for (const std::vector<TilePos>& path : paths){
                    anywhere.insert(anywhere.end(), path.begin(), path.end());
                }
                std::sort(anywhere.begin(), anywhere.end(), [](const TilePos& a, const TilePos& b){
                    return a.y != b.y ? a.y < b.y : a.x < b.x;
                });
                anywhere.erase(std::unique(anywhere.begin(), anywhere.end()), anywhere.end());
                route_status(stream, label + ": a trainer the map lacks stopped us; fighting.");
                fight("unknown trainer");
                if (leg.ends_at_wall){
                    const Milliseconds limit = ROUTE_START_DELAY +
                        ms_per_tile(leg.direction) / ROUTE_RUN_SPEEDUP * leg.tiles * 5 / 4 + 1000ms;
                    stream.log("[Route] " + leg_label + ": on to the wall after the battle.", COLOR_ORANGE);
                    walk_until_dpad(stream, context, x, y, limit, leg_label + ": on to the wall", watch_banner, true);
                    from = planned.after;
                    log_leg_done(stream, maps, label, planned, 0);
                    continue;
                }
                from = anywhere;
                replanned = true;
                break;
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
            stream.log(ROUTE_LOG_TAG + route_trainer_line(label, trainer.name, route_tiles(maps, stopped)));
            route_status(stream, "Fighting " + trainer.name + ".");
            fight(trainer.name);
            trainers.erase(trainers.begin() + watching[0]);
            //  Their sight line only says roughly where: BDSP's trainers stand a
            //  few tiles from Platinum's (r242a). A leg to a wall ends at its
            //  wall however far it got: walk on to it and carry on. An open leg
            //  counts its taps: the step that brought us into sight was complete.
            if (leg.ends_at_wall){
                const Milliseconds limit = ROUTE_START_DELAY +
                    ms_per_tile(leg.direction) / ROUTE_RUN_SPEEDUP * leg.tiles * 5 / 4 + 1000ms;
                stream.log("[Route] " + leg_label + ": on to the wall after the battle.", COLOR_ORANGE);
                walk_until_dpad(stream, context, x, y, limit, leg_label + ": on to the wall", watch_banner, true);
                from = planned.after;
                log_leg_done(stream, maps, label, planned, 0);
                continue;
            }
            //  Only the taps that moved count: the trainer freezes the player
            //  on the tile they saw, and the taps pressed while they walk over
            //  move nobody (r248a: "4 taps" were 2 steps; the belief of two
            //  tiles then cost a 12-tile run to a wall and 5 back).
            const size_t taps = std::min(leg.tiles, result.moved_chunks + turn_moved);
            std::vector<TilePos> by_taps;
            for (const TilePos& pos : from){
                const std::vector<TilePos> path = leg_path(grid, pos, leg.direction, leg.tiles);
                const size_t k = std::min(taps, path.size());
                by_taps.push_back(k == 0 ? pos : path[k - 1]);
            }
            std::sort(by_taps.begin(), by_taps.end(), [](const TilePos& a, const TilePos& b){
                return a.y != b.y ? a.y < b.y : a.x < b.x;
            });
            by_taps.erase(std::unique(by_taps.begin(), by_taps.end()), by_taps.end());
            stream.log(ROUTE_LOG_TAG + route_replan_line(label, route_log_leg(leg),
                std::to_string(taps) + " taps before the battle", route_tiles(maps, by_taps)), COLOR_ORANGE);
            from = by_taps;
            replanned = true;
            break;
        }
        if (!replanned){
            //  Legs whose ends span two maps skip the per-leg check; the walk
            //  as a whole must still have seen the banner of the map it ends on.
            const std::string map_end = map_of(maps, from);
            const bool changed_map = !map_start.empty() && !map_end.empty() && map_end != map_start;
            if (changed_map && std::none_of(all_banners.begin(), all_banners.end(), [&](const std::string& read){
                return location_banner_matches(read, map_end);
            })){
                std::string seen;
                for (const std::string& read : all_banners){
                    seen += (seen.empty() ? "\"" : ", \"") + read + "\"";
                }
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    label + ": arrived at " + describe(maps, from) + " on the map, but " + map_end +
                    "'s location banner never showed (read " + (seen.empty() ? "nothing" : seen) + ").", stream);
            }
            stream.log(ROUTE_LOG_TAG + route_arrived_line(label, route_tiles(maps, from)));
            return from;
        }
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": too many plans.", stream);
}



std::vector<TilePos> walk_waypoints(
    VideoStream& stream, ProControllerContext& context,
    const MapSet& maps,
    std::vector<TilePos> from,
    const std::vector<TilePos>& waypoints,
    std::vector<TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label,
    const RobustPlanCosts& costs
){
    for (size_t i = 0; i < waypoints.size(); i++){
        const TilePos goal = waypoints[i];
        from = walk_route(stream, context, maps, from,
            [&](const TilePos& pos){ return pos == goal; },
            trainers, fight,
            label + " (" + std::to_string(i + 1) + "/" + std::to_string(waypoints.size()) + ", to " + maps.describe(goal) + ")",
            {}, costs);
    }
    return from;
}

void talk_to_npc(
    VideoStream& stream, ProControllerContext& context,
    const MapSet& maps,
    std::vector<TilePos>& at,
    const TilePos& npc,
    const std::vector<Direction>& sides,
    std::vector<TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label
){
    for (Direction side : sides){
        const TilePos stand = npc.next(side);
        if (!maps.world().standable(stand)){
            continue;
        }
        at = walk_route(stream, context, maps, at, [&](const TilePos& pos){ return pos == stand; },
            trainers, fight, label);
        //  Face the NPC: the opposite of the side we stand on.
        const Direction face =
            side == Direction::NORTH ? Direction::SOUTH :
            side == Direction::SOUTH ? Direction::NORTH :
            side == Direction::EAST ? Direction::WEST : Direction::EAST;
        route_turn(stream, context, face);
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        context.wait_for_all_requests();
        ShortDialogWatcher dialog(COLOR_CYAN);
        if (wait_until(stream, context, 2000ms, {{dialog}}) == 0){
            route_status(stream, label + ": talking from " + maps.describe(stand) + ".");
            return;
        }
        route_status(stream, label + ": nobody answered from " + maps.describe(stand) + "; trying another side.");
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": no side of " + maps.describe(npc) + " started a dialogue.", stream);
}

void finish_dialogue(
    VideoStream& stream, ProControllerContext& context,
    Button button, const std::string& label, Milliseconds timeout
){
    //  No line counts: talks differ in length (r115a: 8 A left the Poke Mart
    //  lady's Potions talk open). The box blinks off between lines and during
    //  "obtained" jingles, so only a stable absence ends the talk.
    const WallClock none = WallClock::max();
    ShortDialogDetector dialog;
    OverworldDetector overworld;
    const WallClock deadline = current_time() + timeout;
    WallClock quiet_since = none;
    WallClock unknown_since = none;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        const WallClock now = current_time();
        const bool talking = frame && dialog.detect(frame);
        if (frame && !talking && overworld.detect(frame)){
            unknown_since = none;
            if (quiet_since == none){
                quiet_since = now;
            }else if (now - quiet_since >= 1500ms){
                return;
            }
            context.wait_for(200ms);
            continue;
        }
        quiet_since = none;
        //  Press only on a box (an A on the overworld talks again), or on a
        //  screen that is neither for 3 s.
        if (talking || (unknown_since != none && now - unknown_since >= 3000ms)){
            unknown_since = none;
            pbf_press_button(context, button, 80ms, 1200ms);
            continue;
        }
        if (unknown_since == none){
            unknown_since = now;
        }
        context.wait_for(200ms);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": the dialogue did not end within " +
        std::to_string(std::chrono::duration_cast<std::chrono::seconds>(timeout).count()) + " s.", stream);
}

void enter_door_north(VideoStream& stream, ProControllerContext& context, const std::string& label){
    //  Sideways steps to try after a blocked walk: east, then back west and
    //  one more.
    const Direction steps[] = {Direction::EAST, Direction::WEST, Direction::WEST};
    for (size_t attempt = 0; attempt <= 3; attempt++){
        const WalkResult walk = walk_until(stream, context, 0, 1, 2500ms, label);
        if (walk.stop == WalkStop::TRANSITION){
            context.wait_for(1500ms);
            return;
        }
        if (attempt == 3){
            break;
        }
        route_status(stream, label + ": the door frame stopped us; one tile " +
            (steps[attempt] == Direction::EAST ? "east." : "west."));
        route_turn(stream, context, steps[attempt]);
        const double x = steps[attempt] == Direction::EAST ? 1 : -1;
        pbf_move_left_joystick(context, {x, 0}, route_open_hold(steps[attempt], 1), 200ms);
        context.wait_for_all_requests();
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": could not go through the door.", stream);
}

bool door_fade_look(DoorFadeWatch& watch, double brightness, bool room){
    if (brightness < DOOR_FADE_DARK){
        watch.saw_dark = true;
    }
    return watch.saw_dark && !room && brightness >= DOOR_OUTSIDE_BRIGHT;
}

bool door_walk_stops(DoorFadeWatch& watch, double brightness, bool room){
    //  Both are asked: the fade watch keeps what it has seen.
    const bool faded = door_fade_look(watch, brightness, room);
    return faded || !room;
}

namespace{

//  Three looks half a second apart, none a room: outside, whatever the fade
//  did. A look of a lit Center can miss its black edge (r259 at 800 ms),
//  three in a row standing still do not.
bool outside_for_three_looks(VideoStream& stream, ProControllerContext& context, const std::string& label){
    for (size_t look = 0; look < 3; look++){
        context.wait_for_all_requests();
        VideoSnapshot now = stream.video().snapshot();
        const bool room = !now || indoors(*now.frame);
        stream.log("[Route] " + label + ": standing look " + std::to_string(look + 1) + ": " + (room ? "a room" : "not a room") + ".");
        if (room){
            return false;
        }
        pbf_wait(context, 500ms);
    }
    return true;
}

}

void leave_door_south(VideoStream& stream, ProControllerContext& context, const std::string& label){
    //  A last line of talk may still be up, and it would stop the walk.
    pbf_mash_button(context, BUTTON_B, 2000ms);
    context.wait_for_all_requests();
    //  Stop the walk at the fade or at the first look that is not a room: a
    //  stick held on walks on outside (r124a ten rows south of the Mart's
    //  door, r257 into a house, r261 three rows). Standing looks then confirm.
    DoorFadeWatch watch;
    auto out_of_door = [&](const ImageViewRGB32& screen){
        const double brightness = image_stats(screen).average.sum();
        const bool room = indoors(screen);
        const bool out = door_walk_stops(watch, brightness, room);
        stream.log(std::format("[Route] {}: look: brightness {:.0f}, {}, fade {}.", label, brightness,
            room ? "a room" : "not a room", watch.saw_dark ? "seen" : "not seen"));
        return out;
    };
    bool out = false;
    for (size_t tries = 0; tries < 5 && !out; tries++){
        const WalkResult walk = walk_until(stream, context, 0, -1, 6000ms, label, out_of_door);
        if (walk.stop == WalkStop::DIALOG){
            pbf_mash_button(context, BUTTON_B, 2000ms);
        }
        //  Let the fade-in finish, then look again standing still.
        pbf_wait(context, 1500ms);
        out = walk.stop == WalkStop::TRANSITION || outside_for_three_looks(stream, context, label);
        if (!out){
            stream.log("[Route] " + label + ": still indoors; walking on.");
        }
    }
    if (!out){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": did not get back out.", stream);
    }
    //  Let the fade-in finish.
    pbf_wait(context, 1500ms);
}



}
}
}
