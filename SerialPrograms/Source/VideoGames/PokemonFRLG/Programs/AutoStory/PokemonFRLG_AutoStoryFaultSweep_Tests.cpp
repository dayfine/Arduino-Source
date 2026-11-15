/*  AutoStory Fault Sweep Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each class of failure console B showed (the shared sim faults,
 *  Pokemon_SimFaults.h), injected at every row of the story on the
 *  overworld sim, instead of only where it was seen:
 *
 *  - SCENE_END_POSE: a step the sim does not play (a scene, a talk, an
 *    event) that moved the player leaves them on another walkable tile
 *    within one of the table's pose, while the program goes on from the
 *    table's (BDSP 01.04, lab-B 2026-11-09).
 *  - WILD_BATTLES: wild battles at the game's own odds on every map with
 *    land encounters, over DEFAULT_SWEEP_SEEDS seeds (08.01's Rock Tunnel).
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
 *  clean first, from its start_gates and start_flags: a row that fails
 *  clean fails its "clean run" test and is not swept; a row with no start
 *  is not swept. A variant fails when the row then fails or ends elsewhere. A sweep the
 *  program fails today is on known_failing() with its likely fix: its test
 *  passes while it still fails, and fails once it passes.
 *
 *  The runner (variants, seeds, clean first, the known-failing list) is
 *  the shared one (Pokemon_SimSweep.h, Pokemon_SimSeeds.h,
 *  Pokemon_KnownFailing.h); this file keeps FRLG's rows and how each
 *  class runs on its sim.
 *
 */

#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/Navigation/Simulation/Pokemon_KnownFailing.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimFaults.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimSeeds.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimSweep.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_HideoutLiftRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSimField.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryHideoutLift.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryFaultSweep_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using namespace std::chrono_literals;

using Pokemon::FaultClass;
using Pokemon::fault_class_name;
using Pokemon::SweepVariant;

//  Sweeps the program fails today, by test name, with the likely fix.
const Pokemon::KnownFailing& known_failing(){
    static const Pokemon::KnownFailing list({
        {"PokemonFRLG::AutoStory - Fault sweep: 02.02, wild battles",
            "below_oak on Route 1 gives up after 9 stops part-way (seeds 16, 30): count stops that make no progress, not every battle"},
        {"PokemonFRLG::AutoStory - Fault sweep: 10.06, scene end pose",
            "below_fuji_house plans from (4, 7), where Mr. Fuji's script warps the player (warp 4, 7, exact); from a tile off, no wall in his house funnels the walk onto (3, 4): legs that re-anchor, if the console shows the warp can land elsewhere"},
        {"PokemonFRLG::AutoStory - Fault sweep: 09.01, unseen step",
            "Rocket Hideout's arrow maze: an unseen tap leaves the player on the leg's start or its end, and from what the screen shows the two look alike: a test tap toward a tile blocked from one but not the other tells them apart"},
        {"PokemonFRLG::AutoStory - Fault sweep: 04.05, clean run",
            "route4_east plans no legs from Mt. Moon B2F (14, 8), past the helix gap: the planner's way out of B2F over story_maps(), not yet looked at"},
        {"PokemonFRLG::AutoStory - Fault sweep: 06.02, clean run",
            "below_gym_tree walks onto the gym's door warp and sees no fade (VermilionCity_Gym (4, 12)): the door's warp on the sim after trash_cans, which the sim stubs, not yet looked at"},
        {"PokemonFRLG::AutoStory - Fault sweep: 09.02, clean run",
            "b2f_arrows_to_lift_room moves nothing twice from RocketHideout_B2F (1, 4) after moon_stone, a talk the sim stubs: the arrow tiles' spin from that tile on the sim, not yet looked at"},
        {"PokemonFRLG::AutoStory - Fault sweep: 17.03, clean run",
            "Takashi (10, 2) walks up to (10, 4) when he sees the player on (10, 5) and stays there after the battle, so below_giovanni's way north up column 10 is blocked (moved nothing twice); on the console too: the route after a line battle planned with the trainer who walked up as a wall (with_trainers()), or a way to Takashi from row 3"},
        {"PokemonFRLG::AutoStory - Fault sweep: 08.03, mount lost",
            "riding with no fault, route7_underground_exit's warp to UndergroundPath_EastEntrance is not confirmed on the sim (the screen after the fade reads as before the hold): the sim's warp off the Bicycle, before this class can sweep 08.03"},
    });
    return list;
}

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

//  The gyms whose leader the story beats before row `row`: every trainer
//  there is beaten with the leader (the leader's script's set_gym_trainers),
//  so none of them sees the player on a later row's walk (17.04b past
//  Takashi, 14.04b past Erika's trainers).
std::set<std::string> gyms_cleared_before(const StoryCheckpoint& row){
    std::set<std::string> gyms;
    for (const StoryCheckpoint& earlier : story_checkpoints()){
        if (earlier.id == row.id){
            break;
        }
        for (const StoryStep& step : earlier.steps){
            if (step.kind == StoryStepKind::BATTLE && step.pose_after && Pokemon::story_arg<std::string>(step.args, "trainer").starts_with("TRAINER_LEADER_")){
                gyms.insert(Pokemon::grid_pose_of(*step.pose_after).map);
            }
        }
    }
    return gyms;
}

//  The run flags row `row` starts with: its start_flags.
std::set<std::string> start_flag_set(const StoryCheckpoint& row){
    return std::set<std::string>(row.start_flags.begin(), row.start_flags.end());
}

//  sweep_maps() as row `row` starts: the gyms cleared before it with no
//  trainer left to see the player, nor the trainers its start_flags have
//  beaten (16.03: the Rocket by 5F's corridor, whom 16.02's walk fights).
std::vector<MapData> row_maps(const StoryCheckpoint& row){
    std::vector<MapData> maps = with_trainers_beaten(sweep_maps(), beaten_trainers(start_flag_set(row)));
    const std::set<std::string> cleared = gyms_cleared_before(row);
    for (MapData& map : maps){
        if (!cleared.contains(map.name)){
            continue;
        }
        for (MapObject& object : map.objects){
            object.trainer.reset();
        }
    }
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
    std::map<size_t, Pokemon::GateState> gates_before;
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
    out.gates_before.emplace(c, sim.state().gates);
    const size_t warps_before = sim.state().warped_from.size();
    out.failure = row_step_on_sim(sim, at, row.steps[c], row.id, start_flag_set(row));
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

//  `script` with what row `row` starts with: its start_gates open and,
//  from its start_flags, the Card Key held (16.03 to 16.06's doors, 14.04b's
//  cut tree, 19.04d's switch).
SimScript with_row_start(SimScript script, const StoryCheckpoint& row){
    script.gates = row.start_gates.value_or(Pokemon::GateState());
    script.card_key_held = std::find(row.start_flags.begin(), row.start_flags.end(), "card_key_held") != row.start_flags.end();
    return script;
}

//  `script` for a new sim after `sim`, riding if that one was, with its
//  gates and the Card Key if it had them.
SimScript carried_over(SimScript script, const OverworldSim& sim){
    script.on_bike = sim.state().on_bike;
    script.gates = sim.state().gates;
    script.card_key_held = sim.card_key_held();
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
    const SimScript script = with_lifts(with_row_start(row_script, row));
    RowRun out;
    std::optional<MapPosition> off_maps;
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    std::vector<MapData> maps = row_maps(row);
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
            sim = std::make_unique<OverworldSim>(maps, land, at.facing.value_or(sim->state().facing), carried_over(script, *sim));
        }
    }
    if (out.failure.empty() && !sim->state().violations.empty()){
        out.failure = "a violation on the way: " + sim->state().violations.back();
    }
    out.end = off_maps.value_or(sim->state().position);
    add(sim->state());
    return out;
}

//  Why `run` differs from the clean run `clean`, or nothing.
std::optional<std::string> differs(const RowRun& run, const RowRun& clean){
    const std::optional<std::string> failure = run.failure.empty() ? std::nullopt : std::optional<std::string>(run.failure);
    return Pokemon::variant_failure(failure, to_string(run.end), to_string(clean.end));
}

//  The sweep of `fault` over row `row` with `variants`.
UnitTestResult sweep_variants(Logger& logger, const StoryCheckpoint& row, FaultClass fault, const std::vector<SweepVariant>& variants){
    return Pokemon::run_sweep(logger, row.id, fault_class_name(fault), variants);
}

//  A variant over the whole row: row `row` with `script`, against `clean`.
SweepVariant whole_row_variant(std::string label, const StoryCheckpoint& row, const RowRun& clean, SimScript script){
    return {"", std::move(label), [&row, &clean, script = std::move(script)]{
        return differs(run_row(row, script, {}, clean.scripted), clean);
    }};
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

//  Row `row`'s step `c` ending on `tile`, not its pose_after.
SweepVariant end_pose_variant(const StoryCheckpoint& row, const RowRun& clean, size_t c, TilePos tile){
    return {row.steps[c].name, std::format("ends on {}", to_string(tile)), [&row, &clean, c, tile]{
        return differs(run_row(row, SimScript{}, {{c, tile}}, clean.scripted), clean);
    }};
}

//  Row `row`'s stubbed steps that moved the player and have a walk after
//  them, each ending on every open tile next to the table's pose in turn.
UnitTestResult sweep_scene_end_pose(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    std::vector<SweepVariant> variants;
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
            variants.push_back(end_pose_variant(row, clean, c, tile));
        }
    }
    return sweep_variants(logger, row, FaultClass::SCENE_END_POSE, variants);
}

//  Row `row` with wild battles at the game's odds, from each seed. Logs
//  passed, time, tiles walked again and time lost per battle.
UnitTestResult sweep_wild_battles(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    int battles = 0;
    const Pokemon::SeedsResult seeds = Pokemon::run_seeds(logger, std::format("{} wild battles", row.id), [&](uint32_t seed){
        SimScript script;
        script.encounter_rates = STORY_ENCOUNTER_RATES;
        script.encounter_seed = seed;
        script.wild_floor_maps = WILD_FLOOR_MAPS;
        const RowRun run = run_row(row, script, {}, clean.scripted);
        battles += run.wild_battles;
        Pokemon::SeedRun out{.failure = differs(run, clean)};
        if (out.failure){
            return out;
        }
        out.measures["time (s)"] = run.time.count() / 1000.;
        out.measures["tiles walked again"] = run.steps - clean.steps;
        if (run.wild_battles > 0){
            out.measures["s lost per battle"] = (run.time - clean.time - run.in_wild_battles).count() / 1000. / run.wild_battles;
        }
        return out;
    });
    logger.log(std::format("Fault sweep {} wild battles: {} battles, clean time {:.1f} s.", row.id, battles, clean.time.count() / 1000.));
    return Pokemon::sweep_result(logger, row.id, fault_class_name(FaultClass::WILD_BATTLES), seeds.seeds, seeds.failures);
}

//  Row `row` with every warp's fade ending LATE_FADE late.
UnitTestResult sweep_late_fade(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    SimScript script;
    script.timing.fade += LATE_FADE;
    return sweep_variants(logger, row, FaultClass::LATE_FADE, {
        whole_row_variant(std::format("every fade {} ms late", LATE_FADE.count()), row, clean, std::move(script)),
    });
}

//  Row `row` with every warp's new map fading in for SLOW_FADE_IN.
UnitTestResult sweep_slow_fade_in(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    SimScript script;
    script.timing.fade_in = SLOW_FADE_IN;
    return sweep_variants(logger, row, FaultClass::SLOW_FADE_IN, {
        whole_row_variant(std::format("every warp fading in {} ms", SLOW_FADE_IN.count()), row, clean, std::move(script)),
    });
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
    std::vector<SweepVariant> variants;
    for (unsigned seed = 0; seed <= DARK_CAVE_SEEDS; seed++){
        SimScript script;
        script.dark_maps = DARK_MAPS;
        if (seed > 0){
            script.encounter_rates = STORY_ENCOUNTER_RATES;
            script.encounter_seed = seed;
            script.wild_floor_maps = WILD_FLOOR_MAPS;
        }
        variants.push_back(whole_row_variant(seed == 0 ? std::string("no wild battles") : std::format("seed {}", seed), row, clean, std::move(script)));
    }
    return sweep_variants(logger, row, FaultClass::DARK_CAVE, variants);
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
//  it fails or ends off the clean run's tile, or nothing.
std::optional<std::string> line_with_battle(const StoryCheckpoint& row, const RowRun& clean, size_t c, int battle_step){
    const MapPosition from = clean.before.at(c);
    const auto next = clean.before.find(c + 1);
    const MapPosition last = next == clean.before.end() ? clean.end : next->second;
    SimScript script = with_row_start(SimScript{}, row);
    script.gates = clean.gates_before.at(c);
    script.wild_floor_maps = {from.map};
    script.encounters = {SimEncounter{.grass_step = battle_step}};
    OverworldSim sim(row_maps(row), from, clean.facing_before.at(c), script);
    StoryPose at{from.map, from.point, clean.facing_before.at(c)};
    const std::string failure = row_step_on_sim(sim, at, row.steps[c], row.id);
    return Pokemon::variant_failure(failure.empty() ? std::nullopt : std::optional<std::string>(failure), to_string(sim.state().position), to_string(last));
}

//  Row `row` with a wild battle on each tile of each step into a trainer's
//  line before its last, one at a time: the tap after it must not be lost.
UnitTestResult sweep_lost_tap(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    std::vector<SweepVariant> variants;
    for (size_t c = 0; c < row.steps.size(); c++){
        const StoryStep& step = row.steps[c];
        if (step.kind != StoryStepKind::STEP || step.action != TRAINER_LINE_VARIANT || clean.scripted.contains(c)){
            continue;
        }
        const int tiles = line_tiles_before_last(row, clean, c);
        for (int battle_step = 1; battle_step <= tiles; battle_step++){
            variants.push_back({step.name, std::format("a wild battle on step {}", battle_step), [&row, &clean, c, battle_step]{
                return line_with_battle(row, clean, c, battle_step);
            }});
        }
    }
    return sweep_variants(logger, row, FaultClass::LOST_TAP, variants);
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
    std::vector<SweepVariant> variants;
    for (const auto& [c, map] : first_warps_by_map(row, clean)){
        variants.push_back({row.steps[c].name, std::format("every warp on {} does nothing", map), [&row, &clean, c, map]() -> std::optional<std::string>{
            SimScript script;
            script.dead_warp_maps = {map};
            const RowRun run = run_row(row, script, {}, clean.scripted);
            if (run.failed_at == c){
                return std::nullopt;
            }
            if (run.failure.empty()){
                return std::format("not stopped: the row went on and ended on {}.", to_string(run.end));
            }
            return std::format("stopped at {}, not here: {}", run.failed_at ? row.steps[*run.failed_at].name : "the row's end", run.failure);
        }});
    }
    return sweep_variants(logger, row, FaultClass::WARP_NO_OP, variants);
}

//  Row `row` with each step tap that moved the player read as not moved,
//  one at a time, counted across the row's sims.
UnitTestResult sweep_unseen_step(Logger& logger, const StoryCheckpoint& row, const RowRun& clean){
    std::vector<SweepVariant> variants;
    for (int tap = 1; tap <= clean.moved_step_taps; tap++){
        SimScript script;
        script.unseen_step_taps = {tap};
        variants.push_back(whole_row_variant(std::format("step tap {} read as not moved", tap), row, clean, std::move(script)));
    }
    return sweep_variants(logger, row, FaultClass::UNSEEN_STEP, variants);
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
    if (!clean.failure.empty()){
        return Pokemon::sweep_result(logger, row.id, fault_class_name(FaultClass::MOUNT_LOST), 0, {{"", "riding, no fault", clean.failure}});
    }
    std::vector<SweepVariant> variants;
    for (int leg = 1; leg <= clean.bike_legs; leg++){
        SimScript script = riding;
        script.off_bike_at_legs = {leg};
        variants.push_back(whole_row_variant(std::format("bike leg {} starts off the Bicycle", leg), row, clean, std::move(script)));
    }
    return sweep_variants(logger, row, FaultClass::MOUNT_LOST, variants);
}

//  Class `fault` swept over row `row`, which runs clean as `clean`.
UnitTestResult sweep_fault(Logger& logger, FaultClass fault, const StoryCheckpoint& row, const RowRun& clean){
    switch (fault){
    case FaultClass::SCENE_END_POSE:
        return sweep_scene_end_pose(logger, row, clean);
    case FaultClass::WILD_BATTLES:
        return sweep_wild_battles(logger, row, clean);
    case FaultClass::LATE_FADE:
        return sweep_late_fade(logger, row, clean);
    case FaultClass::SLOW_FADE_IN:
        return sweep_slow_fade_in(logger, row, clean);
    case FaultClass::DARK_CAVE:
        return sweep_dark_cave(logger, row, clean);
    case FaultClass::LOST_TAP:
        return sweep_lost_tap(logger, row, clean);
    case FaultClass::WARP_NO_OP:
        return sweep_warp_no_op(logger, row, clean);
    case FaultClass::UNSEEN_STEP:
        return sweep_unseen_step(logger, row, clean);
    case FaultClass::MOUNT_LOST:
        return sweep_mount_lost(logger, row);
    case FaultClass::SCENE_EARLY:
    case FaultClass::BATTLE_OPENING:
        //  Not in FAULT_SWEEP_CASES: no test is made.
        break;
    }
    return true;
}

//  A fault class swept over row `row` (empty in FAULT_SWEEP_CASES: each
//  row with a start); no fault: the row's clean run alone.
struct FaultSweepInput{
    std::optional<FaultClass> fault = std::nullopt;
    std::string row = {};
};
//  Whether a sweep is expected to fail is KNOWN_FAILING's, by name.
struct FaultSweepExpectation{
};
//  `name`: the fault class's in FAULT_SWEEP_CASES; "<row>, <fault class>"
//  in a test.
struct FaultSweepCase{
    std::string name;
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

//  The name of the fault sweep test of case `name`.
std::string fault_sweep_test_name(std::string_view name){
    return std::format("PokemonFRLG::AutoStory - Fault sweep: {}", name);
}

class Test_AutoStoryFaultSweep : public UnitTest{
public:
    explicit Test_AutoStoryFaultSweep(FaultSweepCase test_case)
        : UnitTest(fault_sweep_test_name(test_case.name))
        , m_fault(test_case.input.fault)
        , m_row(table_row(test_case.input.row))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return known_failing().run(logger, name(), [&]{ return sweep(logger); });
    }

private:
    UnitTestResult sweep(Logger& logger) const{
        const RowRun clean = run_row(m_row, SimScript{});
        const std::optional<std::string> clean_failure = clean.failure.empty() ? std::nullopt : std::optional<std::string>(clean.failure);
        if (!m_fault){
            if (clean_failure){
                return std::format("the row fails clean: {}", *clean_failure);
            }
            return true;
        }
        //  A row that fails clean fails its "clean run" test.
        return Pokemon::sweep_clean_first(logger, m_row.id, clean_failure, [&]{
            return sweep_fault(logger, *m_fault, m_row, clean);
        });
    }

    std::optional<FaultClass> m_fault;
    const StoryCheckpoint& m_row;
};

} // namespace


//  The shared sim faults (Pokemon_SimFaults.h) this sweep injects: all but
//  SCENE_EARLY and BATTLE_OPENING, which need knobs the sims don't have yet
//  (see the top of this file).
const FaultSweepCase FAULT_SWEEP_CASES[] = {
    {.name = "clean run", .input = {.fault = std::nullopt}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::SCENE_END_POSE)), .input = {.fault = FaultClass::SCENE_END_POSE}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::WILD_BATTLES)), .input = {.fault = FaultClass::WILD_BATTLES}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::LATE_FADE)), .input = {.fault = FaultClass::LATE_FADE}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::SLOW_FADE_IN)), .input = {.fault = FaultClass::SLOW_FADE_IN}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::DARK_CAVE)), .input = {.fault = FaultClass::DARK_CAVE}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::LOST_TAP)), .input = {.fault = FaultClass::LOST_TAP}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::WARP_NO_OP)), .input = {.fault = FaultClass::WARP_NO_OP}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::UNSEEN_STEP)), .input = {.fault = FaultClass::UNSEEN_STEP}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::MOUNT_LOST)), .input = {.fault = FaultClass::MOUNT_LOST}, .expectation = {}},
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

namespace{

//  Each of FAULT_SWEEP_CASES over each row with a start.
std::vector<FaultSweepCase> fault_sweep_cases(){
    std::vector<FaultSweepCase> out;
    for (const FaultSweepCase& item : FAULT_SWEEP_CASES){
        for (const StoryCheckpoint& row : story_checkpoints()){
            if (!row.start || !story_pose(Pokemon::pose_of(*row.start))){
                continue;
            }
            out.push_back({
                .name = std::format("{}, {}", row.id, item.name),
                .input = {.fault = item.input.fault, .row = row.id},
                .expectation = item.expectation,
            });
        }
    }
    return out;
}

//  The fault sweep tests made: fault_sweep_cases().
struct KnownFailingNamesInput{
};
//  Every known_failing() entry names one of them: one renamed would
//  otherwise drop off the list and fail the suite.
struct KnownFailingNamesExpectation{
};
struct KnownFailingNamesCase{
    std::string name;
    KnownFailingNamesInput input;
    KnownFailingNamesExpectation expectation;
};

class Test_AutoStoryFaultSweepKnownFailing : public UnitTest{
public:
    explicit Test_AutoStoryFaultSweepKnownFailing(const KnownFailingNamesCase& test_case)
        : UnitTest(fault_sweep_test_name(test_case.name))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::set<std::string> names;
        for (const FaultSweepCase& test_case : fault_sweep_cases()){
            names.insert(fault_sweep_test_name(test_case.name));
        }
        const std::vector<std::string> stale = known_failing().stale(names);
        if (stale.empty()){
            return true;
        }
        std::string out = "KNOWN_FAILING names no such test:";
        for (const std::string& entry : stale){
            out += std::format("\n  {}", entry);
        }
        return out;
    }
};

const KnownFailingNamesCase KNOWN_FAILING_NAMES_CASES[] = {
    {.name = "every known failing entry names a test", .input = {}, .expectation = {}},
};

} // namespace

void add_tests_AutoStoryFaultSweep(UnitTestDatabase& database){
    for (const FaultSweepCase& test_case : fault_sweep_cases()){
        database.add<Test_AutoStoryFaultSweep>(test_case);
    }
    for (const KnownFailingNamesCase& test_case : KNOWN_FAILING_NAMES_CASES){
        database.add<Test_AutoStoryFaultSweepKnownFailing>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
