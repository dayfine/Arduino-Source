/*  BDSP AutoStory - Planned Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "Pokemon/Navigation/Pokemon_RouteLog.h"
#include "PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_RouteIO.h"
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

void log_plan(RouteIO& io, const StoryWorld& maps, const std::string& label, const std::vector<TilePos>& from, const std::vector<PlannedLeg>& legs){
    std::vector<PlannedRouteLeg> logged;
    for (const PlannedLeg& leg : legs){
        logged.push_back(planned_route_leg(maps, leg));
    }
    io.note(ROUTE_LOG_TAG + route_plan_line(label, route_tiles(maps, from), logged), RouteNote::LOG);
}

//  Logs a leg that ended on its planned tiles, and how that is known: an
//  open leg counts its taps through a wild battle, so its end is assumed; a
//  leg to a wall is walked again to its wall.
void log_leg_done(RouteIO& io, const StoryWorld& maps, const std::string& label, const PlannedLeg& planned, size_t wild_battles){
    const bool assumed = !planned.leg.ends_at_wall && wild_battles > 0;
    io.note(ROUTE_LOG_TAG + route_leg_end_line(label, route_log_leg(planned.leg), route_tiles(maps, planned.after),
        assumed ? EndBasis::ASSUMED : EndBasis::WALKED, assumed ? std::to_string(wild_battles) + " wild battle(s) on the way" : ""), RouteNote::LOG);
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
//  At a run the limit is still timed at a walk: a pace measured too fast
//  must not cut a long leg short.
Milliseconds wall_limit(const TileLeg& leg){
    const Milliseconds per_tile = route_ms_per_tile(leg.direction, MoveSpeed::WALK);
    return ROUTE_START_DELAY + per_tile * leg.tiles * 5 / 4 + 1000ms;
}

//  Fails the program with `message` (RouteNote::FAILURE).
[[noreturn]] void fail(RouteIO& io, const std::string& message){
    io.note(message, RouteNote::FAILURE);
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "RouteIO::note() returned from a failure: " + message);
}

//  route_turn() through `io`.
void turn(RouteIO& io, Direction direction){
    io.note(std::string("[Route] Turning ") + direction_name(direction) + ".", RouteNote::LOG);
    io.press(route_turn_tap(direction));
}

//  A press of A, to talk to someone in front of us.
RouteInput press_a(RouteInputEnd end){
    return {.kind = RouteInputKind::BUTTON_A, .direction = Direction::NORTH, .hold = 80ms, .release = 200ms,
        .count = 1, .end = end};
}

//  A wait for someone walking about to get out of the way.
constexpr RouteInput WAIT_FOR_THE_WAY{.kind = RouteInputKind::WAIT, .direction = Direction::NORTH, .hold = 2500ms,
    .release = 0ms, .count = 1, .end = RouteInputEnd::DONE};

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
    RouteWalker(RouteIO& io, const StoryWorld& maps, RouteTrainers& trainers, const RouteWalkPlan& plan)
        : m_io(io)
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

    RouteIO& m_io;
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
        m_io.note(m_plan.label + ": " + std::to_string(legs.size()) + " legs planned", RouteNote::STATUS);
        log_plan(m_io, m_maps, m_plan.label, m_from, legs);
        bool replanned = false;
        for (size_t number = 1; number <= legs.size() && !replanned; number++){
            replanned = walk_leg(legs[number - 1], number, legs.size()) == LegNext::REPLAN;
        }
        if (!replanned){
            return arrive();
        }
    }
    fail(m_io, m_plan.label + ": too many plans.");
}

std::vector<PlannedLeg> RouteWalker::plan_legs() const{
    std::optional<std::vector<PlannedLeg>> legs = plan_robust_route(m_grid, m_from, m_plan.goal, m_plan.costs);
    if (!legs){
        //  From a few possible tiles the exact-open-leg costs found no
        //  plan where the default ones did (r240a, three tiles on Route
        //  204's row 27); the default plan is longer but sound.
        m_io.note("[Route] " + m_plan.label + ": no plan with the route costs from " + describe(m_maps, m_from) +
            "; trying the default costs.", RouteNote::WARNING);
        legs = plan_robust_route(m_grid, m_from, m_plan.goal, RobustPlanCosts());
    }
    if (!legs){
        fail(m_io, m_plan.label + ": no plan from " + describe(m_maps, m_from) + ".");
    }
    return std::move(*legs);
}

LegNext RouteWalker::walk_leg(const PlannedLeg& planned, size_t number, size_t count){
    m_io.note(route_leg_line(number, count, planned_route_leg(m_maps, planned)), RouteNote::STATUS);
    LegWalk walk{
        .planned = planned,
        .label = m_plan.label + ": " + to_string(planned.leg),
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
        fail(m_io, walk.label + ": unexpected transition.");
    }
    return after_leg(walk);
}

//  Legs whose ends span two maps skip the per-leg check; the walk as a
//  whole must still have seen the banner of the map it ends on.
std::vector<TilePos> RouteWalker::arrive() const{
    const std::string map_end = map_of(m_maps, m_from);
    const bool changed_map = !m_map_start.empty() && !map_end.empty() && map_end != m_map_start;
    bool seen = std::any_of(m_all_banners.begin(), m_all_banners.end(), [&](const std::string& read){
        return location_banner_matches(read, map_end);
    });
    //  The banner can come up after the last leg ends: Sandgem's showed about
    //  a second later (r35). Watch for it a while before judging.
    for (const WallClock end = current_time() + 3000ms; changed_map && !seen && current_time() < end;){
        m_io.look([&](const ImageViewRGB32& screen){
            seen = location_banner_matches(read_location_banner(screen), map_end);
        });
        if (!seen){
            m_io.press({.kind = RouteInputKind::PAUSE, .direction = Direction::NORTH, .hold = 250ms, .release = 0ms,
                .count = 1, .end = RouteInputEnd::QUEUED});
        }
    }
    //  No banner read at all is only a warning (r20-r22: none we could read on
    //  the Lakefront-to-Route 201 crossing); the next step checks where we are.
    if (changed_map && !seen && m_all_banners.empty()){
        m_io.note("[Route] " + m_plan.label + ": no banner read for " + map_end + "; going on.", RouteNote::WARNING);
    }else if (changed_map && !seen){
        const std::string reads = quoted_list(m_all_banners);
        fail(m_io, m_plan.label + ": arrived at " + describe(m_maps, m_from) + " on the map, but " + map_end +
            "'s location banner never showed (read " + (reads.empty() ? "nothing" : reads) + ").");
    }
    m_io.note(ROUTE_LOG_TAG + route_arrived_line(m_plan.label, route_tiles(m_maps, m_from)), RouteNote::LOG);
    return m_from;
}


//  Run (B held) on the D-pad: a leg to a wall ends at the wall however fast
//  we get there, and the D-pad keeps to the row.
LegNext RouteWalker::wall_leg(LegWalk& walk){
    const TileLeg& leg = walk.planned.leg;
    const Milliseconds limit = wall_limit(leg);
    walk.result = m_io.walk_to_wall({.direction = walk.planned.leg.direction, .limit = limit, .label = walk.label, .options = run_watching(walk)});
    //  A wild battle may end the walk early. Walking to a wall again ends in
    //  the same place, so finish the leg.
    const bool interrupted = walk.result.wild_battles > 0;
    for (size_t again = 0; again < 3 && walk.result.wild_battles > 0 &&
        (walk.result.stop == WalkStop::LIMIT || walk.result.stop == WalkStop::BLOCKED); again++
    ){
        m_io.note("[Route] " + walk.label + ": a wild battle on the way; walking to the wall again.", RouteNote::LOG);
        walk.result = m_io.walk_to_wall({.direction = walk.planned.leg.direction, .limit = limit, .label = walk.label, .options = run_watching(walk)});
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
    size_t walked_tiles = tiles_moved(walk.result, route_ms_per_tile(leg.direction, m_plan.speed));
    //  Every leg that reaches its wall measures the pace: the tiles are known.
    if (walk.result.stop == WalkStop::BLOCKED && nearest != SIZE_MAX && walked_tiles >= nearest){
        m_io.note("[Speed] " + std::string(m_plan.speed == MoveSpeed::RUN ? "run" : "walk") + " " +
            direction_name(leg.direction) + ": " + std::to_string(nearest) + " tiles in " +
            std::to_string(walk.result.walked.count()) + " ms to the wall.", RouteNote::LOG);
    }
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
    const Milliseconds per_tile = route_ms_per_tile(walk.planned.leg.direction, m_plan.speed);
    const Milliseconds limit = wall_limit(walk.planned.leg);
    //  A stick walk can catch a corner when we stand part of a tile off
    //  centre, where a D-pad tap, which moves whole tiles, gets through: on
    //  Route 203's stairs the walk west stopped nine times in a row and two
    //  taps crossed (r225a, 2026-10-31). Tap twice, then walk on to the wall.
    if (walk.result.stop == WalkStop::BLOCKED && walked_tiles + 2 < nearest){
        m_io.note("[Route] " + walk.label + ": stopped after about " + std::to_string(walked_tiles) +
            " tiles, short of the wall; two D-pad taps, then on to the wall.", RouteNote::WARNING);
        m_io.press({.kind = RouteInputKind::DPAD, .direction = walk.planned.leg.direction, .hold = 160ms, .release = 640ms,
            .count = 2, .end = RouteInputEnd::DONE});
        walk.result = m_io.walk_to_wall({.direction = walk.planned.leg.direction, .limit = limit, .label = walk.label, .options = run_watching(walk)});
        walked_tiles += tiles_moved(walk.result, per_tile);
    }
    //  Someone walking about may stand in the way for a moment: wait and
    //  walk to the wall again before calling it an obstacle.
    for (size_t again = 0; again < 2 && walk.result.stop == WalkStop::BLOCKED && walked_tiles + 2 < nearest; again++){
        m_io.note("[Route] " + walk.label + ": stopped after about " + std::to_string(walked_tiles) +
            " tiles, short of the wall; waiting for the way to clear.", RouteNote::WARNING);
        m_io.press(WAIT_FOR_THE_WAY);
        walk.result = m_io.walk_to_wall({.direction = walk.planned.leg.direction, .limit = limit, .label = walk.label, .options = run_watching(walk)});
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
        fail(m_io, walk.label + ": stopped early, and no tile fits.");
    }
    mark_blocker(walk, stopped);
    return replan_from(walk, std::move(stopped), "stopped early after about " + std::to_string(walked_tiles) + " tiles");
}

void RouteWalker::check_at_wall(LegWalk& walk){
    WalkResult check = m_io.walk_to_wall({.direction = walk.planned.leg.direction, .limit = 2000ms,
        .label = walk.label + ": at the wall?", .options = {.arrived = nullptr, .pace = DpadPace::RUN}});
    if (check.stop == WalkStop::DIALOG){
        //  A trainer who saw us froze the walk (the "!") until its time ran
        //  out, then walked over (r241a: Aroma Lady Taylor's line crosses
        //  Route 204's corridor). Battle, then walk on to the wall: where
        //  they saw us from is not reliable (BDSP's trainers stand a few
        //  tiles from Platinum's, r242a), and a leg to a wall ends at its
        //  wall however far it got.
        m_io.note(walk.label + ": a trainer stopped us on the way; battling.", RouteNote::STATUS);
        m_trainers.fight("a trainer on the way");
        m_io.note("[Route] " + walk.label + ": on to the wall after the battle.", RouteNote::WARNING);
        walk.result = m_io.walk_to_wall({.direction = walk.planned.leg.direction, .limit = wall_limit(walk.planned.leg),
            .label = walk.label + ": on to the wall", .options = run_watching(walk)});
        //  Where the walk on stopped is what counts now, not the line that
        //  stopped the check (r54: at the wall after Route 202's second
        //  trainer, and failed on the "dialog").
        check = walk.result;
    }
    if (check.stop != WalkStop::BLOCKED &&
        !(check.stop == WalkStop::LIMIT && m_io.holding_against_wall(walk.planned.leg.direction, walk.label + ": at the wall?"))
    ){
        fail(m_io, walk.label + ": ran out of time short of the wall, and walking on gets " +
            walk_stop_name(check.stop) + ": not where the plan has us.");
    }
}

//  After a battle on a leg to a wall: a leg to a wall ends at its wall
//  however far it got, so walk on to it and carry on.
void RouteWalker::on_to_wall(LegWalk& walk){
    m_io.note("[Route] " + walk.label + ": on to the wall after the battle.", RouteNote::WARNING);
    m_io.walk_to_wall({.direction = walk.planned.leg.direction, .limit = wall_limit(walk.planned.leg),
        .label = walk.label + ": on to the wall", .options = run_watching(walk)});
    m_from = walk.planned.after;
    log_leg_done(m_io, m_maps, m_plan.label, walk.planned, 0);
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
        walk.result = m_io.walk_taps(leg.direction, leg.tiles - walk.turn_moved, walk.label);
    }
    //  A wild battle starts once the step onto the grass is done, so the
    //  taps still count; the walk went on after it.
    if (walk.result.wild_battles > 0){
        m_io.note("[Route] " + walk.label + ": " + std::to_string(walk.result.wild_battles) +
            " wild battle(s) on the way; the tap count holds.", RouteNote::WARNING);
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
        m_io.note("[Route] " + walk.label + ": blocked after " + std::to_string(moved) + " of " +
            std::to_string(leg.tiles) + " tiles; waiting for the way to clear.", RouteNote::WARNING);
        m_io.press(WAIT_FOR_THE_WAY);
        const WalkResult rest = m_io.walk_taps(leg.direction, leg.tiles - moved, walk.label + ": the rest");
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

//  Turns to the leg's way with a tap too short to step (50 ms: on the
//  console 40 to 80 ms taps only turned). The leg's taps then each move a
//  tile. The old 160 ms "turn" tap stepped whichever way we faced, and
//  guessing from the screen whether it had was wrong both ways (r248a
//  stepped; r60 only turned but counted a tile, a column off below
//  Jubilife's Center).
void RouteWalker::turn_tap(LegWalk& walk){
    turn(m_io, walk.planned.leg.direction);
    walk.turn_moved = 0;
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
    m_io.look([&walk](const ImageViewRGB32& screen){
        watch_banner(walk, screen);
    });
    const std::string banner_text = quoted_list(walk.banners);
    m_all_banners.insert(m_all_banners.end(), walk.banners.begin(), walk.banners.end());
    if (!walk.map_before.empty() && !walk.map_after.empty() && walk.map_after != walk.map_before && !banner_ok(walk)){
        //  No banner read at all is weak evidence: a scene can cover it (r19), and
        //  the Lakefront-to-Route 201 crossing showed none we could read (r20, r21).
        //  Go on and let the next step check where we are; a banner for another
        //  map still fails.
        if (banner_text.empty()){
            m_io.note("[Route] " + walk.label + ": no banner read for " + walk.map_after + "; going on.", RouteNote::WARNING);
            return;
        }
        fail(m_io, walk.label + ": expected to enter " + walk.map_after + ", but the location banner read " +
            (banner_text.empty() ? "nothing" : banner_text) + ".");
    }
    if (!banner_text.empty()){
        m_io.note("[Route] " + walk.label + ": location banner " + banner_text + ".", RouteNote::LOG);
    }
    //  Where each leg ended, to compare with the plan after a run.
    m_io.look([&walk](const ImageViewRGB32& screen){
        const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
        Filesystem::create_directories(Filesystem::Path(folder));
        screen.save(folder + now_to_filestring() + "-" + normalize_location_name(walk.label) + ".png");
    });
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
        const Milliseconds wait = watching.empty() ? 500ms : 4000ms;
        talking = m_io.dialog_within(wait, COLOR_YELLOW);
    }
    if (!talking){
        m_from = walk.planned.after;
        log_leg_done(m_io, m_maps, m_plan.label, walk.planned, walk.result.wild_battles);
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
        fail(m_io, walk.label + ": dialogue the map does not explain stopped us 3 times.");
    }
    m_io.note(m_plan.label + ": a trainer the map lacks stopped us; fighting.", RouteNote::STATUS);
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
        fail(m_io, walk.label + ": " + trainer.name + " talked, but no path enters their sight.");
    }
    m_io.note(ROUTE_LOG_TAG + route_trainer_line(m_plan.label, trainer.name, route_tiles(m_maps, stopped)), RouteNote::LOG);
    m_io.note("Fighting " + trainer.name + ".", RouteNote::STATUS);
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
    //  The taps can be a tile off too (r55: "5 taps" were 4, under Logan's
    //  nose at Route 202 (26, 5), and the replan walked into him and then
    //  along the wrong row). Plan from both: where the taps say and where his
    //  sight line crosses the leg.
    by_taps.insert(by_taps.end(), stopped.begin(), stopped.end());
    return replan_from(walk, sorted_unique(std::move(by_taps)), std::to_string(taps) + " taps before the battle, or where they saw us");
}


//  Presses A at whoever stopped the walk; a trainer we walked up to from
//  the side starts the battle this way.
void RouteWalker::talk_to_blocker(const std::string& leg_label){
    m_io.press(press_a(RouteInputEnd::QUEUED));
    if (m_io.dialog_within(2000ms, COLOR_YELLOW)){
        m_io.note(leg_label + ": someone is in the way; talking to them.", RouteNote::STATUS);
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
    m_io.note("[Route] " + walk.label + ": something the map lacks is at " +
        describe(m_maps, blocker) + "; marked it.", RouteNote::WARNING);
}

LegNext RouteWalker::replan_from(const LegWalk& walk, std::vector<TilePos> stopped, const std::string& why){
    m_io.note(ROUTE_LOG_TAG + route_replan_line(m_plan.label, route_log_leg(walk.planned.leg), why, route_tiles(m_maps, stopped)), RouteNote::WARNING);
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
    ConsoleRouteIO io(stream, context, escape);
    return walk_route(io, maps, std::move(from), trainers, plan);
}

std::vector<TilePos> walk_route(
    RouteIO& io, const StoryWorld& maps, std::vector<TilePos> from,
    RouteTrainers& trainers, const RouteWalkPlan& plan
){
    return RouteWalker(io, maps, trainers, plan).run(std::move(from));
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
    ConsoleRouteIO io(stream, context, escape);
    talk_to_npc(io, maps, at, trainers, talk);
}

void talk_to_npc(
    RouteIO& io, const StoryWorld& maps, std::vector<TilePos>& at,
    RouteTrainers& trainers, const NpcTalk& talk
){
    for (Direction side : talk.sides){
        const TilePos stand = talk.npc.next(side);
        if (!standable(maps, stand)){
            continue;
        }
        at = walk_route(io, maps, at, trainers, {
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
        turn(io, face);
        io.press(press_a(RouteInputEnd::DONE));
        if (io.dialog_within(2000ms, COLOR_CYAN)){
            io.note(talk.label + ": talking from " + describe(maps, stand) + ".", RouteNote::STATUS);
            return;
        }
        io.note(talk.label + ": nobody answered from " + describe(maps, stand) + "; trying another side.", RouteNote::STATUS);
    }
    fail(io, talk.label + ": no side of " + describe(maps, talk.npc) + " started a dialogue.");
}



}
}
}
