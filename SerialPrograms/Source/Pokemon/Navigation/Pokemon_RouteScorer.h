/*  Route Scorer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Scores a walk plan on a tile grid: how long it takes on the game's
 *  measured paces, from every tile the player may start on, and whether it
 *  can go wrong. A leg held to a wall ends where the wall stops it; a leg of
 *  taps moves exactly its taps; a held open leg may end a few tiles short or
 *  long, and those tiles must be safe. Interruptions on the way (a wild
 *  battle in grass, someone walking about, a trainer's sight) are charged by
 *  their chance times the recovery they need, and a recovery that can lead
 *  back into itself strands the player: the plan fails.
 *
 *  Game-neutral: the game supplies the step rules, the tile risks, the paces
 *  and, for recoveries, its replanner.
 *
 */

#ifndef PokemonAutomation_Pokemon_RouteScorer_H
#define PokemonAutomation_Pokemon_RouteScorer_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What walking onto a tile risks.
enum class TileRisk{
    NONE,
    ENCOUNTER,      //  tall grass: a wild battle may start
    WANDERER,       //  someone walking about may stand in the way
    SIGHT,          //  a trainer sees the player and walks over
    WARP,           //  a door or warp: walking onto it leaves the map
    TRIGGER,        //  a scene starts here and stops the player
};

//  The grid a plan is scored on.
struct ScoreMap{
    //  Where one step from a tile lands (a ledge hop lands two tiles on), or
    //  nothing if the player is stopped. A warp's tile is landed on.
    std::function<std::optional<TilePos>(const TilePos&, Direction)> step;
    std::function<TileRisk(const TilePos&)> risk;
};

//  How a leg is walked.
enum class ScoredLegKind{
    TO_WALL,        //  held until the player stops: lands exactly
    TAPS,           //  one D-pad tap per tile: lands exactly
    OPEN_HOLD,      //  held for its tiles' time: may land a few tiles off
};
//  One leg of a plan.
struct ScoredLeg{
    Direction direction = Direction::NORTH;
    size_t tiles = 0;
    ScoredLegKind kind = ScoredLegKind::TAPS;
    bool run = false;       //  held at a run (B), not a walk
};
//  "west 16 (taps)", "south 33 (run to wall)".
std::string to_string(const ScoredLeg& leg);

//  The game's measured times.
struct RoutePaces{
    //  Time per tile held in a direction, walking or running.
    std::function<long long(Direction, bool run)> ms_per_tile;
    long long start_ms = 250;       //  from pressing to the first step
    long long tap_ms = 800;         //  one tap and its release
    long long press_ms = 100;       //  a leg's overhead: the turn, the watch
    long long stop_ms = 1000;       //  confirming a stop at a wall (a re-localization)
};

//  How interruptions are charged.
struct InterruptModel{
    double encounter_per_tile = 0.1;
    long long encounter_ms = 12000;     //  a wild battle run from
    double wanderer_per_tile = 0.25;
    long long wanderer_ms = 2500;       //  waiting for them to move on
    long long trainer_ms = 45000;       //  a trainer battle
    long long replan_ms = 2000;         //  a stop and a new plan
    //  A held open leg of n tiles may end open_error + n / open_error_per
    //  tiles short or long.
    int open_error = 1;
    int open_error_per = 5;
    //  Most recoveries in a row before the plan counts as stuck.
    size_t max_recoveries = 3;
    //  Most tiles the player may be on at once, before a plan is too loose.
    size_t max_spread = 32;
};

//  A new plan from where an interruption left the player, or nothing if
//  none exists. Null: a recovery that needs a new plan fails.
using RouteReplanner = std::function<std::optional<std::vector<ScoredLeg>>(const std::vector<TilePos>& from)>;

//  Whether a plan gets there, from every start and through every recovery.
enum class RouteVerdict{
    PASS,
    FAIL,           //  may end off the goal, overshoot onto a hazard, or never arrive
    STRAND,         //  a recovery can lead back into itself
    RETRIES,        //  more recoveries in a row than InterruptModel::max_recoveries
};
//  "PASS", "FAIL", "STRAND", "RETRIES".
const char* verdict_name(RouteVerdict verdict);

//  A plan's score.
struct RouteScore{
    RouteVerdict verdict = RouteVerdict::PASS;
    std::string why;            //  for a verdict other than PASS
    long long nominal_ms = 0;   //  slowest start, nothing interrupting
    double expected_ms = 0;     //  average start, interruptions by their chance
    long long worst_ms = 0;     //  slowest start and the costliest single interruption
    size_t stops = 0;           //  legs that end on a wall: each re-localizes
    size_t presses = 0;         //  legs plus taps
    size_t recoveries = 0;      //  places an interruption may happen
    size_t replans = 0;         //  of those, the ones the plan's own legs cannot absorb
};

//  Scores `legs` walked from any of `from` toward tiles `goal` accepts.
RouteScore score_route(
    const ScoreMap& map, const std::vector<TilePos>& from,
    const std::function<bool(const TilePos&)>& goal,
    const std::vector<ScoredLeg>& legs,
    const RoutePaces& paces, const InterruptModel& model,
    const RouteReplanner& replan = nullptr
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
