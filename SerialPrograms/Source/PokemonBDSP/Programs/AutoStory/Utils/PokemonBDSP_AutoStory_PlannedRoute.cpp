/*  BDSP AutoStory - Planned Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Navigation/Pokemon_RouteLog.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_PlannedRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


namespace{

//  World tiles as tiles of the named maps, for the shared route log
//  (Pokemon_RouteLog.h); a tile no map holds is a "world" tile.
std::vector<RouteTile> route_tiles(const StoryWorld& maps, const std::vector<TilePos>& tiles){
    std::vector<RouteTile> out;
    for (const TilePos& pos : tiles){
        const std::optional<MapTile> local = maps.to_local(pos);
        out.push_back(local ? RouteTile{local->map, local->pos} : RouteTile{"world", pos});
    }
    return out;
}

std::string describe(const StoryWorld& maps, const std::vector<TilePos>& tiles){
    return Pokemon::describe(route_tiles(maps, tiles));
}

RouteLogLeg route_log_leg(const TileLeg& leg){
    return {.action = RouteLegAction::WALK, .direction = leg.direction, .tiles = leg.tiles,
        .end = leg.ends_at_wall ? RouteLegEnd::WALL : RouteLegEnd::OPEN};
}

PlannedRouteLeg planned_route_leg(const StoryWorld& maps, const PlannedLeg& leg){
    return {.leg = route_log_leg(leg.leg), .after = route_tiles(maps, leg.after)};
}

void log_plan(VideoStream& stream, const StoryWorld& maps, const std::string& label, const std::vector<TilePos>& from, const std::vector<PlannedLeg>& legs){
    std::vector<PlannedRouteLeg> logged;
    for (const PlannedLeg& leg : legs){
        logged.push_back(planned_route_leg(maps, leg));
    }
    stream.log(ROUTE_LOG_TAG + route_plan_line(label, route_tiles(maps, from), logged));
}

//  Logs a leg that ended on its planned tiles, and how that is known: an
//  open leg counts its taps through a wild battle, so its end is assumed; a
//  leg to a wall is walked again to its wall.
void log_leg_done(VideoStream& stream, const StoryWorld& maps, const std::string& label, const PlannedLeg& planned, size_t wild_battles){
    const bool assumed = !planned.leg.ends_at_wall && wild_battles > 0;
    stream.log(ROUTE_LOG_TAG + route_leg_end_line(label, route_log_leg(planned.leg), route_tiles(maps, planned.after),
        assumed ? EndBasis::ASSUMED : EndBasis::WALKED, assumed ? std::to_string(wild_battles) + " wild battle(s) on the way" : ""));
}

//  The tiles the player passes when walking `tiles` tiles from `from`, the
//  first one after `from` first. Stops early where the player is stopped.
std::vector<TilePos> leg_path(const StoryWorld& grid, TilePos from, Direction direction, size_t tiles){
    std::vector<TilePos> path;
    size_t travelled = 0;
    while (travelled < tiles){
        const std::optional<TilePos> next = step(grid, from, direction);
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
std::string map_of(const StoryWorld& maps, const std::vector<TilePos>& tiles){
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


namespace{

//  Sorted by row, then column, without repeats.
std::vector<TilePos> sorted_unique(std::vector<TilePos> tiles){
    std::sort(tiles.begin(), tiles.end(), [](const TilePos& a, const TilePos& b){
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());
    return tiles;
}

std::string quoted_list(const std::vector<std::string>& reads){
    std::string text;
    for (const std::string& read : reads){
        text += (text.empty() ? "\"" : ", \"") + read + "\"";
    }
    return text;
}

//  Long enough to reach the wall from the farthest tile, with room for a
//  slow start; past the wall the player stands still.
Milliseconds wall_limit(const TileLeg& leg){
    const Milliseconds per_tile = route_ms_per_tile(leg.direction) / ROUTE_RUN_SPEEDUP;
    return ROUTE_START_DELAY + per_tile * leg.tiles * 5 / 4 + 1000ms;
}

//  Tiles a walk to the wall moved, from the time it walked.
size_t tiles_moved(const WalkResult& walk, Milliseconds per_tile){
    const Milliseconds moving = walk.walked - ROUTE_START_DELAY -
        (walk.stop == WalkStop::BLOCKED ? 800ms : 0ms);
    return moving.count() <= 0 ? (size_t)0 : (size_t)(moving / per_tile);
}

//  Whether a leg's walk goes on to the next leg or the route plans again.
enum class LegNext{
    NEXT_LEG,
    REPLAN,
};

//  One planned leg as it is walked: where it goes, and the location
//  banners read on the way.
struct LegWalk{
    const PlannedLeg& planned;
    std::string label;
    JoystickPosition stick;
    std::string map_before;
    std::string map_after;
    std::vector<std::string> banners = {};
    WalkResult result{WalkStop::LIMIT, 0ms, 0};
    //  Tiles the first tap of an open leg moved (see open_leg()).
    size_t turn_moved = 0;
};

bool banner_ok(const LegWalk& walk){
    return std::any_of(walk.banners.begin(), walk.banners.end(), [&](const std::string& read){
        return location_banner_matches(read, walk.map_after);
    });
}

//  Notes the location banners the walk passes, to check map changes. A wild
//  battle or a second walk to the wall may follow the banner: any banner
//  seen during the leg counts.
bool watch_banner(LegWalk& walk, const ImageViewRGB32& screen){
    if (walk.banners.empty() || (!walk.map_after.empty() && !banner_ok(walk) && walk.banners.size() < 8)){
        const std::string read = read_location_banner(screen);
        if (!read.empty() && (walk.banners.empty() || walk.banners.back() != read)){
            walk.banners.push_back(read);
        }
    }
    return false;
}

//  Running on the D-pad, reading the banners on the way.
DpadWalkOptions run_watching(LegWalk& walk){
    return {.arrived = [&walk](const ImageViewRGB32& screen){ return watch_banner(walk, screen); }, .pace = DpadPace::RUN};
}

//  walk_route(): plans, walks each leg, and plans again from where the walk
//  turned out to be when something stopped it.
class RouteWalker{
public:
    RouteWalker(
        VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
        const StoryWorld& maps, RouteTrainers& trainers, const RouteWalkPlan& plan
    )
        : m_stream(stream)
        , m_context(context)
        , m_escape(escape)
        , m_maps(maps)
        , m_grid(maps)
        , m_trainers(trainers)
        , m_plan(plan)
    {}

    std::vector<TilePos> run(std::vector<TilePos> from);

private:
    std::vector<PlannedLeg> plan_legs() const;
    LegNext walk_leg(const PlannedLeg& planned, size_t number, size_t count);
    std::vector<TilePos> arrive() const;

    //  Legs to a wall.
    LegNext wall_leg(LegWalk& walk);
    size_t unstick_wall_leg(LegWalk& walk, size_t walked_tiles, size_t nearest);
    LegNext stopped_short_of_wall(LegWalk& walk, size_t walked_tiles);
    void check_at_wall(LegWalk& walk);
    void on_to_wall(LegWalk& walk);

    //  Open legs, tapped.
    LegNext open_leg(LegWalk& walk);
    void turn_tap(LegWalk& walk);
    LegNext blocked_open_leg(LegWalk& walk, size_t moved);

    //  After each leg.
    void check_banners(LegWalk& walk);
    LegNext after_leg(LegWalk& walk);
    std::vector<size_t> watching_trainers(const std::vector<std::vector<TilePos>>& paths) const;
    LegNext unknown_trainer(LegWalk& walk, const std::vector<std::vector<TilePos>>& paths);
    LegNext known_trainer(LegWalk& walk, const std::vector<std::vector<TilePos>>& paths, const std::vector<size_t>& watching);

    void talk_to_blocker(const std::string& leg_label);
    LegNext replan_from(const LegWalk& walk, std::vector<TilePos> stopped, const std::string& why);
    void mark_blocker(const LegWalk& walk, const std::vector<TilePos>& stopped);

    VideoStream& m_stream;
    ProControllerContext& m_context;
    const WildBattleEscape& m_escape;
    const StoryWorld& m_maps;
    //  Our own copy, so things found in the way can be added to it.
    StoryWorld m_grid;
    RouteTrainers& m_trainers;
    const RouteWalkPlan& m_plan;
    std::vector<TilePos> m_from;
    //  Which way the player faces, once a leg has set it.
    std::optional<Direction> m_facing;
    //  Every banner read on the way, to check the map we arrive on against.
    std::vector<std::string> m_all_banners;
    std::string m_map_start;
    //  Dialogue the map does not explain, so far on this route.
    size_t m_unexplained_talks = 0;
};

std::vector<TilePos> RouteWalker::run(std::vector<TilePos> from){
    //  Tiles to avoid are ground the game lets us walk on (a trainer's line
    //  of sight), so they are off the map, not walls: a leg must not end
    //  "at" one, or the walk would run on through it.
    for (const TilePos& pos : m_plan.avoid){
        if (std::find(from.begin(), from.end(), pos) == from.end()){
            m_grid.uncover(pos);
        }
    }
    m_from = std::move(from);
    m_map_start = map_of(m_maps, m_from);
    for (size_t plans = 0; plans < 8; plans++){
        const std::vector<PlannedLeg> legs = plan_legs();
        route_status(m_stream, m_plan.label + ": " + std::to_string(legs.size()) + " legs planned");
        log_plan(m_stream, m_maps, m_plan.label, m_from, legs);
        bool replanned = false;
        for (size_t number = 1; number <= legs.size() && !replanned; number++){
            replanned = walk_leg(legs[number - 1], number, legs.size()) == LegNext::REPLAN;
        }
        if (!replanned){
            return arrive();
        }
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        m_plan.label + ": too many plans.", m_stream);
}

std::vector<PlannedLeg> RouteWalker::plan_legs() const{
    std::optional<std::vector<PlannedLeg>> legs = plan_robust_route(m_grid, m_from, m_plan.goal, m_plan.costs);
    if (!legs){
        //  From a few possible tiles the exact-open-leg costs found no
        //  plan where the default ones did (r240a, three tiles on Route
        //  204's row 27); the default plan is longer but sound.
        m_stream.log("[Route] " + m_plan.label + ": no plan with the route costs from " + describe(m_maps, m_from) +
            "; trying the default costs.", COLOR_ORANGE);
        legs = plan_robust_route(m_grid, m_from, m_plan.goal, RobustPlanCosts());
    }
    if (!legs){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            m_plan.label + ": no plan from " + describe(m_maps, m_from) + ".", m_stream);
    }
    return std::move(*legs);
}

LegNext RouteWalker::walk_leg(const PlannedLeg& planned, size_t number, size_t count){
    route_status(m_stream, route_leg_line(number, count, planned_route_leg(m_maps, planned)));
    LegWalk walk{
        .planned = planned,
        .label = m_plan.label + ": " + to_string(planned.leg),
        .stick = route_stick(planned.leg.direction),
        .map_before = map_of(m_maps, m_from),
        .map_after = map_of(m_maps, planned.after),
    };
    const LegNext next = planned.leg.ends_at_wall ? wall_leg(walk) : open_leg(walk);
    if (next == LegNext::REPLAN){
        return next;
    }
    m_facing = planned.leg.direction;
    check_banners(walk);
    if (walk.result.stop == WalkStop::TRANSITION){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            walk.label + ": unexpected transition.", m_stream);
    }
    return after_leg(walk);
}

//  Legs whose ends span two maps skip the per-leg check; the walk as a
//  whole must still have seen the banner of the map it ends on.
std::vector<TilePos> RouteWalker::arrive() const{
    const std::string map_end = map_of(m_maps, m_from);
    const bool changed_map = !m_map_start.empty() && !map_end.empty() && map_end != m_map_start;
    const bool seen = std::any_of(m_all_banners.begin(), m_all_banners.end(), [&](const std::string& read){
        return location_banner_matches(read, map_end);
    });
    if (changed_map && !seen){
        const std::string reads = quoted_list(m_all_banners);
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            m_plan.label + ": arrived at " + describe(m_maps, m_from) + " on the map, but " + map_end +
            "'s location banner never showed (read " + (reads.empty() ? "nothing" : reads) + ").", m_stream);
    }
    m_stream.log(ROUTE_LOG_TAG + route_arrived_line(m_plan.label, route_tiles(m_maps, m_from)));
    return m_from;
}


//  Run (B held) on the D-pad: a leg to a wall ends at the wall however fast
//  we get there, and the D-pad keeps to the row.
LegNext RouteWalker::wall_leg(LegWalk& walk){
    const TileLeg& leg = walk.planned.leg;
    const Milliseconds limit = wall_limit(leg);
    walk.result = walk_until_dpad(m_stream, m_context, m_escape, walk.stick, limit, walk.label, run_watching(walk));
    //  A wild battle may end the walk early. Walking to a wall again ends in
    //  the same place, so finish the leg.
    const bool interrupted = walk.result.wild_battles > 0;
    for (size_t again = 0; again < 3 && walk.result.wild_battles > 0 &&
        (walk.result.stop == WalkStop::LIMIT || walk.result.stop == WalkStop::BLOCKED); again++
    ){
        m_stream.log("[Route] " + walk.label + ": a wild battle on the way; walking to the wall again.");
        walk.result = walk_until_dpad(m_stream, m_context, m_escape, walk.stick, limit, walk.label, run_watching(walk));
    }
    if (interrupted){
        return LegNext::NEXT_LEG;
    }
    //  A wall well before the nearest one the map has: something the map
    //  does not know is in the way. Work out where we stopped from the time
    //  walked, put it on our map and plan again from there.
    size_t nearest = SIZE_MAX;
    for (const TilePos& pos : m_from){
        nearest = std::min(nearest, slide_to_wall(m_grid, pos, leg.direction).tiles);
    }
    size_t walked_tiles = tiles_moved(walk.result, route_ms_per_tile(leg.direction) / ROUTE_RUN_SPEEDUP);
    walked_tiles = unstick_wall_leg(walk, walked_tiles, nearest);
    //  Any stop short of the wall's distance: someone who stays put is
    //  talked to, and the leg is planned again from where we are. Within the
    //  time count's slack this may be the wall itself, where A does nothing
    //  and the new plan walks no further (r239a: a wanderer 3 tiles short
    //  passed as "about 8 of 10", and every later leg was off by those tiles).
    if (walk.result.stop == WalkStop::BLOCKED && walked_tiles < nearest){
        return stopped_short_of_wall(walk, walked_tiles);
    }
    //  A leg to the wall that ran out of time is not at its wall: the open
    //  leg before it stopped short, and it ran along another row (r127a went
    //  west along Sandgem's row 13 for row 14, and the next leg north walked
    //  into the lab). Out of tall grass a stop reads, so walk on: a player
    //  still moving is not where the plan has him.
    if (walk.result.stop == WalkStop::LIMIT && route_wall_readable(m_grid, walk.planned.after)){
        check_at_wall(walk);
    }
    return LegNext::NEXT_LEG;
}

//  A walk to the wall that stopped well short of it: tap through a corner,
//  then wait for someone in the way. Returns the tiles walked by then.
size_t RouteWalker::unstick_wall_leg(LegWalk& walk, size_t walked_tiles, size_t nearest){
    const Milliseconds per_tile = route_ms_per_tile(walk.planned.leg.direction) / ROUTE_RUN_SPEEDUP;
    const Milliseconds limit = wall_limit(walk.planned.leg);
    //  A stick walk can catch a corner when we stand part of a tile off
    //  centre, where a D-pad tap, which moves whole tiles, gets through: on
    //  Route 203's stairs the walk west stopped nine times in a row and two
    //  taps crossed (r225a, 2026-10-31). Tap twice, then walk on to the wall.
    if (walk.result.stop == WalkStop::BLOCKED && walked_tiles + 2 < nearest){
        const Direction direction = walk.planned.leg.direction;
        const DpadPosition dpad =
            direction == Direction::NORTH ? DPAD_UP :
            direction == Direction::SOUTH ? DPAD_DOWN :
            direction == Direction::WEST ? DPAD_LEFT : DPAD_RIGHT;
        m_stream.log("[Route] " + walk.label + ": stopped after about " + std::to_string(walked_tiles) +
            " tiles, short of the wall; two D-pad taps, then on to the wall.", COLOR_ORANGE);
        for (size_t tap = 0; tap < 2; tap++){
            pbf_press_dpad(m_context, dpad, 160ms, 640ms);
        }
        m_context.wait_for_all_requests();
        walk.result = walk_until_dpad(m_stream, m_context, m_escape, walk.stick, limit, walk.label, run_watching(walk));
        walked_tiles += tiles_moved(walk.result, per_tile);
    }
    //  Someone walking about may stand in the way for a moment: wait and
    //  walk to the wall again before calling it an obstacle.
    for (size_t again = 0; again < 2 && walk.result.stop == WalkStop::BLOCKED && walked_tiles + 2 < nearest; again++){
        m_stream.log("[Route] " + walk.label + ": stopped after about " + std::to_string(walked_tiles) +
            " tiles, short of the wall; waiting for the way to clear.", COLOR_ORANGE);
        pbf_wait(m_context, 2500ms);
        m_context.wait_for_all_requests();
        walk.result = walk_until_dpad(m_stream, m_context, m_escape, walk.stick, limit, walk.label, run_watching(walk));
        walked_tiles += tiles_moved(walk.result, per_tile);
    }
    return walked_tiles;
}

LegNext RouteWalker::stopped_short_of_wall(LegWalk& walk, size_t walked_tiles){
    //  Someone who stays put: talk to them. A trainer we walked up to from
    //  the side starts the battle this way.
    talk_to_blocker(walk.label);
    std::vector<TilePos> stopped;
    for (const TilePos& pos : m_from){
        const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, SIZE_MAX);
        for (size_t k = walked_tiles == 0 ? 0 : walked_tiles - 1; k <= walked_tiles + 1 && k <= path.size(); k++){
            stopped.push_back(k == 0 ? pos : path[k - 1]);
        }
    }
    stopped = sorted_unique(std::move(stopped));
    if (stopped.empty()){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            walk.label + ": stopped early, and no tile fits.", m_stream);
    }
    mark_blocker(walk, stopped);
    return replan_from(walk, std::move(stopped), "stopped early after about " + std::to_string(walked_tiles) + " tiles");
}

void RouteWalker::check_at_wall(LegWalk& walk){
    const WalkResult check = walk_until_dpad(m_stream, m_context, m_escape, walk.stick, 2000ms,
        walk.label + ": at the wall?", {.arrived = nullptr, .pace = DpadPace::RUN});
    if (check.stop == WalkStop::DIALOG){
        //  A trainer who saw us froze the walk (the "!") until its time ran
        //  out, then walked over (r241a: Aroma Lady Taylor's line crosses
        //  Route 204's corridor). Battle, then walk on to the wall: where
        //  they saw us from is not reliable (BDSP's trainers stand a few
        //  tiles from Platinum's, r242a), and a leg to a wall ends at its
        //  wall however far it got.
        route_status(m_stream, walk.label + ": a trainer stopped us on the way; battling.");
        m_trainers.fight("a trainer on the way");
        m_stream.log("[Route] " + walk.label + ": on to the wall after the battle.", COLOR_ORANGE);
        walk.result = walk_until_dpad(m_stream, m_context, m_escape, walk.stick, wall_limit(walk.planned.leg),
            walk.label + ": on to the wall", run_watching(walk));
    }
    if (check.stop != WalkStop::BLOCKED &&
        !(check.stop == WalkStop::LIMIT && holding_against_wall(m_stream, m_context, walk.stick.x, walk.stick.y, walk.label + ": at the wall?"))
    ){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            walk.label + ": ran out of time short of the wall, and walking on gets " +
            walk_stop_name(check.stop) + ": not where the plan has us.", m_stream);
    }
}

//  After a battle on a leg to a wall: a leg to a wall ends at its wall
//  however far it got, so walk on to it and carry on.
void RouteWalker::on_to_wall(LegWalk& walk){
    m_stream.log("[Route] " + walk.label + ": on to the wall after the battle.", COLOR_ORANGE);
    walk_until_dpad(m_stream, m_context, m_escape, walk.stick, wall_limit(walk.planned.leg),
        walk.label + ": on to the wall", run_watching(walk));
    m_from = walk.planned.after;
    log_leg_done(m_stream, m_maps, m_plan.label, walk.planned, 0);
}


//  Whole tiles on the D-pad, a tap each: a tap moves exactly one tile, while
//  a stick hold timed for three moved one in the Ravaged Path (r235b) and the
//  belief drifted four rows.
LegNext RouteWalker::open_leg(LegWalk& walk){
    const TileLeg& leg = walk.planned.leg;
    if (m_facing != leg.direction){
        turn_tap(walk);
    }
    if (leg.tiles > walk.turn_moved){
        walk.result = walk_tiles_dpad(m_stream, m_context, m_escape, walk.stick, leg.tiles - walk.turn_moved, walk.label);
    }
    //  A wild battle starts once the step onto the grass is done, so the
    //  taps still count; the walk went on after it.
    if (walk.result.wild_battles > 0){
        m_stream.log("[Route] " + walk.label + ": " + std::to_string(walk.result.wild_battles) +
            " wild battle(s) on the way; the tap count holds.", COLOR_ORANGE);
    }
    //  Taps that moved nothing: someone stands in the way (the Youngster
    //  below the Ravaged Path wanders across the row under the cliff, r239a,
    //  r240a). Two still taps end the walk, so the tiles moved are the taps
    //  less those two. Wait once for them to move on and tap the rest (r246a
    //  lost 20 s per block on three waits); someone who stays is talked to,
    //  put on the map, and the leg planned again from the tile we stand on,
    //  which the taps give exactly.
    size_t moved = std::min(leg.tiles, walk.result.moved_chunks + walk.turn_moved);
    if (walk.result.stop == WalkStop::BLOCKED && moved < leg.tiles){
        m_stream.log("[Route] " + walk.label + ": blocked after " + std::to_string(moved) + " of " +
            std::to_string(leg.tiles) + " tiles; waiting for the way to clear.", COLOR_ORANGE);
        pbf_wait(m_context, 2500ms);
        m_context.wait_for_all_requests();
        const WalkResult rest = walk_tiles_dpad(m_stream, m_context, m_escape, walk.stick, leg.tiles - moved, walk.label + ": the rest");
        moved = std::min(leg.tiles, moved + rest.moved_chunks);
        walk.result.stop = rest.stop;
        walk.result.walked += rest.walked;
        walk.result.wild_battles += rest.wild_battles;
    }
    if (walk.result.stop == WalkStop::BLOCKED && moved < leg.tiles){
        return blocked_open_leg(walk, moved);
    }
    return LegNext::NEXT_LEG;
}

//  The first tap of a leg was meant to turn, but a 160 ms D-pad tap turns
//  and steps whichever way the player faced (r248a: all seven did; r246a:
//  "south 1" out of the cave went two tiles, onto the Youngster's row), so
//  the tap is measured like any other and a step counts for the leg.
void RouteWalker::turn_tap(LegWalk& walk){
    const DpadPosition dpad = walk.stick.x < 0 ? DPAD_LEFT : walk.stick.x > 0 ? DPAD_RIGHT : walk.stick.y > 0 ? DPAD_UP : DPAD_DOWN;
    m_context.wait_for_all_requests();
    VideoSnapshot before = m_stream.video().snapshot();
    pbf_press_dpad(m_context, dpad, 160ms, 700ms);
    m_context.wait_for_all_requests();
    VideoSnapshot after = m_stream.video().snapshot();
    if (before && after && walk_screen_change(*before.frame, *after.frame) > WALK_TAP_BLOCKED_CHANGE){
        walk.turn_moved = 1;
        m_stream.log("[Route] " + walk.label + ": the turn tap moved a tile (already facing that way); it counts.", COLOR_ORANGE);
    }
}

LegNext RouteWalker::blocked_open_leg(LegWalk& walk, size_t moved){
    talk_to_blocker(walk.label);
    std::vector<TilePos> stopped;
    for (const TilePos& pos : m_from){
        const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, moved);
        stopped.push_back(moved == 0 || path.empty() ? pos : path[std::min(moved, path.size()) - 1]);
    }
    stopped = sorted_unique(std::move(stopped));
    mark_blocker(walk, stopped);
    return replan_from(walk, std::move(stopped),
        "blocked after " + std::to_string(moved) + " of " + std::to_string(walk.planned.leg.tiles) + " tiles");
}


//  Entering another map shows its name; a missing or wrong name means the
//  walk did not end where the plan says. From tiles on several maps (a leg
//  not confirmed) the banner may already have shown.
void RouteWalker::check_banners(LegWalk& walk){
    m_context.wait_for_all_requests();
    if (VideoSnapshot screen = m_stream.video().snapshot()){
        watch_banner(walk, *screen.frame);
    }
    const std::string banner_text = quoted_list(walk.banners);
    m_all_banners.insert(m_all_banners.end(), walk.banners.begin(), walk.banners.end());
    if (!walk.map_before.empty() && !walk.map_after.empty() && walk.map_after != walk.map_before && !banner_ok(walk)){
        //  A scene that starts as we cross the edge covers the banner (r19: Rowan
        //  and Dawn on Route 201's edge). With no banner read and a dialogue up,
        //  let the next step, the scene, decide where we are. The scene may still
        //  be walking its people in (r20), so give its first line a few seconds.
        ShortDialogWatcher scene_line(COLOR_YELLOW);
        if (banner_text.empty() && wait_until(m_stream, m_context, 6000ms, {{scene_line}}) == 0){
            m_stream.log("[Route] " + walk.label + ": no banner, but a scene started; going on.", COLOR_ORANGE);
            return;
        }
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            walk.label + ": expected to enter " + walk.map_after + ", but the location banner read " +
            (banner_text.empty() ? "nothing" : banner_text) + ".", m_stream);
    }
    if (!banner_text.empty()){
        m_stream.log("[Route] " + walk.label + ": location banner " + banner_text + ".");
    }
    //  Where each leg ended, to compare with the plan after a run.
    if (VideoSnapshot screen = m_stream.video().snapshot()){
        const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
        Filesystem::create_directories(Filesystem::Path(folder));
        screen.frame->save(folder + now_to_filestring() + "-" + normalize_location_name(walk.label) + ".png");
    }
}

//  A trainer who sees us freezes the player and walks over; the dialogue
//  may open after the walk has ended.
LegNext RouteWalker::after_leg(LegWalk& walk){
    const TileLeg& leg = walk.planned.leg;
    std::vector<std::vector<TilePos>> paths;
    for (const TilePos& pos : m_from){
        paths.push_back(leg_path(m_grid, pos, leg.direction, leg.ends_at_wall ? SIZE_MAX : leg.tiles + 1));
    }
    const std::vector<size_t> watching = watching_trainers(paths);
    bool talking = walk.result.stop == WalkStop::DIALOG;
    if (!talking){
        ShortDialogWatcher dialog(COLOR_YELLOW);
        const Milliseconds wait = watching.empty() ? 500ms : 4000ms;
        talking = wait_until(m_stream, m_context, wait, {{dialog}}) == 0;
    }
    if (!talking){
        m_from = walk.planned.after;
        log_leg_done(m_stream, m_maps, m_plan.label, walk.planned, walk.result.wild_battles);
        return LegNext::NEXT_LEG;
    }
    if (watching.empty()){
        return unknown_trainer(walk, paths);
    }
    return known_trainer(walk, paths, watching);
}

//  The trainers whose sight one of the leg's possible paths enters.
std::vector<size_t> RouteWalker::watching_trainers(const std::vector<std::vector<TilePos>>& paths) const{
    std::vector<size_t> watching;
    for (size_t t = 0; t < m_trainers.sights.size(); t++){
        const std::vector<TilePos> sight = sight_tiles(m_grid, m_trainers.sights[t]);
        const bool seen = std::any_of(paths.begin(), paths.end(), [&](const std::vector<TilePos>& path){
            return std::any_of(path.begin(), path.end(), [&](const TilePos& pos){
                return std::find(sight.begin(), sight.end(), pos) != sight.end();
            });
        });
        if (seen){
            watching.push_back(t);
        }
    }
    return watching;
}

//  A trainer the map does not have. Fight, then plan again from anywhere
//  the leg could have stopped.
LegNext RouteWalker::unknown_trainer(LegWalk& walk, const std::vector<std::vector<TilePos>>& paths){
    std::vector<TilePos> anywhere = m_from;
    for (const std::vector<TilePos>& path : paths){
        anywhere.insert(anywhere.end(), path.begin(), path.end());
    }
    anywhere = sorted_unique(std::move(anywhere));
    //  A scene the map lacks (r14: Barry's Route 201 warning, then the row that
    //  pulls us back) would replan into it forever: give up after the second.
    if (++m_unexplained_talks > 2){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            walk.label + ": dialogue the map does not explain stopped us 3 times.", m_stream);
    }
    route_status(m_stream, m_plan.label + ": a trainer the map lacks stopped us; fighting.");
    m_trainers.fight("unknown trainer");
    if (walk.planned.leg.ends_at_wall){
        on_to_wall(walk);
        return LegNext::NEXT_LEG;
    }
    m_from = std::move(anywhere);
    return LegNext::REPLAN;
}

LegNext RouteWalker::known_trainer(LegWalk& walk, const std::vector<std::vector<TilePos>>& paths, const std::vector<size_t>& watching){
    //  Each possible start tile was stopped on the first tile of its path
    //  inside a watching trainer's sight.
    const TrainerSight trainer = m_trainers.sights[watching[0]];
    std::vector<TilePos> stopped;
    for (const std::vector<TilePos>& path : paths){
        const auto first_seen = std::find_if(path.begin(), path.end(), [&](const TilePos& pos){
            return std::any_of(watching.begin(), watching.end(), [&](size_t t){
                const std::vector<TilePos> sight = sight_tiles(m_grid, m_trainers.sights[t]);
                return std::find(sight.begin(), sight.end(), pos) != sight.end();
            });
        });
        if (first_seen != path.end()){
            stopped.push_back(*first_seen);
        }
    }
    if (stopped.empty()){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            walk.label + ": " + trainer.name + " talked, but no path enters their sight.", m_stream);
    }
    m_stream.log(ROUTE_LOG_TAG + route_trainer_line(m_plan.label, trainer.name, route_tiles(m_maps, stopped)));
    route_status(m_stream, "Fighting " + trainer.name + ".");
    m_trainers.fight(trainer.name);
    m_trainers.sights.erase(m_trainers.sights.begin() + watching[0]);
    //  Their sight line only says roughly where: BDSP's trainers stand a few
    //  tiles from Platinum's (r242a). A leg to a wall walks on to its wall.
    if (walk.planned.leg.ends_at_wall){
        on_to_wall(walk);
        return LegNext::NEXT_LEG;
    }
    //  An open leg counts only the taps that moved: the trainer freezes the
    //  player on the tile they saw, and the taps pressed while they walk
    //  over move nobody (r248a: "4 taps" were 2 steps; the belief of two
    //  tiles then cost a 12-tile run to a wall and 5 back).
    const size_t taps = std::min(walk.planned.leg.tiles, walk.result.moved_chunks + walk.turn_moved);
    std::vector<TilePos> by_taps;
    for (const TilePos& pos : m_from){
        const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, walk.planned.leg.tiles);
        const size_t k = std::min(taps, path.size());
        by_taps.push_back(k == 0 ? pos : path[k - 1]);
    }
    return replan_from(walk, sorted_unique(std::move(by_taps)), std::to_string(taps) + " taps before the battle");
}


//  Presses A at whoever stopped the walk; a trainer we walked up to from
//  the side starts the battle this way.
void RouteWalker::talk_to_blocker(const std::string& leg_label){
    pbf_press_button(m_context, BUTTON_A, 80ms, 200ms);
    ShortDialogWatcher dialog(COLOR_YELLOW);
    if (wait_until(m_stream, m_context, 2000ms, {{dialog}}) == 0){
        route_status(m_stream, leg_label + ": someone is in the way; talking to them.");
        m_trainers.fight("someone in the way");
    }
}

//  One tile we may stand on: what stopped us is on the next tile, and goes
//  on our map.
void RouteWalker::mark_blocker(const LegWalk& walk, const std::vector<TilePos>& stopped){
    if (stopped.size() != 1){
        return;
    }
    const TilePos blocker = stopped[0].next(walk.planned.leg.direction);
    m_grid.block(blocker);
    m_stream.log("[Route] " + walk.label + ": something the map lacks is at " +
        describe(m_maps, blocker) + "; marked it.", COLOR_ORANGE);
}

LegNext RouteWalker::replan_from(const LegWalk& walk, std::vector<TilePos> stopped, const std::string& why){
    m_stream.log(ROUTE_LOG_TAG + route_replan_line(m_plan.label, route_log_leg(walk.planned.leg), why, route_tiles(m_maps, stopped)), COLOR_ORANGE);
    m_facing = walk.planned.leg.direction;
    m_from = std::move(stopped);
    return LegNext::REPLAN;
}

}


std::vector<TilePos> walk_route(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps, std::vector<TilePos> from,
    RouteTrainers& trainers, const RouteWalkPlan& plan
){
    return RouteWalker(stream, context, escape, maps, trainers, plan).run(std::move(from));
}

std::vector<TilePos> walk_waypoints(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps, std::vector<TilePos> from,
    RouteTrainers& trainers, const WaypointWalkPlan& plan
){
    for (size_t i = 0; i < plan.waypoints.size(); i++){
        const TilePos goal = plan.waypoints[i];
        from = walk_route(stream, context, escape, maps, from, trainers, {
            .goal = [goal](const TilePos& pos){ return pos == goal; },
            .label = plan.label + " (" + std::to_string(i + 1) + "/" + std::to_string(plan.waypoints.size()) +
                ", to " + describe(maps, goal) + ")",
            .avoid = {},
            .costs = plan.costs,
        });
    }
    return from;
}

void talk_to_npc(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps, std::vector<TilePos>& at,
    RouteTrainers& trainers, const NpcTalk& talk
){
    for (Direction side : talk.sides){
        const TilePos stand = talk.npc.next(side);
        if (!standable(maps, stand)){
            continue;
        }
        at = walk_route(stream, context, escape, maps, at, trainers, {
            .goal = [stand](const TilePos& pos){ return pos == stand; },
            .label = talk.label,
            .avoid = {},
            .costs = route_plan_costs(),
        });
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
            route_status(stream, talk.label + ": talking from " + describe(maps, stand) + ".");
            return;
        }
        route_status(stream, talk.label + ": nobody answered from " + describe(maps, stand) + "; trying another side.");
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        talk.label + ": no side of " + describe(maps, talk.npc) + " started a dialogue.", stream);
}



}
}
}
