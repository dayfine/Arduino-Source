/*  AutoStory Fault Sweep Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each class of failure console B showed, injected at every row of the
 *  story on the overworld sim, instead of only where it was seen:
 *
 *  - SCENE_END_POSE: a step the sim does not play (a scene, a talk, an
 *    event) that moved the player leaves them on another walkable tile
 *    within one of the table's pose, while the program goes on from the
 *    table's (BDSP 01.04, lab-B 2026-11-09).
 *  - WILD_BATTLES: wild battles at the game's own odds on every map with
 *    land encounters, over GAME_ODDS_SEEDS seeds (08.01's Rock Tunnel).
 *  - LATE_FADE: every warp's fade ends LATE_FADE later than the sim's
 *    usual (07.02's warp to Cerulean City).
 *  - SLOW_FADE_IN: after every warp's fade the new map fades in for
 *    SLOW_FADE_IN, the game taking no input yet: a fixed wait after a
 *    fade, then a move (BDSP 00.04b heal_sandgem, lab-B 2026-11-10).
 *  - DARK_CAVE: the caves without Flash dark (DARK_MAPS), so each
 *    battle there ends on the dark cave's overworld, with no wild
 *    battles and with them at the game's odds (08.01 hiker_lenny, lab-B
 *    2026-11-09).
 *  - LOST_TAP: a wild battle on each tile of each step into a trainer's
 *    line before its last, so the next tap comes as the overworld comes
 *    back, inside the lockout after a battle (08.01 winston_line, console
 *    B, 2026-11-09 15:50). The step alone, from where the clean run took
 *    it, on its map made a wild floor.
 *  - WARP_NO_OP: the warps of each map the row warps from do nothing,
 *    no fade and no move, so the step that walks onto the first of them
 *    must stop there, not go on as if warped (09.01's B4F stairs, console
 *    B, 2026-11-10, reported as walked to B3F).
 *  - UNSEEN_STEP: each step tap that moved the player, one at a time, read
 *    by the walker as not moved: it must not tap again into a second
 *    tile (09.02 Rocket Hideout B2F, console B, 2026-11-11 15:03 and 15:36).
 *  - MOUNT_LOST: rows that ride, with ride_bike played on the sim, each
 *    leg counted out on the Bicycle, one at a time, starting with the
 *    player off it: the walker must get back on, not replan its legs on
 *    foot at the Bicycle's taps (11.01, console B, 2026-11-12 05:42).
 *
 *  Not swept here: a missed fade, which needs a knob in OverworldSim; a
 *  battle's opening at a step's start, which needs one in OverworldStepSim;
 *  and the step tap lockout between two taps, which step_once() waits out.
 *
 *  The sim plays walks, steps, cuts and trainers' battles; after a step it
 *  does not play, the player is put on the table's pose. Each row runs
 *  clean first; a row that fails clean, or has no start, is not swept. A
 *  variant fails when the row then fails or ends elsewhere. A sweep the
 *  program fails today is on KNOWN_FAILING with its likely fix: its test
 *  passes while it still fails, and fails once it passes.
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_HideoutLiftRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSimField.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryHideoutLift.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryFaultSweep_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using namespace std::chrono_literals;

enum class FaultClass{
    SCENE_END_POSE,
    WILD_BATTLES,
    LATE_FADE,
    SLOW_FADE_IN,
    DARK_CAVE,
    LOST_TAP,
    WARP_NO_OP,
    UNSEEN_STEP,
    MOUNT_LOST,
};
std::string_view fault_class_name(FaultClass fault){
    switch (fault){
    case FaultClass::SCENE_END_POSE:
        return "scene end pose";
    case FaultClass::WILD_BATTLES:
        return "wild battles at the game's odds";
    case FaultClass::LATE_FADE:
        return "late warp fade";
    case FaultClass::SLOW_FADE_IN:
        return "slow fade-in after every warp";
    case FaultClass::DARK_CAVE:
        return "dark cave without Flash";
    case FaultClass::LOST_TAP:
        return "lost tap after a battle in a trainer's line";
    case FaultClass::WARP_NO_OP:
        return "warp no-op";
    case FaultClass::UNSEEN_STEP:
        return "step tap read as not moved";
    case FaultClass::MOUNT_LOST:
        return "off the Bicycle at a bike leg";
    }
    return "?";
}

//  Sweeps the program fails today, by test name, with the likely fix.
const std::map<std::string, std::string> KNOWN_FAILING{
    {"PokemonFRLG::AutoStory - Fault sweep: 02.02, wild battles at the game's odds",
        "below_oak on Route 1 gives up after 9 stops part-way (seeds 16, 30): count stops that make no progress, not every battle"},
    {"PokemonFRLG::AutoStory - Fault sweep: 10.06, scene end pose",
        "below_fuji_house plans from (4, 7), where Mr. Fuji's script warps the player (warp 4, 7, exact); from a tile off, no wall in his house funnels the walk onto (3, 4): legs that re-anchor, if the console shows the warp can land elsewhere"},
    {"PokemonFRLG::AutoStory - Fault sweep: 11.01, warp no-op",
        "fly_celadon: a Fly that doesn't leave Lavender Town goes unseen, and east_of_snorlax walks from Lavender to Route 16 by Routes 8 and 7 instead: check the fly lands on its town's tile (FlyTown::landing) before the step ends"},
    {"PokemonFRLG::AutoStory - Fault sweep: 12.03, warp no-op",
        "fly_pallet: a Fly that doesn't leave Fuchsia City goes unseen, and the row, which walks on from Fuchsia's streets to Pallet's shore, ends on Cinnabar's nurse tile: the same fix as 11.01's, the fly landing on its town's tile (FlyTown::landing) checked before the step ends"},
    {"PokemonFRLG::AutoStory - Fault sweep: 14.03, warp no-op",
        "fly_celadon: a Fly that doesn't leave Cinnabar Island goes unseen, and the row, which walks on from Celadon's heal spot, ends below Erika: the same fix as 11.01's, the fly landing on its town's tile (FlyTown::landing) checked before the step ends"},
    {"PokemonFRLG::AutoStory - Fault sweep: 17.02, warp no-op",
        "fly_viridian: a Fly that doesn't leave Saffron City goes unseen, and the row, which walks on from Saffron's streets, ends on Viridian's Gym tile: the same fix as 11.01's, the fly landing on its town's tile (FlyTown::landing) checked before the step ends"},
    {"PokemonFRLG::AutoStory - Fault sweep: 18.01, late warp fade",
        "pad_to_top_right: a pad hop stays on SaffronCity_Gym, so a fade seen late (or a slow fade-in) leaves no scene change to confirm the warp and the step finds no fade: a scene check for a warp within one map (the player's tile moved from the pad), as the walker's scene_confirms_missed_fade does for a leg"},
    {"PokemonFRLG::AutoStory - Fault sweep: 18.02, late warp fade",
        "pad_from_top_right: a pad hop stays on SaffronCity_Gym, so a fade seen late (or a slow fade-in) leaves no scene change to confirm the warp and the step finds no fade: a scene check for a warp within one map (the player's tile moved from the pad), as the walker's scene_confirms_missed_fade does for a leg"},
    {"PokemonFRLG::AutoStory - Fault sweep: 18.02, slow fade-in after every warp",
        "pad_from_top_right: a pad hop stays on SaffronCity_Gym, so a fade seen late (or a slow fade-in) leaves no scene change to confirm the warp and the step finds no fade: a scene check for a warp within one map (the player's tile moved from the pad), as the walker's scene_confirms_missed_fade does for a leg"},
    {"PokemonFRLG::AutoStory - Fault sweep: 08.03, off the Bicycle at a bike leg",
        "riding with no fault, route7_underground_exit's warp to UndergroundPath_EastEntrance is not confirmed on the sim (the screen after the fade reads as before the hold): the sim's warp off the Bicycle, before this class can sweep 08.03"},
};

//  The land encounter rates of the maps the story walks (pokefirered
//  src/data/wild_encounters.json, FireRed). Mt. Moon's floor is wild
//  on every tile (WILD_FLOOR_MAPS).
const std::map<std::string, int> STORY_ENCOUNTER_RATES{
    {"Route1", 21}, {"Route2", 21}, {"Route3", 21}, {"Route4", 21}, {"Route5", 21}, {"Route6", 21},
    {"Route7", 21}, {"Route8", 21}, {"Route9", 21}, {"Route10", 21}, {"Route11", 21}, {"Route22", 21}, {"Route24", 21}, {"Route25", 21},
    {"ViridianForest", 14}, {"MtMoon_1F", 7}, {"MtMoon_B1F", 5}, {"MtMoon_B2F", 7},
    {"RockTunnel_1F", 7}, {"RockTunnel_B1F", 7},
};
//  Caves wild on every floor tile, also where the maps mark no wild tiles
//  (Mt. Moon B2F, console B, 2026-11-03).
const std::set<std::string> WILD_FLOOR_MAPS{"MtMoon_1F", "MtMoon_B1F", "MtMoon_B2F"};
constexpr unsigned GAME_ODDS_SEEDS = 30;
//  The caves the story walks dark: no Flash before Lavender Town.
const std::set<std::string> DARK_MAPS{"RockTunnel_1F", "RockTunnel_B1F"};
//  Seeds of wild battles at the game's odds in the dark, after a run
//  with none.
constexpr unsigned DARK_CAVE_SEEDS = 10;
//  How much later than SimTiming's a late fade ends: the new map shows
//  this long after the usual.
constexpr Milliseconds LATE_FADE = 1500ms;
//  How long the new map fades in after a warp's fade, taking no input: a
//  model, longer than the GBA's fade-in, to find moves made in it.
constexpr Milliseconds SLOW_FADE_IN = 1000ms;

//  The maps as the story after the Hall of Fame and a new game flag them:
//  the same set for every row, as each stage's tests use, with the two
//  lifts' coded rooms (09.02, 10.01).
const std::vector<MapData>& sweep_maps(){
    static const std::vector<MapData> maps = []{
        const std::vector<MapData> all = overworld_maps();
        std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
        flags.merge(map_flag_set(all, "new_game"));
        //  As story_maps(): Route 16's Snorlax gone (11.01's flute).
        flags.insert("FLAG_HIDE_ROUTE_16_SNORLAX");
        std::vector<MapData> flagged = overworld_maps(flags);
        flagged.push_back(rocket_hideout_lift_room());
        flagged.push_back(department_store_lift_room());
        return flagged;
    }();
    return maps;
}

//  `script` with the lifts' floors, for sweep_maps()' lift rooms.
SimScript with_lifts(SimScript script){
    script.lift_floors[std::string(LIFT_MAP)] = rocket_hideout_lift_exits();
    script.lift_floors[std::string(DEPT_STORE_LIFT_MAP)] = department_store_lift_exits();
    return script;
}

//  Whether the overworld sim plays `step` (OverworldStepSim): the rest is stubbed.
bool sim_plays(const StoryStep& step){
    switch (step.kind){
    case StoryStepKind::ROUTE:
    case StoryStepKind::LEGS:
    case StoryStepKind::STEP:
    case StoryStepKind::BATTLE:
        return true;
    case StoryStepKind::GAME:
        //  A Fly lands on its town's landing tile, as the sim plays it; a
        //  lift's warp lets out on its floor's tile (sweep_maps()' lift
        //  rooms, with_lifts()). A boulder's push leaves the player behind
        //  it, as the sim plays it.
        return step.action == CUT_TREE_VARIANT || step.action == PUSH_BOULDER_VARIANT || is_field_move_step(step) || parse_lift_floor_step(step).has_value();
    default:
        return false;
    }
}

//  A row run on the sim.
struct RowRun{
    //  The first failure, "" if none.
    std::string failure;
    //  The steps whose failure was let pass, by index (run_row()).
    std::set<size_t> scripted;
    //  Where the player stood before each step, by index.
    std::map<size_t, MapPosition> before;
    std::map<size_t, Direction> facing_before;
    //  The maps each step warped from, by index.
    std::map<size_t, std::vector<std::string>> warped_from;
    //  The step `failure` is from; none for a violation or a row's end.
    std::optional<size_t> failed_at;
    MapPosition end;
    Milliseconds time{0};
    int steps = 0;
    int wild_battles = 0;
    Milliseconds in_wild_battles{0};
    int moved_step_taps = 0;
    int bike_legs = 0;
};

//  Where a stubbed step leaves the player instead of its pose_after: by step index.
using Landings = std::map<size_t, TilePos>;

//  Whether `map` is one of sweep_maps().
bool map_loaded(const std::string& map){
    const std::vector<MapData>& maps = sweep_maps();
    return std::any_of(maps.begin(), maps.end(), [&](const MapData& data){ return data.name == map; });
}

//  Whether a step of `row` after its step `c` walks.
bool walks_after(const StoryCheckpoint& row, size_t c){
    return std::any_of(row.steps.begin() + c + 1, row.steps.end(), [](const StoryStep& next){
        return next.kind == StoryStepKind::ROUTE || next.kind == StoryStepKind::LEGS || next.kind == StoryStepKind::STEP;
    });
}

//  By the battle's map and name. The Game Corner's Grunt walks off once
//  beaten (CeladonCity_GameCorner_Text_DefeatedGrunt: removeobject).
const std::map<std::pair<std::string, std::string>, std::string> TALK_BATTLE_HIDE_FLAGS{
    {{"CeladonCity_GameCorner", "rocket_grunt"}, "FLAG_HIDE_GAME_CORNER_ROCKET"},
};

//  Whether `failure` is a script the sim does not have: a scene's trigger
//  stepped onto (no trainer sees the player there) or a leader's battle
//  after a talk (no trainer's message up).
bool unscripted(const std::string& failure){
    return failure.find(": no trainer saw the player.") != std::string::npos
        || failure.find(": no_battle.") != std::string::npos;
}

//  Hides from `maps` the trainer who leaves after `step`, a battle a talk
//  starts (talk_battle_hide_flag()). Returns whether there was one.
bool hide_talk_battle_trainer(std::vector<MapData>& maps, const StoryStep& step){
    const std::optional<std::string> hidden = talk_battle_hide_flag(step);
    if (!hidden){
        return false;
    }
    for (MapData& map : maps){
        hide_objects(map, {*hidden});
    }
    return true;
}

//  Row `row`'s step `c` on `sim` from `at`, recorded in `out`. Returns
//  whether it failed as unscripted() and was let pass (run_row()).
bool run_row_step(
    RowRun& out, OverworldSim& sim, StoryPose& at, const StoryCheckpoint& row, size_t c,
    const std::optional<std::set<size_t>>& scripted
){
    out.before.emplace(c, sim.state().position);
    out.facing_before.emplace(c, sim.state().facing);
    const size_t warps_before = sim.state().warped_from.size();
    out.failure = row_step_on_sim(sim, at, row.steps[c], row.id);
    const std::vector<std::string>& warped = sim.state().warped_from;
    out.warped_from[c].assign(warped.begin() + warps_before, warped.end());
    const bool let_pass = unscripted(out.failure) && (!scripted || scripted->contains(c));
    if (let_pass){
        out.failure.clear();
        out.scripted.insert(c);
    }
    if (!out.failure.empty()){
        out.failed_at = c;
    }
    return let_pass;
}

//  ride_bike, which the row's sim stubs, played on `sim` when `script` has
//  the Bicycle registered (MOUNT_LOST).
void play_ride_bike(OverworldSim& sim, const SimScript& script, const StoryStep& step){
    if (script.bike_registered && step.kind == StoryStepKind::GAME && step.action == RIDE_BIKE_VARIANT){
        sim.use_field(SimField::BIKE_SELECT);
    }
}

//  `script` for a new sim after `state`'s, riding if that one was.
SimScript still_riding(SimScript script, const SimState& state){
    script.on_bike = state.on_bike;
    return script;
}

//  Row `row` on the sim with `script`, from its start. After a step the sim
//  does not play, the player is put on its pose_after, or on `landings`'
//  tile for it, the program going on from the pose_after either way. A
//  step that fails as unscripted() is let pass as one the sim does not
//  play: in a clean run (`scripted` unset) at any step, else only at the
//  steps in `scripted`, where the clean run let it pass.
RowRun run_row(
    const StoryCheckpoint& row, const SimScript& row_script, const Landings& landings = {},
    const std::optional<std::set<size_t>>& scripted = std::nullopt
){
    const SimScript script = with_lifts(row_script);
    RowRun out;
    std::optional<MapPosition> off_maps;
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    std::vector<MapData> maps = sweep_maps();
    auto sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, at.facing.value_or(Direction::SOUTH), script);
    const auto add = [&out](const SimState& state){
        out.time += state.time;
        out.steps += state.steps;
        out.wild_battles += state.wild_battles;
        out.in_wild_battles += state.in_wild_battles;
        out.moved_step_taps += state.moved_step_taps;
        out.bike_legs += state.bike_legs;
    };
    for (size_t c = 0; c < row.steps.size() && out.failure.empty(); c++){
        const StoryStep& step = row.steps[c];
        const bool let_pass = run_row_step(out, *sim, at, row, c, scripted);
        play_ride_bike(*sim, script, step);
        if (!out.failure.empty() || (sim_plays(step) && !let_pass) || !step.pose_after){
            continue;
        }
        at = *story_pose(Pokemon::pose_of(*step.pose_after));
        const bool hidden = let_pass && hide_talk_battle_trainer(maps, step);
        const auto landing = landings.find(c);
        const MapPosition land{at.map, landing == landings.end() ? at.tile : landing->second};
        if (!map_loaded(land.map)){
            //  A room with no map (Celadon's Pokemon Center) must end the row.
            off_maps = land;
            out.failure = walks_after(row, c) ? std::format("{}/{}: ends on {}, which has no map, with a walk after it.", row.id, step.name, land.map) : "";
            break;
        }
        if (!(sim->state().position == land) || hidden){
            add(sim->state());
            sim = std::make_unique<OverworldSim>(maps, land, at.facing.value_or(sim->state().facing), still_riding(script, sim->state()));
        }
    }
    if (out.failure.empty() && !sim->state().violations.empty()){
        out.failure = "a violation on the way: " + sim->state().violations.back();
    }
    out.end = off_maps.value_or(sim->state().position);
    add(sim->state());
    return out;
}

//  Why `run` differs from the clean run `clean`, or "".
std::string differs(const RowRun& run, const RowRun& clean){
    if (!run.failure.empty()){
        return run.failure;
    }
    if (!(run.end == clean.end)){
        return std::format("ended on {}, not {}.", to_string(run.end), to_string(clean.end));
    }
    return "";
}

//  One injected variant that failed its row.
struct SweepFailure{
    std::string step;
    std::string variant;
    std::string stopped;
};

UnitTestResult sweep_result(Logger& logger, FaultClass fault, const std::string& row, size_t variants, const std::vector<SweepFailure>& failures){
    logger.log(std::format("Fault sweep {} {}: {} variants, {} fail.", row, fault_class_name(fault), variants, failures.size()));
    if (failures.empty()){
        return true;
    }
    std::string out = std::format("{} of {} variants fail {}:", failures.size(), variants, row);
    for (const SweepFailure& failure : failures){
        logger.log(std::format("| {} | {} | {} | {} | {} |", row, failure.step, fault_class_name(fault), failure.variant, failure.stopped));
        out += std::format("\n  {} {}: {}", failure.step, failure.variant, failure.stopped);
    }
    return out;
}

//  The open tiles within one of `pose`, diagonals too: where a scene can
//  leave the player instead.
std::vector<TilePos> open_neighbors(const StoryPose& pose){
    const std::vector<MapData>& maps = sweep_maps();
    const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapData& data){ return data.name == pose.map; });
    std::vector<TilePos> out;
    if (map == maps.end()){
        return out;
    }
    for (int dy = -1; dy <= 1; dy++){
        for (int dx = -1; dx <= 1; dx++){
            const TilePos tile{pose.tile.x + dx, pose.tile.y + dy};
            const bool open = (dx != 0 || dy != 0) && in_bounds(*map, tile) && object_at(*map, tile) == nullptr
                && (tile_at(*map, tile) == TileKind::OPEN || tile_at(*map, tile) == TileKind::TALL_GRASS);
            if (open){
                out.push_back(tile);
            }
        }
    }
    return out;
}

//  Row `row`'s stubbed steps that moved the player and have a walk after
//  them, each ending on every open tile next to the table's pose in turn.
UnitTestResult sweep_scene_end_pose(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    std::vector<SweepFailure> failures;
    size_t variants = 0;
    for (size_t c = 0; c < row.steps.size(); c++){
        const StoryStep& step = row.steps[c];
        const std::optional<StoryPose> after = step.pose_after ? story_pose(Pokemon::pose_of(*step.pose_after)) : std::nullopt;
        //  A scene that only turns the player (the S.S. Ticket's, the aide's)
        //  ends where our walk did: not the game's end pose.
        const auto from = clean.before.find(c);
        //  With no walk after it, nothing in the row goes on from the pose.
        const bool walk_after = walks_after(row, c);
        if (sim_plays(step) || !walk_after || !after || (from != clean.before.end() && from->second == MapPosition{after->map, after->tile})){
            continue;
        }
        for (const TilePos& tile : open_neighbors(*after)){
            variants++;
            const std::string stopped = differs(run_row(row, SimScript{}, {{c, tile}}, clean.scripted), clean);
            if (!stopped.empty()){
                failures.push_back({step.name, std::format("ends on {}", to_string(tile)), stopped});
            }
        }
    }
    return sweep_result(logger, FaultClass::SCENE_END_POSE, row.id, variants, failures);
}

//  The mean of `values`, "-" if none.
std::string mean_of(const std::vector<double>& values){
    if (values.empty()){
        return "-";
    }
    return std::format("{:.1f}", std::accumulate(values.begin(), values.end(), 0.) / values.size());
}

//  Row `row` with wild battles at the game's odds, from each seed. Logs
//  passed, time, tiles walked again and time lost per battle.
UnitTestResult sweep_wild_battles(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    std::vector<SweepFailure> failures;
    std::vector<double> seconds, again, lost;
    int battles = 0;
    for (unsigned seed = 1; seed <= GAME_ODDS_SEEDS; seed++){
        SimScript script;
        script.encounter_rates = STORY_ENCOUNTER_RATES;
        script.encounter_seed = seed;
        script.wild_floor_maps = WILD_FLOOR_MAPS;
        const RowRun run = run_row(row, script, {}, clean.scripted);
        battles += run.wild_battles;
        const std::string stopped = differs(run, clean);
        if (!stopped.empty()){
            failures.push_back({"", std::format("seed {}", seed), stopped});
            continue;
        }
        seconds.push_back(run.time.count() / 1000.);
        again.push_back(run.steps - clean.steps);
        if (run.wild_battles > 0){
            lost.push_back((run.time - clean.time - run.in_wild_battles).count() / 1000. / run.wild_battles);
        }
    }
    logger.log(std::format(
        "Fault sweep {} wild battles: passed {}/{}, {} battles; of those passed: time (s) mean {}, clean {:.1f}; tiles walked again mean {}; s lost per battle mean {}",
        row.id, GAME_ODDS_SEEDS - failures.size(), GAME_ODDS_SEEDS, battles, mean_of(seconds), clean.time.count() / 1000., mean_of(again), mean_of(lost)
    ));
    return sweep_result(logger, FaultClass::WILD_BATTLES, row.id, GAME_ODDS_SEEDS, failures);
}

//  Row `row` with every warp's fade ending LATE_FADE late.
UnitTestResult sweep_late_fade(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    SimScript script;
    script.timing.fade += LATE_FADE;
    const std::string stopped = differs(run_row(row, script, {}, clean.scripted), clean);
    std::vector<SweepFailure> failures;
    if (!stopped.empty()){
        failures.push_back({"", std::format("every fade {} ms late", LATE_FADE.count()), stopped});
    }
    return sweep_result(logger, FaultClass::LATE_FADE, row.id, 1, failures);
}

//  Row `row` with every warp's new map fading in for SLOW_FADE_IN.
UnitTestResult sweep_slow_fade_in(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    SimScript script;
    script.timing.fade_in = SLOW_FADE_IN;
    const std::string stopped = differs(run_row(row, script, {}, clean.scripted), clean);
    std::vector<SweepFailure> failures;
    if (!stopped.empty()){
        failures.push_back({"", std::format("every warp fading in {} ms", SLOW_FADE_IN.count()), stopped});
    }
    return sweep_result(logger, FaultClass::SLOW_FADE_IN, row.id, 1, failures);
}

//  Row `row` with DARK_MAPS dark: once with no wild battles, then with
//  them at the game's odds from DARK_CAVE_SEEDS seeds. Each trainer's
//  battle there ends on the dark overworld (battle_end_on_sim() in
//  PokemonFRLG_AutoStoryRowSim.cpp); wild ones are run from, after which
//  nothing is held for an evolution. A row that walks no dark cave is
//  not swept.
UnitTestResult sweep_dark_cave(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    const bool walks_dark = DARK_MAPS.contains(clean.end.map) || std::any_of(clean.before.begin(), clean.before.end(), [](const auto& before){
        return DARK_MAPS.contains(before.second.map);
    });
    if (!walks_dark){
        logger.log(std::format("Fault sweep {}: not swept, the row walks no dark cave.", row.id));
        return true;
    }
    std::vector<SweepFailure> failures;
    for (unsigned seed = 0; seed <= DARK_CAVE_SEEDS; seed++){
        SimScript script;
        script.dark_maps = DARK_MAPS;
        if (seed > 0){
            script.encounter_rates = STORY_ENCOUNTER_RATES;
            script.encounter_seed = seed;
            script.wild_floor_maps = WILD_FLOOR_MAPS;
        }
        const std::string stopped = differs(run_row(row, script, {}, clean.scripted), clean);
        if (!stopped.empty()){
            failures.push_back({"", seed == 0 ? std::string("no wild battles") : std::format("seed {}", seed), stopped});
        }
    }
    return sweep_result(logger, FaultClass::DARK_CAVE, row.id, DARK_CAVE_SEEDS + 1, failures);
}

//  How many tiles row `row`'s step `c`, into a trainer's line, steps onto
//  before its last, in the clean run `clean`.
int line_tiles_before_last(const StoryCheckpoint& row, const RowRun& clean, size_t c){
    const auto from = clean.before.find(c);
    const auto next = clean.before.find(c + 1);
    if (from == clean.before.end()){
        return 0;
    }
    const MapPosition last = next == clean.before.end() ? clean.end : next->second;
    const Direction direction = Pokemon::story_arg<Direction>(row.steps[c].args, "direction");
    TilePos tile = from->second.point.next(direction);
    int out = 0;
    for (; out + 1 < TRAINER_LINE_TILES && last.map == from->second.map && !(tile == last.point); out++){
        tile = tile.next(direction);
    }
    return out;
}

//  Row `row`'s step `c`, into a trainer's line, alone from where the clean
//  run `clean` took it, with a wild battle on its `battle_step`th step: why
//  it fails or ends off the clean run's tile, or "".
std::string line_with_battle(const StoryCheckpoint& row, const RowRun& clean, size_t c, int battle_step){
    const MapPosition from = clean.before.at(c);
    const auto next = clean.before.find(c + 1);
    const MapPosition last = next == clean.before.end() ? clean.end : next->second;
    SimScript script;
    script.wild_floor_maps = {from.map};
    script.encounters = {SimEncounter{.grass_step = battle_step}};
    OverworldSim sim(sweep_maps(), from, clean.facing_before.at(c), script);
    StoryPose at{from.map, from.point, clean.facing_before.at(c)};
    const std::string failure = row_step_on_sim(sim, at, row.steps[c], row.id);
    if (!failure.empty()){
        return failure;
    }
    if (!(sim.state().position == last)){
        return std::format("ended on {}, not {}.", to_string(sim.state().position), to_string(last));
    }
    return "";
}

//  Row `row` with a wild battle on each tile of each step into a trainer's
//  line before its last, one at a time: the tap after it must not be lost.
UnitTestResult sweep_lost_tap(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    std::vector<SweepFailure> failures;
    size_t variants = 0;
    for (size_t c = 0; c < row.steps.size(); c++){
        const StoryStep& step = row.steps[c];
        if (step.kind != StoryStepKind::STEP || step.action != TRAINER_LINE_VARIANT || clean.scripted.contains(c)){
            continue;
        }
        const int tiles = line_tiles_before_last(row, clean, c);
        for (int battle_step = 1; battle_step <= tiles; battle_step++){
            variants++;
            if (const std::string stopped = line_with_battle(row, clean, c, battle_step); !stopped.empty()){
                failures.push_back({step.name, std::format("a wild battle on step {}", battle_step), stopped});
            }
        }
    }
    return sweep_result(logger, FaultClass::LOST_TAP, row.id, variants, failures);
}

//  The steps of the clean run `clean` of `row` that warp, by index: the
//  first from each map, with that map.
std::vector<std::pair<size_t, std::string>> first_warps_by_map(const StoryCheckpoint& row, const RowRun& clean){
    std::vector<std::pair<size_t, std::string>> out;
    std::set<std::string> maps;
    for (size_t c = 0; c < row.steps.size(); c++){
        const auto warped = clean.warped_from.find(c);
        if (warped == clean.warped_from.end()){
            continue;
        }
        for (const std::string& map : warped->second){
            if (maps.insert(map).second){
                out.emplace_back(c, map);
            }
        }
    }
    return out;
}

//  Row `row` with the warps of each map it warps from doing nothing, one
//  map at a time: the step onto the first of them must stop the row.
UnitTestResult sweep_warp_no_op(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    std::vector<SweepFailure> failures;
    const std::vector<std::pair<size_t, std::string>> warps = first_warps_by_map(row, clean);
    for (const auto& [c, map] : warps){
        SimScript script;
        script.dead_warp_maps = {map};
        const RowRun run = run_row(row, script, {}, clean.scripted);
        if (run.failed_at == c){
            continue;
        }
        const std::string variant = "every warp on " + map + " does nothing";
        if (run.failure.empty()){
            failures.push_back({row.steps[c].name, variant, std::format("not stopped: the row went on and ended on {}.", to_string(run.end))});
        }else{
            failures.push_back({row.steps[c].name, variant, std::format("stopped at {}, not here: {}", run.failed_at ? row.steps[*run.failed_at].name : "the row's end", run.failure)});
        }
    }
    return sweep_result(logger, FaultClass::WARP_NO_OP, row.id, warps.size(), failures);
}

//  Row `row` with each step tap that moved the player read as not moved,
//  one at a time, counted across the row's sims.
UnitTestResult sweep_unseen_step(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    std::vector<SweepFailure> failures;
    for (int tap = 1; tap <= clean.moved_step_taps; tap++){
        SimScript script;
        script.unseen_step_taps = {tap};
        const std::string stopped = differs(run_row(row, script, {}, clean.scripted), clean);
        if (!stopped.empty()){
            failures.push_back({"", std::format("step tap {} read as not moved", tap), stopped});
        }
    }
    return sweep_result(logger, FaultClass::UNSEEN_STEP, row.id, (size_t)clean.moved_step_taps, failures);
}

//  Row `row` riding: the Bicycle registered and ride_bike played on the
//  sim (run_row()). Each leg counted out on the Bicycle, one at a time,
//  starts with the player off it, counted across the row's sims. A row
//  that rides no such leg is not swept.
UnitTestResult sweep_mount_lost(Logger& logger, const StoryCheckpoint& row){
    SimScript riding;
    riding.bike_registered = true;
    const RowRun clean = run_row(row, riding);
    if (clean.bike_legs == 0){
        logger.log(std::format("Fault sweep {}: not swept, the row rides no leg counted out on the Bicycle.", row.id));
        return true;
    }
    std::vector<SweepFailure> failures;
    if (!clean.failure.empty()){
        failures.push_back({"", "riding, no fault", clean.failure});
        return sweep_result(logger, FaultClass::MOUNT_LOST, row.id, 0, failures);
    }
    for (int leg = 1; leg <= clean.bike_legs; leg++){
        SimScript script = riding;
        script.off_bike_at_legs = {leg};
        const std::string stopped = differs(run_row(row, script, {}, clean.scripted), clean);
        if (!stopped.empty()){
            failures.push_back({"", std::format("bike leg {} starts off the Bicycle", leg), stopped});
        }
    }
    return sweep_result(logger, FaultClass::MOUNT_LOST, row.id, (size_t)clean.bike_legs, failures);
}

//  A fault class swept over row `row` (empty in FAULT_SWEEP_CASES: each
//  row with a start).
struct FaultSweepInput{
    FaultClass fault = FaultClass::SCENE_END_POSE;
    std::string row = {};
};
//  Whether a sweep is expected to fail is KNOWN_FAILING's, by name.
struct FaultSweepExpectation{
};
struct FaultSweepCase{
    FaultSweepInput input;
    FaultSweepExpectation expectation;
};

//  The table's row `id`.
const StoryCheckpoint& table_row(const std::string& id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No row " + id);
}

class Test_AutoStoryFaultSweep : public UnitTest{
public:
    explicit Test_AutoStoryFaultSweep(FaultSweepCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Fault sweep: {}, {}", test_case.input.row, fault_class_name(test_case.input.fault)))
        , m_fault(test_case.input.fault)
        , m_row(table_row(test_case.input.row))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        UnitTestResult result;
        try{
            result = sweep(logger);
        }catch (Exception& e){
            result = std::string(e.name()) + ": " + e.message();
        }
        const auto known = KNOWN_FAILING.find(name());
        if (known == KNOWN_FAILING.end()){
            return result;
        }
        if (result.result == UnitTestResult::PASSED){
            return "on KNOWN_FAILING (" + known->second + ") but passes now: take it off the list.";
        }
        logger.log("Known failing until " + known->second + ": " + result.message);
        return true;
    }

private:
    UnitTestResult sweep(Logger& logger) const{
        const RowRun clean = run_row(m_row, SimScript{});
        if (!clean.failure.empty()){
            logger.log(std::format("Fault sweep {}: not swept, the row fails clean: {}", m_row.id, clean.failure));
            return true;
        }
        switch (m_fault){
        case FaultClass::SCENE_END_POSE:
            return sweep_scene_end_pose(logger, m_row, clean);
        case FaultClass::WILD_BATTLES:
            return sweep_wild_battles(logger, m_row, clean);
        case FaultClass::LATE_FADE:
            return sweep_late_fade(logger, m_row, clean);
        case FaultClass::SLOW_FADE_IN:
            return sweep_slow_fade_in(logger, m_row, clean);
        case FaultClass::DARK_CAVE:
            return sweep_dark_cave(logger, m_row, clean);
        case FaultClass::LOST_TAP:
            return sweep_lost_tap(logger, m_row, clean);
        case FaultClass::WARP_NO_OP:
            return sweep_warp_no_op(logger, m_row, clean);
        case FaultClass::UNSEEN_STEP:
            return sweep_unseen_step(logger, m_row, clean);
        case FaultClass::MOUNT_LOST:
            return sweep_mount_lost(logger, m_row);
        }
        return true;
    }

    FaultClass m_fault;
    const StoryCheckpoint& m_row;
};

} // namespace


const FaultSweepCase FAULT_SWEEP_CASES[] = {
    {.input = {.fault = FaultClass::SCENE_END_POSE}, .expectation = {}},
    {.input = {.fault = FaultClass::WILD_BATTLES}, .expectation = {}},
    {.input = {.fault = FaultClass::LATE_FADE}, .expectation = {}},
    {.input = {.fault = FaultClass::SLOW_FADE_IN}, .expectation = {}},
    {.input = {.fault = FaultClass::DARK_CAVE}, .expectation = {}},
    {.input = {.fault = FaultClass::LOST_TAP}, .expectation = {}},
    {.input = {.fault = FaultClass::WARP_NO_OP}, .expectation = {}},
    {.input = {.fault = FaultClass::UNSEEN_STEP}, .expectation = {}},
    {.input = {.fault = FaultClass::MOUNT_LOST}, .expectation = {}},
};

std::optional<std::string> talk_battle_hide_flag(const StoryStep& step){
    if (step.kind != StoryStepKind::BATTLE || !step.pose_after){
        return std::nullopt;
    }
    const auto found = TALK_BATTLE_HIDE_FLAGS.find({Pokemon::grid_pose_of(*step.pose_after).map, step.name});
    if (found == TALK_BATTLE_HIDE_FLAGS.end()){
        return std::nullopt;
    }
    return found->second;
}

void add_tests_AutoStoryFaultSweep(UnitTestDatabase& database){
    for (const FaultSweepCase& item : FAULT_SWEEP_CASES){
        for (const StoryCheckpoint& row : story_checkpoints()){
            if (!row.start || !story_pose(Pokemon::pose_of(*row.start))){
                continue;
            }
            database.add<Test_AutoStoryFaultSweep>(FaultSweepCase{.input = {.fault = item.input.fault, .row = row.id}, .expectation = item.expectation});
        }
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
