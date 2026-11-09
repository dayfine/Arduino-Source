/*  Story Monte Carlo
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs story rows on a game's simulation many times, each time disturbed
 *  the way the console disturbs a run: wild battles at random moments on
 *  encounter tiles, a walk that ends part of a tile off, someone wandering
 *  into the way, a fade or a banner that goes unseen. Each run is one seed
 *  of a pinned list, so a failure found is a failure reproduced: the report
 *  gives, per row, the pass rate and, per failing step, its failure kinds
 *  with a seed that shows each.
 *
 *  Game-agnostic: a game binds it by running one row for one seed's
 *  DisturbancePlan (a SeedRun) on its own simulation.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryMonteCarlo_H
#define PokemonAutomation_Pokemon_StoryMonteCarlo_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A deterministic random stream (splitmix64): the same seed draws the same
//  numbers on every platform.
class SimRandom{
public:
    explicit SimRandom(uint64_t seed = 0)
        : m_state(seed)
    {}

    uint64_t next();
    //  Uniform in [0, 1).
    double uniform();
    //  Uniform in [lo, hi], both inclusive.
    int64_t between(int64_t lo, int64_t hi);
    //  True with probability `p`.
    bool chance(double p);

private:
    uint64_t m_state;
};


//  How hard a run is disturbed. The rates are per tile the player enters.
//  They are measured on lab-B's BDSP runs of rows 00.00 to 01.11, 2026-10-31
//  to 2026-11-08 (its SerialPrograms.log and the one rotated before it, the
//  AutoStory program's lines only), as noted on each.
struct DisturbanceRates{
    //  A wild battle on an encounter tile. Caves: 16 battles in 838 tiles
    //  (the "[Walk] ...: N moved chunks, M wild battle(s)" lines of 01.03's
    //  mine steps, 2026-11-07 and 08; their taps move a tile a chunk). Per
    //  run it is less: 42 battles in the 71 runs that reached the mine, at
    //  the 52 cave tiles the simulation walks there, 1.1%. Grass: 10
    //  battles in about 1,200 tiles, from the battles logged in the steps of
    //  00.03, 00.04d, 01.06 and 01.08 that cross grass, times each step's
    //  runs, times the grass tiles the simulation walks in it (14, 43, 12,
    //  13). Gen IV's rate / 180 (30 and 10 there) drew 8 to 14 battles a
    //  01.03 run; the console met 0 to 2.
    double grass_encounter = 10.0 / 1200;
    double cave_encounter = 16.0 / 838;
    //  Where a stick hold that does not end at a wall ends, along its way:
    //  off by up to this many thousandths of a tile per tile walked,
    //  uniformly. Not measured.
    int drift_per_tile = 150;
    //  The same for D-pad taps, which move whole tiles: 54 runs of 01.03's
    //  above_the_mine ended nine open taps on the tile above the mine's
    //  warp, none on the warp. One open leg of the 466 of more than a tile
    //  went a tile past (11:55 on 2026-11-08); that is not modelled.
    int tap_drift_per_tile = 0;
    //  Off by no more than this, the player still fits a one-tile gap (a
    //  door, a corridor); past it the player covers two tiles. Assumed: a
    //  door frame stopped a player 0.375 tile off (the grid sim's measure).
    int fit = 250;
    //  Someone wandering starts their walk up to this far into it.
    Milliseconds wanderer_start = Milliseconds(10000);
    //  Someone wandering tries a step this often (a fifth of the tries they
    //  stay). Oreburgh's hiker stopped 15 of 54 above_the_mine runs short of
    //  the wall ("stopped after about N tiles"); in 11 the way cleared after
    //  taps or waits, 4 talked to him, 1 failed with no plan. The simulation
    //  at 2000 ms: 17 of 100 seeds stopped, 16 talked, 16 failed; at 1000 ms
    //  16, 14 and 14. The step's pace is not what keeps him in the way.
    Milliseconds wander_step = Milliseconds(2000);
    //  The chance a seed misses each fade or banner its row looks for (it
    //  misses one at most). The door's own watcher missed 13 of 35 fades,
    //  but settle() then saw each arrival: no door step of 01.03 failed for
    //  a fade on 2026-11-08. Over the whole week 9 door steps failed "no
    //  fade within 120 s" in about 230 settles, most of them a step that
    //  had not reached its door; 2 required banners went unread, both
    //  01.03's old out_of_the_mine.
    double missed_detection = 0.02;
    //  The chance a seed drops the hold of one fade its row looks for: a
    //  short fade whose dark or white fell between two frames, so only its
    //  fall and the scene after show. 0 in the rates measured; 1 in the
    //  tests that every seed survives it.
    double missed_hold = 0;
    //  The chance someone walks up and talks to the player before a step
    //  (a dialog between two steps, cleared by the executor's look). Not
    //  measured: 0 in the rates measured, set by the tests that pop them.
    double dialog_between_steps = 0;
};

//  One seed's disturbances: independent random streams for each kind, so a
//  change in how many battles a run draws does not move its drift, and the
//  one detection the run misses, if any (as an index into the row's fades,
//  then its banners, from an undisturbed run), and the one fade whose hold
//  it drops, if any (an index into the row's fades). The hold is drawn
//  from a stream of its own, so a plan's other draws do not move with it.
struct DisturbancePlan{
    uint64_t seed = 0;
    DisturbanceRates rates;
    SimRandom battles;
    SimRandom drift;
    SimRandom wanderers;
    std::optional<size_t> missed_fade;
    std::optional<size_t> missed_banner;
    std::optional<size_t> missed_hold = std::nullopt;
    //  Whether a dialog pops up before each step, drawn last from the
    //  seed, so the other draws do not move with it.
    SimRandom dialogs = SimRandom();
};
//  The plan for `seed`, for a row whose undisturbed run looks for `fades`
//  fades and `banners` banners.
DisturbancePlan make_disturbance_plan(uint64_t seed, const DisturbanceRates& rates, size_t fades, size_t banners);
//  "seed 17: miss fade 1": what a plan draws up front, to reproduce it.
//  A dropped hold reads "fade 2's hold unseen".
std::string describe(const DisturbancePlan& plan);


//  The pinned seeds: the same `count` seeds on every run, the first of a
//  longer list the first of a shorter one.
std::vector<uint64_t> story_seeds(size_t count);
//  The seed count to run: `fallback`, or the environment variable `name`
//  when it holds a positive number (the heavy, on-demand mode).
size_t seed_count(const char* name, size_t fallback);
//  The rows to run: all of `rows`, or those the environment variable
//  `name` lists (comma-separated ids), in `rows`' order.
std::vector<std::string> selected_rows(const char* name, const std::vector<std::string>& rows);


//  What one seed's run of a row did.
struct SeedOutcome{
    bool passed = false;
    //  The step it failed in, and how ("short_leg", "sim_failure: no door
    //  north of (#, #)"): numbers become '#', so runs that failed the same
    //  way group together.
    std::string failed_step;
    std::string kind;
    //  The failure as it was, for the seed that shows it first.
    std::string detail;
};
//  `failure: message` with digit runs as '#', at most `limit` characters.
std::string failure_kind(std::string_view failure, std::string_view message, size_t limit = 90);

//  Runs row `row` once, disturbed by `seed`.
using SeedRun = std::function<SeedOutcome(const std::string& row, uint64_t seed)>;

//  One failure kind of a step: how often, and the first seed that shows it.
struct KindCount{
    size_t runs = 0;
    uint64_t seed = 0;
    std::string detail;
};
//  A row's runs over the seeds.
struct RowReport{
    std::string row;
    size_t runs = 0;
    size_t passes = 0;
    //  By failed step, then by kind.
    std::map<std::string, std::map<std::string, KindCount>> failures;
};
//  Runs `row` once for each of `seeds`.
RowReport run_row_seeds(const std::string& row, const std::vector<uint64_t>& seeds, const SeedRun& run);
//  Runs each of `rows` for each of `seeds`, in order.
std::vector<RowReport> run_seeds(const std::vector<std::string>& rows, const std::vector<uint64_t>& seeds, const SeedRun& run);
//  "01.03: 41/50 passed (82%)", then a line per failing step and kind
//  with its count and seed.
std::string format_report(const RowReport& report);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
