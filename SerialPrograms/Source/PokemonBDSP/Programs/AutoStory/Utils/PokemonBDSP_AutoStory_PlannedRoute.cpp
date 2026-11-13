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
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "PokemonBDSP_AutoStory_LegBattle.h"
#include "PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_RouteChoice.h"
#include "PokemonBDSP_AutoStory_CountedWallLeg.h"
#include "PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP_AutoStory_RouteSteps.h"
#include "PokemonBDSP_AutoStory_WalkDpad.h"
#include "PokemonBDSP_AutoStory_WallWalk.h"

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
    io.note(std::format("{}{}", ROUTE_LOG_TAG, route_plan_line(label, route_tiles(maps, from), logged)), RouteNote::LOG);
}

//  Logs a leg that ended on its planned tiles, and how that is known: an
//  open leg counts its taps through a wild battle, so its end is assumed; a
//  leg to a wall is walked again to its wall.
void log_leg_done(RouteIO& io, const StoryWorld& maps, const std::string& label, const PlannedLeg& planned, size_t wild_battles){
    const bool assumed = !planned.leg.ends_at_wall && wild_battles > 0;
    io.note(std::format("{}{}", ROUTE_LOG_TAG, route_leg_end_line(label, route_log_leg(planned.leg), route_tiles(maps, planned.after),
        assumed ? EndBasis::ASSUMED : EndBasis::WALKED, assumed ? std::to_string(wild_battles) + " wild battle(s) on the way" : "")), RouteNote::LOG);
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

} // namespace


[[noreturn]] void fail(RouteIO& io, const std::string& message){
    io.note(message, RouteNote::FAILURE);
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "RouteIO::note() returned from a failure: " + message);
}

void turn(RouteIO& io, Direction direction){
    io.note(std::string("[Route] Turning ") + direction_name(direction) + ".", RouteNote::LOG);
    io.press(route_turn_tap(direction));
}

RouteInput press_a(RouteInputEnd end){
    return {.kind = RouteInputKind::BUTTON_A, .direction = Direction::NORTH, .hold = 80ms, .release = 200ms,
        .count = 1, .end = end};
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

//  A talk no battle followed is heard out until no dialogue shows this
//  long, pressed through at most this often.
constexpr Milliseconds HEAR_OUT_QUIET = 5000ms;
constexpr size_t HEAR_OUT_PRESSES = 6;

//  A wait for someone walking about to get out of the way.
constexpr RouteInput WAIT_FOR_THE_WAY{.kind = RouteInputKind::WAIT, .direction = Direction::NORTH, .hold = 2500ms,
    .release = 0ms, .count = 1, .end = RouteInputEnd::DONE};

//  Tiles a walk to the wall moved, from the time it walked.
size_t tiles_moved(const WalkResult& walk, Milliseconds per_tile){
    if (walk.stop == WalkStop::LIMIT && walk.moved_chunks > 0){
        const Milliseconds moving = walk.walked - ROUTE_START_DELAY;
        const size_t tiles_from_time = moving.count() <= 0 ? (size_t)0 : (size_t)(moving / per_tile);
        const size_t tiles_from_chunks = (walk.moved_chunks * 400ms) / per_tile;
        return std::min(tiles_from_time, std::max(walk.moved_chunks, tiles_from_chunks));
    }
    const Milliseconds moving = walk.walked - ROUTE_START_DELAY -
        (walk.stop == WalkStop::BLOCKED ? 800ms : 0ms);
    return moving.count() <= 0 ? (size_t)0 : (size_t)(moving / per_tile);
}

//  Tiles a walk on to the wall after a stop moved: none if no chunk moved.
//  A push against someone still in the way reads as stopped late, and its
//  time counted as tiles put a walk stopped 3 tiles in at its wall 17 tiles
//  away (the sim of lab-B 2026-11-12 02:52).
size_t moved_on(const WalkResult& walk, Milliseconds per_tile){
    return walk.moved_chunks == 0 ? 0 : tiles_moved(walk, per_tile);
}

//  Whether a walk to the wall never moved: no chunk changed and its time
//  counts no tile. It stopped where it began, so whoever stopped it stands
//  on the next tile, however short the leg. On lab-B (2026-11-08 16:50,
//  01.08) Route 204's wandering youngster stood a tile ahead of "west 2 (to
//  wall)" from (13, 2), and a replan from (12, 2) and (13, 2) found no plan:
//  no wall is anchored from a tile someone wanders on.
bool never_moved(const WalkResult& walk, size_t walked_tiles){
    return walk.stop == WalkStop::BLOCKED && walk.moved_chunks == 0 && walked_tiles == 0;
}

//  Whether a walk to the wall that stopped took far longer than the tiles
//  to its nearest wall `nearest` allow, past the walker's slack: two of its
//  400 ms chunks, or 30% of the tiles' time on a long leg. Something held
//  it up on the way, and its time no longer counts tiles. On lab-B (2026-11-11 21:00, 00.05b) "east 8 (to wall)" stopped after 2800 ms
//  against a boy below the Mart, about 5 tiles in, but the time counted 17.
bool held_up(const WalkResult& walk, size_t nearest, Milliseconds per_tile){
    if (walk.stop != WalkStop::BLOCKED || walk.wild_battles > 0){
        return false;
    }
    const Milliseconds tiles_time = per_tile * nearest;
    const Milliseconds slack = std::max<Milliseconds>(800ms, tiles_time * 3 / 10);
    //  800 ms: the two still chunks that read as stopped (tiles_moved()).
    return walk.walked > ROUTE_START_DELAY + tiles_time + 800ms + slack;
}

//  Legs to a wall this many tiles or shorter must walk every tile: a tile
//  short is someone in the way, not the time count's slack. On lab-B
//  (2026-11-08, 01.08) Route 204's youngster stopped "west 2 (to wall)"
//  after 1 tile; a tile short passed as at the wall, the route logged
//  arriving at (11, 2) from (13, 2), and the taps north hit the cliff.
constexpr size_t EXACT_WALL_LEG_TILES = 3;

//  The fewest tiles a leg to a wall `expected_tiles` away must have walked
//  to count as at its wall.
size_t min_tiles_at_wall(size_t expected_tiles){
    return expected_tiles > EXACT_WALL_LEG_TILES ? expected_tiles * 7 / 10 : expected_tiles;
}

//  Whether a leg's walk goes on to the next leg or the route plans again.
enum class LegNext{
    NEXT_LEG,
    REPLAN,
    ARRIVED_EARLY,  //  through a warp at the leg's end (walk_route_to_warp())
    SCENE,          //  the next step's scene started (RouteWalkPlan::scene_next)
};
//  What stopped a walk where the map has nothing (see mark_blocker()).
enum class Blocker{ THING, PERSON };
//  How far past a leg's end a walk that ran on may have gone.
constexpr size_t WARP_SLIDE_TILES = 2;
//  How long after a leg that may have slid onto a warp the walk looks for the
//  banner of the map beyond: it shows about 1.5 s after the fade.
constexpr Milliseconds BANNER_AFTER_SLIDE = 2500ms;

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

    RouteEnd run(std::vector<TilePos> from);

private:
    std::optional<std::vector<PlannedLeg>> plan_legs(bool first) const;
    bool find_again();
    LegNext walk_leg(const PlannedLeg& planned, size_t number, size_t count);
    std::vector<TilePos> arrive() const;

    //  Legs to a wall.
    LegNext wall_leg(LegWalk& walk);
    std::optional<LegNext> finish_interrupted_wall(LegWalk& walk, Milliseconds limit);
    size_t unstick_wall_leg(LegWalk& walk, size_t walked_tiles, size_t nearest);
    LegNext stopped_short_of_wall(LegWalk& walk, size_t walked_tiles);
    LegNext stopped_near_count(LegWalk& walk, size_t walked_tiles, Blocker blocker);
    LegNext stopped_well_short(LegWalk& walk, size_t walked_tiles, bool first_moved);
    std::vector<TilePos> back_to_the_wall(const LegWalk& walk, const std::vector<TilePos>& stopped, Blocker blocker);
    LegNext held_up_on_the_way(const LegWalk& walk, size_t nearest);
    LegNext check_at_wall(LegWalk& walk, size_t walked_tiles = SIZE_MAX, size_t expected_tiles = SIZE_MAX);
    LegNext on_to_wall(LegWalk& walk);

    //  Open legs, tapped.
    LegNext open_leg(LegWalk& walk);
    LegNext interrupted_open_leg(LegWalk& walk);
    void turn_tap(LegWalk& walk);
    LegNext blocked_open_leg(LegWalk& walk, size_t moved);
    LegNext talking_blocker(LegWalk& walk, size_t moved);
    LegNext mark_and_replan(LegWalk& walk, size_t moved, Blocker blocker);

    //  After each leg.
    void check_banners(LegWalk& walk);
    bool slid_through_unseen(const LegWalk& walk);
    LegNext after_leg(LegWalk& walk);
    std::vector<size_t> watching_trainers(const std::vector<std::vector<TilePos>>& paths) const;
    LegNext unknown_trainer(LegWalk& walk, const std::vector<std::vector<TilePos>>& paths);
    LegNext known_trainer(LegWalk& walk, const std::vector<std::vector<TilePos>>& paths, const std::vector<size_t>& watching);

    Blocker talk_to_blocker(const std::string& leg_label);
    void count_unexplained_talk(const std::string& leg_label, const std::vector<TilePos>& near);
    bool heard_out_a_talk(const std::string& leg_label);
    LegNext replan_after_talk(const LegWalk& walk, std::vector<TilePos> stopped);
    std::vector<TilePos> talk_stop_tiles(const LegWalk& walk, size_t walked_tiles) const;
    std::vector<TilePos> open_leg_talk_tiles(const LegWalk& walk) const;
    LegNext replan_from(const LegWalk& walk, std::vector<TilePos> stopped, const std::string& why);
    void mark_blocker(const LegWalk& walk, const std::vector<TilePos>& stopped, Blocker blocker);
    void mark_tiles_past(const LegWalk& walk, const std::vector<TilePos>& stopped);

    RouteIO& m_io;
    const StoryWorld& m_maps;
    //  Our own copy, so things found in the way can be added to it.
    StoryWorld m_grid;
    RouteTrainers& m_trainers;
    const RouteWalkPlan& m_plan;
    std::vector<TilePos> m_from;
    //  Which way the player faces, once a leg has set it.
    std::optional<Direction> m_facing;
    std::optional<WallBefore> m_prev_wall;  //  the leg before this one, if it went to a wall
    //  Every banner read on the way, to check the map we arrive on against.
    std::vector<std::string> m_all_banners;
    std::string m_map_start;
    WarpSeen m_warp;
    //  Dialogue the map does not explain, so far on this route.
    size_t m_unexplained_talks = 0;
};

RouteEnd RouteWalker::run(std::vector<TilePos> from){
    //  Tiles to avoid are ground the game lets us walk on (a trainer's line
    //  of sight), so they are off the map, not walls: a leg must not end
    //  "at" one, or the walk would run on through it. Walls stay walls.
    for (const TilePos& pos : m_plan.avoid){
        if (m_grid.tile(pos) != Pokemon::TileKind::WALL && std::find(from.begin(), from.end(), pos) == from.end()){
            m_grid.uncover(pos);
        }
    }
    m_from = std::move(from);
    m_map_start = map_of(m_maps, m_from);
    for (size_t plans = 0; plans < 8; plans++){
        const std::optional<std::vector<PlannedLeg>> planned = plan_legs(plans == 0);
        if (!planned){
            if (!find_again()){
                fail(m_io, m_plan.label + ": no plan from " + describe(m_maps, m_from) + ".");
            }
            continue;
        }
        const std::vector<PlannedLeg>& legs = *planned;
        m_io.note(m_plan.label + ": " + std::to_string(legs.size()) + " legs planned", RouteNote::STATUS);
        log_plan(m_io, m_maps, m_plan.label, m_from, legs);
        LegNext next = LegNext::NEXT_LEG;
        for (size_t number = 1; number <= legs.size() && next == LegNext::NEXT_LEG; number++){
            next = walk_leg(legs[number - 1], number, legs.size());
        }
        if (next != LegNext::REPLAN){
            return {next == LegNext::NEXT_LEG ? arrive() : m_from, m_warp, next == LegNext::SCENE};
        }
    }
    fail(m_io, m_plan.label + ": too many plans.");
}

std::optional<std::vector<PlannedLeg>> RouteWalker::plan_legs(bool first) const{
    std::optional<std::vector<PlannedLeg>> legs = plan_walk({m_grid, m_from, m_plan.goal, m_plan.costs, m_plan.speed, {}, m_plan.goal_is_trigger}, first);
    if (!legs){
        //  From a few possible tiles the exact-open-leg costs found no
        //  plan where the default ones did (r240a, three tiles on Route
        //  204's row 27); the default plan is longer but sound.
        m_io.note("[Route] " + m_plan.label + ": no plan with the route costs from " + describe(m_maps, m_from) +
            "; trying the default costs.", RouteNote::WARNING);
        legs = plan_recovered_route(m_grid, m_from, m_plan.goal, RobustPlanCosts());
    }
    if (!legs && !m_plan.avoid.empty()){
        m_io.note("[Route] " + m_plan.label + ": no plan avoiding hazard tiles; trying without hazard avoidance.", RouteNote::WARNING);
        legs = plan_recovered_route(m_maps, m_from, m_plan.goal, m_plan.costs);
        if (!legs){
            legs = plan_recovered_route(m_maps, m_from, m_plan.goal, RobustPlanCosts());
        }
    }
    return legs;
}

//  From tiles no plan brings to one wall (a talk on the plaza's open legs
//  spread us over two columns, lab-B 2026-11-12 07:44, 00.05c run 8): hold
//  toward a wall each of them stops at, the way whose walls tiles the time
//  cannot tell apart share, and keep the walls whose distance the time
//  walked fits. Returns whether there was such a way.
bool RouteWalker::find_again(){
    constexpr size_t SLACK = 2;
    for (const Direction direction : {Direction::SOUTH, Direction::NORTH, Direction::EAST, Direction::WEST}){
        std::vector<WallHit> hits;
        size_t farthest = 0;
        for (const TilePos& pos : m_from){
            hits.push_back(slide_to_wall(m_grid, pos, direction));
            farthest = std::max(farthest, hits.back().tiles);
        }
        const bool apart = std::all_of(hits.begin(), hits.end(), [&](const WallHit& a){
            return std::all_of(hits.begin(), hits.end(), [&](const WallHit& b){
                return a.stop == b.stop || a.tiles > b.tiles + SLACK || b.tiles > a.tiles + SLACK;
            });
        });
        if (!apart || farthest == 0){
            continue;
        }
        const TileLeg leg{.direction = direction, .tiles = farthest, .ends_at_wall = true, .held = false};
        const std::string label = m_plan.label + ": " + to_string(leg) + " to find us again";
        const WalkResult walk = m_io.walk_to_wall({.direction = direction, .limit = wall_limit(leg), .label = label,
            .options = {.arrived = nullptr, .pace = DpadPace::RUN}});
        if (walk.stop != WalkStop::BLOCKED && walk.stop != WalkStop::LIMIT){
            fail(m_io, label + ": " + walk_stop_name(walk.stop) + " instead of a wall.");
        }
        const size_t walked = tiles_moved(walk, route_ms_per_tile(direction, m_plan.speed));
        std::vector<TilePos> found;
        for (const WallHit& hit : hits){
            if (hit.tiles + SLACK >= walked && walked + SLACK >= hit.tiles){
                found.push_back(hit.stop);
            }
        }
        if (found.empty()){
            for (const WallHit& hit : hits){
                found.push_back(hit.stop);
            }
        }
        m_from = sorted_unique(std::move(found));
        m_facing = direction;
        m_io.note(std::format("{}{}", ROUTE_LOG_TAG, route_found_line(m_plan.label, route_tiles(m_maps, m_from), 1)), RouteNote::WARNING);
        return true;
    }
    return false;
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
        m_prev_wall.reset();
        return next;
    }
    m_facing = planned.leg.direction;
    if (walk.result.stop == WalkStop::TRANSITION){
        if (!warp_within(m_grid, m_from, planned.leg.direction, planned.leg.tiles + WARP_SLIDE_TILES)){
            fail(m_io, walk.label + ": unexpected transition.");
        }
        m_io.note("[Route] " + walk.label + ": through the warp at its end (" + describe(walk.result.warp) + "); handing over.", RouteNote::WARNING);
        m_warp = walk.result.warp;
        return LegNext::ARRIVED_EARLY;
    }
    if (slid_through_unseen(walk)){
        return LegNext::ARRIVED_EARLY;
    }
    check_banners(walk);
    const LegNext done = after_leg(walk);
    m_prev_wall = done == LegNext::NEXT_LEG && planned.leg.ends_at_wall ? std::optional<WallBefore>{{planned.leg.direction, wall_limit(planned.leg)}} : std::nullopt;
    return done;
}

//  A leg that stayed on its map but could have run on over a warp (a count
//  off by a tile or two: lab-B 2026-11-12, 01.02's east mouth) may have gone
//  through with its fade between two frames. The banner of another map in
//  the next moments says so: the walk is over, through the warp.
bool RouteWalker::slid_through_unseen(const LegWalk& walk){
    const TileLeg& leg = walk.planned.leg;
    if (walk.map_before != walk.map_after || !warp_within(m_grid, m_from, leg.direction, leg.tiles + WARP_SLIDE_TILES)){
        return false;
    }
    const std::string read = m_io.banner_within(BANNER_AFTER_SLIDE);
    if (!location_banner_is_another(read, walk.map_before)){
        return false;
    }
    m_io.note("[Route] " + walk.label + ": the location banner \"" + read + "\" shows after the leg, no fade seen: through the warp; handing over.", RouteNote::WARNING);
    m_warp.banner = read;
    return true;
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
    m_io.note(std::format("{}{}", ROUTE_LOG_TAG, route_arrived_line(m_plan.label, route_tiles(m_maps, m_from))), RouteNote::LOG);
    return m_from;
}

//  Run (B held) on the D-pad: a leg to a wall ends at the wall however fast
//  we get there, and the D-pad keeps to the row.
LegNext RouteWalker::wall_leg(LegWalk& walk){
    const TileLeg& leg = walk.planned.leg;
    const Milliseconds limit = wall_limit(leg);
    walk.result = walk_to_wall_turning(m_io, m_facing, {.direction = leg.direction, .limit = limit, .label = walk.label, .options = run_watching(walk)});
    if (const std::optional<LegNext> next = finish_interrupted_wall(walk, limit)){
        return *next;
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
    //  Checked on the first walk only: one after a wait starts part way.
    if (nearest != SIZE_MAX && walked_tiles >= nearest && held_up(walk.result, nearest, route_ms_per_tile(leg.direction, m_plan.speed))){
        return held_up_on_the_way(walk, nearest);
    }
    const bool well_short = walked_tiles + 2 < nearest && !never_moved(walk.result, walked_tiles);
    const bool first_moved = walk.result.moved_chunks > 0;
    walked_tiles = unstick_wall_leg(walk, walked_tiles, nearest);
    if (walk.result.stop == WalkStop::BLOCKED && walked_tiles < nearest && well_short){
        return stopped_well_short(walk, walked_tiles, first_moved);
    }
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
        //  A short leg a tile short stopped against someone in the way.
        if (nearest <= EXACT_WALL_LEG_TILES && walked_tiles < nearest){
            return stopped_short_of_wall(walk, walked_tiles);
        }
        return check_at_wall(walk, walked_tiles, nearest);
    }
    return LegNext::NEXT_LEG;
}

//  Nothing without a wild battle on the walk; else how the leg goes on.
std::optional<LegNext> RouteWalker::finish_interrupted_wall(LegWalk& walk, Milliseconds limit){
    if (walk.result.wild_battles == 0){
        return std::nullopt;
    }
    //  A battle that may have begun before the hold changes nothing here:
    //  from where the leg began, it ends at its wall however far it got.
    for (size_t again = 0; again < 3 && walk.result.wild_battles > 0; again++){
        if (walk.result.stop != WalkStop::LIMIT && walk.result.stop != WalkStop::BLOCKED){
            fail(m_io, "unconfirmed transition/dialogue during wall recovery: " + walk.label);
        }
        m_io.note("[Route] " + walk.label + ": a wild battle on the way; walking to the wall again.", RouteNote::LOG);
        walk.result = m_io.walk_to_wall({.direction = walk.planned.leg.direction, .limit = limit,
            .label = walk.label, .options = run_watching(walk)});
    }
    if (walk.result.wild_battles > 0){
        fail(m_io, "recovery budget exhausted: " + walk.label);
    }
    if (walk.result.stop != WalkStop::BLOCKED){
        return check_at_wall(walk);
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
        walked_tiles += moved_on(walk.result, per_tile);
    }
    //  Someone walking about may stand in the way for a moment: wait and
    //  walk to the wall again before calling it an obstacle. A walk that
    //  never moved waits on a short leg too (never_moved()).
    const auto in_the_way = [&]{
        return walk.result.stop == WalkStop::BLOCKED &&
            (walked_tiles + 2 < nearest || (nearest > 0 && never_moved(walk.result, walked_tiles)));
    };
    for (size_t again = 0; again < 2 && in_the_way(); again++){
        m_io.note("[Route] " + walk.label + ": stopped after about " + std::to_string(walked_tiles) +
            " tiles, short of the wall; waiting for the way to clear.", RouteNote::WARNING);
        m_io.press(WAIT_FOR_THE_WAY);
        walk.result = m_io.walk_to_wall({.direction = walk.planned.leg.direction, .limit = limit, .label = walk.label, .options = run_watching(walk)});
        walked_tiles += moved_on(walk.result, per_tile);
    }
    return walked_tiles;
}

LegNext RouteWalker::stopped_short_of_wall(LegWalk& walk, size_t walked_tiles){
    //  Someone who stays put: talk to them. A trainer we walked up to from
    //  the side starts the battle this way.
    return stopped_near_count(walk, walked_tiles, talk_to_blocker(walk.label));
}

LegNext RouteWalker::stopped_near_count(LegWalk& walk, size_t walked_tiles, Blocker blocker){
    //  A walk that never moved is where it began (never_moved()); any other
    //  stop may be a tile either side of the tiles its time counts.
    const size_t last = never_moved(walk.result, walked_tiles) ? 0 : walked_tiles + 1;
    std::vector<TilePos> stopped;
    for (const TilePos& pos : m_from){
        const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, SIZE_MAX);
        for (size_t k = walked_tiles == 0 ? 0 : walked_tiles - 1; k <= last && k <= path.size(); k++){
            stopped.push_back(k == 0 ? pos : path[k - 1]);
        }
    }
    stopped = sorted_unique(std::move(stopped));
    if (stopped.empty()){
        fail(m_io, walk.label + ": stopped early, and no tile fits.");
    }
    mark_blocker(walk, stopped, blocker);
    if (never_moved(walk.result, walked_tiles)){
        mark_tiles_past(walk, stopped);
    }
    return replan_from(walk, std::move(stopped), "stopped early after about " + std::to_string(walked_tiles) + " tiles");
}

//  A walk to the wall that stopped well short of it and stayed stopped:
//  its time no longer counts tiles. A push against someone reads as
//  stopped only late, so the count is at most a tile under where we stand,
//  and may be many over: on lab-B (2026-11-12 02:52, 00.05c) "west 17 (to
//  wall)" from (58, 26) stopped after 2400 ms, read as "about 13 tiles",
//  and the route went on from (44..46, 26) alone. Any tile of the leg up to
//  a tile past the count fits; walk back to the wall behind us to find
//  which, then plan again.
LegNext RouteWalker::stopped_well_short(LegWalk& walk, size_t walked_tiles, bool first_moved){
    const Blocker blocker = talk_to_blocker(walk.label);
    std::vector<TilePos> stopped;
    for (const TilePos& pos : m_from){
        if (!first_moved){
            stopped.push_back(pos);
        }
        const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, walked_tiles + 1);
        stopped.insert(stopped.end(), path.begin(), path.end());
    }
    stopped = sorted_unique(std::move(stopped));
    if (stopped.empty()){
        fail(m_io, walk.label + ": stopped well short of the wall, and no tile fits.");
    }
    if (stopped.size() == 1){
        mark_blocker(walk, stopped, blocker);
        return replan_from(walk, std::move(stopped), "stopped well short of the wall");
    }
    std::vector<TilePos> back = back_to_the_wall(walk, stopped, blocker);
    //  No safe wall behind: the tiles round the count, as before; the whole
    //  leg across a map's edge left Route 204's walk with no plan (sim).
    if (back.empty()){
        return stopped_near_count(walk, walked_tiles, blocker);
    }
    const LegNext next = replan_from(walk, std::move(back), "back at the wall behind the stop");
    m_facing = Direction(((int)walk.planned.leg.direction + 2) % 4);
    return next;
}

//  From several tiles a stop may be on, D-pad taps back to the wall the leg
//  came from: a tap moves exactly a tile, so the taps that moved tell which
//  tile we stopped on, and whatever stopped us stands on the next tile on.
//  Returns where the taps leave us (each stop tile the taps' moved tiles
//  back, if they stopped short of every wall), or nothing, before any tap,
//  when there is no safe wall behind (a warp or someone walking about on
//  the way).
std::vector<TilePos> RouteWalker::back_to_the_wall(const LegWalk& walk, const std::vector<TilePos>& stopped, Blocker blocker){
    const Direction ahead = walk.planned.leg.direction;
    const Direction back = Direction(((int)ahead + 2) % 4);
    size_t most = 0;
    for (const TilePos& pos : stopped){
        const WallHit hit = slide_to_wall(m_grid, pos, back);
        const std::vector<TilePos> path = leg_path(m_grid, pos, back, hit.tiles);
        if (!hit.anchored || path.size() != hit.tiles || std::any_of(path.begin(), path.end(), [&](const TilePos& tile){
            return recovery_hazard(m_grid, tile);
        })){
            return {};
        }
        most = std::max(most, hit.tiles);
    }
    if (most == 0){
        return {};
    }
    m_io.note(std::format("[Route] {}: stopped on one of {} tiles; {} taps back to the wall to find which.",
        walk.label, stopped.size(), most), RouteNote::WARNING);
    turn(m_io, back);
    const WalkResult taps = m_io.walk_taps(back, most, walk.label + ": back to the wall");
    if (taps.stop != WalkStop::BLOCKED && taps.stop != WalkStop::LIMIT){
        fail(m_io, walk.label + ": " + walk_stop_name(taps.stop) + " on the taps back to the wall.");
    }
    const size_t moved = std::min(most, taps.moved_chunks);
    std::vector<TilePos> found;
    std::vector<TilePos> now;
    std::vector<TilePos> shifted;
    for (const TilePos& pos : stopped){
        const WallHit hit = slide_to_wall(m_grid, pos, back);
        if (hit.tiles == moved && taps.wild_battles == 0){
            found.push_back(pos);
            now.push_back(hit.stop);
        }
        const std::vector<TilePos> path = leg_path(m_grid, pos, back, moved);
        if (path.size() == moved){
            shifted.push_back(moved == 0 ? pos : path.back());
        }
    }
    if (now.empty() && shifted.empty()){
        fail(m_io, walk.label + ": the taps back to the wall fit no tile.");
    }
    if (found.size() == 1){
        mark_blocker(walk, found, blocker);
    }
    return sorted_unique(now.empty() ? std::move(shifted) : std::move(now));
}

LegNext RouteWalker::check_at_wall(LegWalk& walk, size_t walked_tiles, size_t expected_tiles){
    if (expected_tiles != SIZE_MAX && expected_tiles > 0 && walked_tiles != SIZE_MAX){
        if (walked_tiles < min_tiles_at_wall(expected_tiles)){
            fail(m_io, walk.label + ": ran out of time well short of the wall (" +
                std::to_string(walked_tiles) + " of " + std::to_string(expected_tiles) + " tiles): not where the plan has us.");
        }
    }
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
        const bool battled = m_trainers.fight("a trainer on the way");
        if (heard_out_a_talk(walk.label) || !battled){
            //  No battle, seen in the fight or after: someone who only talks and may stand in
            //  the way (00.05c's Wonder Station woman, lab-B 2026-11-12: at 04:58 the walk on
            //  ran into her; at 06:12 her talk had ended in the fight and the walk on restarted it).
            std::vector<TilePos> stopped = talk_stop_tiles(walk, walked_tiles == SIZE_MAX ? SIZE_MAX : walked_tiles + check.moved_chunks);
            count_unexplained_talk(walk.label, stopped);
            return replan_after_talk(walk, std::move(stopped));
        }
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
    return LegNext::NEXT_LEG;
}

//  After a battle on a leg to a wall: a leg to a wall ends at its wall
//  however far it got, so walk on to it and carry on.
LegNext RouteWalker::on_to_wall(LegWalk& walk){
    const Milliseconds limit = wall_limit(walk.planned.leg);
    walk.result = walk_on_to_wall(m_io, m_trainers, m_prev_wall, walk.result.moved_chunks + walk.turn_moved == 0,
        {.direction = walk.planned.leg.direction, .limit = limit, .label = walk.label + ": on to the wall", .options = run_watching(walk)});
    std::optional<LegNext> next = finish_interrupted_wall(walk, limit);
    if (!next && walk.result.stop != WalkStop::BLOCKED){
        next = check_at_wall(walk);
    }
    if (next && *next != LegNext::NEXT_LEG){
        return *next;
    }
    m_from = walk.planned.after;
    log_leg_done(m_io, m_maps, m_plan.label, walk.planned, 0);
    return LegNext::NEXT_LEG;
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
    if (walk.result.wild_battles > 0){
        return interrupted_open_leg(walk);
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
        if (rest.stop == WalkStop::DIALOG && rest.moved_chunks == 0 && rest.wild_battles == 0){
            return talking_blocker(walk, moved);
        }
        moved = std::min(leg.tiles, moved + rest.moved_chunks);
        walk.result.stop = rest.stop;
        walk.result.walked += rest.walked;
        walk.result.wild_battles += rest.wild_battles;
        if (rest.origin == WalkOrigin::MAY_PRECEDE_LEG){
            walk.result.origin = rest.origin;
        }
    }
    if (walk.result.wild_battles > 0){
        return interrupted_open_leg(walk);
    }
    if (walk.result.stop == WalkStop::BLOCKED && moved < leg.tiles){
        return blocked_open_leg(walk, moved);
    }
    return LegNext::NEXT_LEG;
}

//  Where an open leg's battle left us: where its screen events place it
//  (place_leg_battle()), a tile or so from where the taps left us; else
//  any tile of the leg from where it began, none moved to all. A battle that may have begun before the leg
//  (WalkOrigin::MAY_PRECEDE_LEG) found us on the tile the leg before ended
//  on, since a battle starts on a tile stepped onto, so the same tiles
//  hold. Planning again from them is bounded by run()'s plans.
LegNext RouteWalker::interrupted_open_leg(LegWalk& walk){
    if (walk.result.stop != WalkStop::LIMIT && walk.result.stop != WalkStop::BLOCKED){
        fail(m_io, "interrupted open leg has an unconfirmed dialogue/transition: " + walk.label);
    }
    const TileLeg& leg = walk.planned.leg;
    const LegBattlePlace place = place_leg_battle(walk.result, leg.tiles);
    m_io.note(std::format("{}{}", ROUTE_LOG_TAG, leg_battle_line(walk.label, walk.result, place)), RouteNote::LOG);
    auto possible = place.moved
        ? tiles_moved(m_grid, m_from, leg.direction, *place.moved)
        : interrupted_leg_positions(m_grid, m_from, leg.direction, leg.tiles);
    const auto next = replan_from(walk, std::move(possible), place.moved
        ? "encounter: placed by its screen events" : "encounter: credited taps do not establish pose");
    m_facing.reset();
    return next;
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
    return mark_and_replan(walk, moved, talk_to_blocker(walk.label));
}

//  Someone in the way who talks as we tap on after the wait is the one who
//  blocked us, not a trainer the map lacks (lab-B 2026-11-12, 00.05c).
LegNext RouteWalker::talking_blocker(LegWalk& walk, size_t moved){
    m_io.note(walk.label + ": someone in the way talked to us; going round them.", RouteNote::STATUS);
    m_trainers.fight("someone in the way");
    return mark_and_replan(walk, moved, Blocker::PERSON);
}

//  The tiles a blocked open leg may have stopped on after `moved` tiles:
//  what stopped it goes on the map, and the walk plans again from them.
LegNext RouteWalker::mark_and_replan(LegWalk& walk, size_t moved, Blocker blocker){
    std::vector<TilePos> stopped;
    for (const TilePos& pos : m_from){
        const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, moved);
        stopped.push_back(moved == 0 || path.empty() ? pos : path[std::min(moved, path.size()) - 1]);
    }
    stopped = sorted_unique(std::move(stopped));
    mark_blocker(walk, stopped, blocker);
    if (moved == 0){
        mark_tiles_past(walk, stopped);
    }
    stopped = blocked_stop_tiles(m_grid, stopped, walk.planned.leg.direction);
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
//  the leg could have stopped. The next step's scene (RouteWalkPlan::scene_next)
//  is not fought: the walk ends there, its dialogue open, for that step.
LegNext RouteWalker::unknown_trainer(LegWalk& walk, const std::vector<std::vector<TilePos>>& paths){
    std::vector<TilePos> anywhere;
    const size_t moved = std::min(walk.planned.leg.tiles, walk.result.moved_chunks + walk.turn_moved);
    if (walk.result.stop == WalkStop::DIALOG && moved > 0){
        for (const TilePos& pos : m_from){
            const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, moved);
            if (!path.empty()){
                anywhere.push_back(path.back());
            }
        }
    }
    if (anywhere.empty()){
        anywhere = m_from;
        for (const std::vector<TilePos>& path : paths){
            anywhere.insert(anywhere.end(), path.begin(), path.end());
        }
    }
    anywhere = sorted_unique(std::move(anywhere));
    if (m_plan.scene_next){
        m_from = std::move(anywhere);
        m_io.note(walk.label + ": the next step's scene started on the way; handing it over.", RouteNote::WARNING);
        return LegNext::SCENE;
    }
    count_unexplained_talk(walk.label, anywhere);
    m_io.note(m_plan.label + ": a trainer the map lacks stopped us; fighting.", RouteNote::STATUS);
    const bool battled = m_trainers.fight("unknown trainer");
    if (heard_out_a_talk(walk.label) || !battled){
        if (!walk.planned.leg.ends_at_wall && anywhere.size() > 1){
            anywhere = open_leg_talk_tiles(walk);
        }
        return replan_after_talk(walk, std::move(anywhere));
    }
    if (walk.planned.leg.ends_at_wall){
        return on_to_wall(walk);
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
    m_io.note(std::format("{}{}", ROUTE_LOG_TAG, route_trainer_line(m_plan.label, trainer.name, route_tiles(m_maps, stopped))), RouteNote::LOG);
    m_io.note("Fighting " + trainer.name + ".", RouteNote::STATUS);
    m_trainers.fight(trainer.name);
    m_trainers.sights.erase(m_trainers.sights.begin() + watching[0]);
    //  Their sight line only says roughly where: BDSP's trainers stand a few
    //  tiles from Platinum's (r242a). A leg to a wall walks on to its wall.
    if (walk.planned.leg.ends_at_wall){
        return on_to_wall(walk);
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
//  the side starts the battle this way. Whoever talks is a person.
Blocker RouteWalker::talk_to_blocker(const std::string& leg_label){
    m_io.press(press_a(RouteInputEnd::QUEUED));
    if (m_io.dialog_within(2000ms, COLOR_YELLOW)){
        m_io.note(leg_label + ": someone is in the way; talking to them.", RouteNote::STATUS);
        m_trainers.fight("someone in the way");
        return Blocker::PERSON;
    }
    return Blocker::THING;
}

//  A scene the map lacks (r14: Barry's Route 201 warning, then the row that
//  pulls us back) would replan into it forever: give up after the second,
//  or the sixth by the greeter (RouteWalkPlan::greeter), who talks each time.
void RouteWalker::count_unexplained_talk(const std::string& leg_label, const std::vector<TilePos>& near){
    if (++m_unexplained_talks > (m_plan.greeter_near(near) ? 6u : 2u)){
        fail(m_io, std::format("{}: dialogue the map does not explain stopped us {} times.", leg_label, m_unexplained_talks));
    }
}

//  After the fight with whoever stopped us: a dialogue still showing, or
//  back within 5 s, is a talk no battle followed (00.05c's Wonder Station
//  woman, lab-B 2026-11-12 04:58: 2 s of B left her explanation on screen).
//  Presses through it again while it shows, until the overworld is quiet
//  5 s. Returns whether there was such a talk.
bool RouteWalker::heard_out_a_talk(const std::string& leg_label){
    size_t presses = 0;
    while (m_io.dialog_within(HEAR_OUT_QUIET, COLOR_YELLOW)){
        if (++presses > HEAR_OUT_PRESSES){
            fail(m_io, std::format("{}: a talk no battle followed still showed after {} press-throughs.", leg_label, HEAR_OUT_PRESSES));
        }
        m_io.note(leg_label + ": no battle followed the talk; hearing it out.", RouteNote::STATUS);
        m_trainers.fight("someone talking");
    }
    return presses > 0;
}

//  Where a talk stopped a leg to a wall: between `walked_tiles` on, if
//  counted (SIZE_MAX if not), and the wall.
std::vector<TilePos> RouteWalker::talk_stop_tiles(const LegWalk& walk, size_t walked_tiles) const{
    std::vector<TilePos> stopped;
    for (const TilePos& pos : m_from){
        const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, SIZE_MAX);
        const size_t first = walked_tiles == SIZE_MAX ? 0 : std::min(walked_tiles, path.size());
        if (first == 0){
            stopped.push_back(pos);
        }
        stopped.insert(stopped.end(), path.begin() + (first == 0 ? 0 : first - 1), path.end());
    }
    return stopped;
}

//  Where a talk no battle followed stopped a tapped leg whose taps were not
//  counted: any tile from where it began to where it ends, none past it. A
//  talk freezes us where we stand and the taps after it move nobody; unlike
//  a trainer's, it has no walk over to wait out (after_leg()'s extra tile).
std::vector<TilePos> RouteWalker::open_leg_talk_tiles(const LegWalk& walk) const{
    std::vector<TilePos> stopped = m_from;
    for (const TilePos& pos : m_from){
        const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, walk.planned.leg.tiles);
        stopped.insert(stopped.end(), path.begin(), path.end());
    }
    return sorted_unique(std::move(stopped));
}

//  After a talk no battle followed, from the tiles it may have stopped us
//  on: whoever talked may stand on the tile ahead, open on our map, so it
//  comes off the map (one tile only), and the walk plans again from there
//  instead of walking on into them.
LegNext RouteWalker::replan_after_talk(const LegWalk& walk, std::vector<TilePos> stopped){
    stopped = sorted_unique(std::move(stopped));
    if (stopped.size() == 1 && slide_to_wall(m_grid, stopped[0], walk.planned.leg.direction).tiles > 0){
        mark_blocker(walk, stopped, Blocker::PERSON);
    }
    return replan_from(walk, std::move(stopped), "heard out a talk no battle followed");
}

//  What stopped us on one tile is on the next; a person's comes off our map: no wall (00.05c).
void RouteWalker::mark_blocker(const LegWalk& walk, const std::vector<TilePos>& stopped, Blocker blocker){
    if (stopped.size() != 1){
        return;
    }
    const TilePos pos = stopped[0].next(walk.planned.leg.direction);
    const bool person = blocker == Blocker::PERSON;
    person ? m_grid.uncover(pos) : m_grid.block(pos);
    m_io.note(std::format("[Route] {}: {} the map lacks is at {}; marked it{}", walk.label, person ? "someone" : "something",
        describe(m_maps, pos), person ? " (no wall to stop against)." : "."), RouteNote::WARNING);
}

//  A walk that never moved from several possible tiles: whatever stopped it
//  stands just past one of them, so mark the tiles just past them all
//  (but the goal and the tiles we may stand on), or the next plan takes the
//  same leg into it again. On lab-B (2026-11-11, 00.05) "east 7 (to wall)"
//  from 15 possible tiles below Jubilife's Center walked 0 tiles in five
//  plans in a row, and the route gave up on "too many plans".
void RouteWalker::mark_tiles_past(const LegWalk& walk, const std::vector<TilePos>& stopped){
    if (stopped.size() < 2){
        return;
    }
    std::vector<TilePos> marked;
    for (const TilePos& pos : stopped){
        const std::optional<TilePos> next = step(m_grid, pos, walk.planned.leg.direction);
        if (!next || m_plan.goal(*next) || std::find(stopped.begin(), stopped.end(), *next) != stopped.end()){
            continue;
        }
        m_grid.block(*next);
        marked.push_back(*next);
    }
    if (!marked.empty()){
        m_io.note("[Route] " + walk.label + ": nothing moved from " + std::to_string(stopped.size()) +
            " possible tiles; marked the tiles past them: " + describe(m_maps, sorted_unique(std::move(marked))) + ".", RouteNote::WARNING);
    }
}

//  A walk to the wall held up on the way (held_up()): it may have stopped on
//  any tile of the leg, its first too if no chunk moved or it is at the wall,
//  and plans again from all of them (a leg to a wall brings back one tile).
LegNext RouteWalker::held_up_on_the_way(const LegWalk& walk, size_t nearest){
    std::vector<TilePos> stopped;
    for (const TilePos& pos : m_from){
        //  At its wall, a start tile stands there whatever moved (lab-B 2026-11-12 14:50).
        const bool at_wall = slide_to_wall(m_grid, pos, walk.planned.leg.direction).tiles == 0;
        if (walk.result.moved_chunks == 0 || at_wall){
            stopped.push_back(pos);
        }
        if (at_wall && walk.result.moved_chunks > 0){
            m_io.note(std::format("[Route] {}: {} is at its wall; {} moved chunks include a turn.", walk.label,
                describe(m_maps, pos), walk.result.moved_chunks), RouteNote::WARNING);
        }
        const std::vector<TilePos> path = leg_path(m_grid, pos, walk.planned.leg.direction, nearest);
        stopped.insert(stopped.end(), path.begin(), path.end());
    }
    stopped = sorted_unique(std::move(stopped));
    if (stopped.empty()){
        fail(m_io, walk.label + ": held up on the way, and no tile fits.");
    }
    return replan_from(walk, std::move(stopped), std::format("held up on the way ({} ms for {} tiles to the wall)",
        walk.result.walked.count(), nearest));
}

LegNext RouteWalker::replan_from(const LegWalk& walk, std::vector<TilePos> stopped, const std::string& why){
    m_io.note(std::format("{}{}", ROUTE_LOG_TAG, route_replan_line(m_plan.label, route_log_leg(walk.planned.leg), why, route_tiles(m_maps, stopped))), RouteNote::WARNING);
    m_facing = walk.planned.leg.direction;
    m_from = std::move(stopped);
    return LegNext::REPLAN;
}

} // namespace

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
    RouteEnd end = walk_route_to_warp(io, maps, std::move(from), trainers, plan);
    if (end.warp.any()){
        fail(io, plan.label + ": unexpected transition.");
    }
    return std::move(end.tiles);
}

RouteEnd walk_route_to_warp(
    RouteIO& io, const StoryWorld& maps, std::vector<TilePos> from,
    RouteTrainers& trainers, const RouteWalkPlan& plan
){
    return RouteWalker(io, maps, trainers, plan).run(std::move(from));
}


namespace{

//  Waits for the way at most this often in one counted leg: Oreburgh's
//  Hiker tries a step every 2 s, and may stay or keep to the column.
constexpr size_t COUNTED_WALL_WAITS = 8;

//  A count by time is off by a tile or so a stop. Short of the wall by more
//  than WAIT_SHORT_OF is someone in the way (Oreburgh's Hiker stops us 4
//  to 7 tiles short of the Center's wall); past COUNT_SLACK either way at
//  the end, the walk is not where the step has us.
constexpr size_t COUNTED_WALL_WAIT_SHORT_OF = 2;
constexpr size_t COUNTED_WALL_COUNT_SLACK = 3;

//  One hold of `leg` to the wall; fails on anything but a stop or the time.
WalkResult hold_counted_wall_leg(RouteIO& io, const CountedWallLeg& leg){
    const WalkResult result = io.walk_to_wall({.direction = leg.direction,
        .limit = wall_limit({.direction = leg.direction, .tiles = leg.most}), .label = leg.label,
        .options = {.arrived = nullptr, .pace = DpadPace::RUN}});
    if (result.stop != WalkStop::BLOCKED && result.stop != WalkStop::LIMIT){
        fail(io, leg.label + ": " + walk_stop_name(result.stop) + " instead of a stop.");
    }
    if (result.wild_battles > 0){
        fail(io, leg.label + ": a wild battle on the way; the count of tiles is lost.");
    }
    return result;
}

} // namespace

size_t walk_counted_wall_leg(RouteIO& io, const CountedWallLeg& leg){
    const Milliseconds per_tile = route_ms_per_tile(leg.direction, leg.speed);
    const size_t slack = COUNTED_WALL_COUNT_SLACK;
    size_t walked = 0;
    for (size_t waits = 0;; waits++){
        const WalkResult result = hold_counted_wall_leg(io, leg);
        walked += tiles_moved(result, per_tile);
        if (result.stop == WalkStop::LIMIT){
            //  The stop went unseen: we must be held against the wall.
            if (!io.holding_against_wall(leg.direction, leg.label + ": at the wall?")){
                fail(io, leg.label + ": ran out of time after about " + std::to_string(walked) + " tiles, short of the wall.");
            }
            break;
        }
        if (walked + COUNTED_WALL_WAIT_SHORT_OF >= leg.fewest || waits == COUNTED_WALL_WAITS){
            break;
        }
        io.note("[Route] " + leg.label + ": stopped after about " + std::to_string(walked) +
            " tiles, short of the wall; waiting for the way to clear.", RouteNote::WARNING);
        io.press(WAIT_FOR_THE_WAY);
    }
    io.note(std::format("[Route] {}: about {} tiles to the wall; the map has {} to {}.", leg.label, walked, leg.fewest, leg.most),
        RouteNote::LOG);
    if (walked + slack < leg.fewest || walked > leg.most + slack){
        fail(io, std::format("{}: about {} tiles to the wall, where the map has {} to {}: not where the step has us.",
            leg.label, walked, leg.fewest, leg.most));
    }
    return walked;
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
