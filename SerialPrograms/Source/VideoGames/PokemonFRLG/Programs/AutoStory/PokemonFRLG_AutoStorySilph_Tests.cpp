/*  AutoStory Silph Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 16.01 to 16.06 (StoryData/frlg-16.01.plan.json ...) on the overworld sim: from
 *  Saffron's nurse into Silph Co., up to 3F and through its pad to 5F;
 *  the 7F rival's fight as PokeGuy fought it.
 *
 */

#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldGatedRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryBattlePolicyData.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryMovedTrainers.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows16_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeachPlan.h"
#include "PokemonFRLG_AutoStoryTrainerLine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySilphInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStorySilphExpectation{
    std::string_view description;
};

struct AutoStorySilphCase{
    AutoStorySilphInput input;
    AutoStorySilphExpectation expectation;
};

class Test_AutoStorySilph : public UnitTest{
public:
    explicit Test_AutoStorySilph(AutoStorySilphCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStorySilphCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  16.01 on the sim from Saffron's nurse: Silph Co.'s stairs to 3F, west
//  along the hall and onto the pad to 5F, where the player stands on the pad
//  that leads back. No trainer is met and no rule is broken.
UnitTestResult test_pad_to_5f(){
    const StoryCheckpoint& row = checkpoint("16.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    std::vector<std::string> floors;
    for (const StoryStep& step : row.steps){
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return std::format("16.01: ended on {}; {}", to_string(sim.state().position), failure);
        }
        floors.push_back(sim.state().position.map);
    }
    const MapPosition pad{"SilphCo_5F", {15, 7}};
    if (!(sim.state().position == pad)){
        return std::format("16.01: ended on {}, not on 5F's pad {}.", to_string(sim.state().position), to_string(pad));
    }
    if (floors != std::vector<std::string>{"SilphCo_3F", "SilphCo_5F"}){
        return "16.01: the steps ended on the wrong floors.";
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "16.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  The steps of row `id` walked on the sim over `maps`, from the row's start with
//  the Card Key and doors its start_flags and start_gates say.
class RowWalk{
public:
    RowWalk(std::string_view id, const std::vector<MapData>& maps)
        : m_row(checkpoint(id))
        , m_at(*story_pose(Pokemon::pose_of(*m_row.start)))
        , m_sim(maps, MapPosition{m_at.map, m_at.tile}, *m_at.facing, script_of(m_row))
        , m_flags(m_row.start_flags.begin(), m_row.start_flags.end())
    {}

    //  Each step in turn; the failure of the first that fails, or empty.
    std::string run(){
        for (const StoryStep& step : m_row.steps){
            //  The sim has no trigger scenes: a scripted battle (the rival's, Giovanni's)
            //  is taken as won where it leaves the player. A trainer's battle, his
            //  message up after the step into his line, is fought.
            if (step.kind == StoryStepKind::BATTLE && m_sim.state().screen != SimScreen::MESSAGE){
                m_at = *story_pose(Pokemon::pose_of(*step.pose_after));
                m_floors.push_back(m_sim.state().position.map);
                continue;
            }
            //  The run's flags as the program's state has them at the step: the
            //  row's start flags and what the steps before set (flags_after).
            if (const std::string failure = row_step_on_sim(m_sim, m_at, step, m_row.id, m_flags); !failure.empty()){
                return failure;
            }
            m_flags.insert(step.flags_after.begin(), step.flags_after.end());
            m_floors.push_back(m_sim.state().position.map);
        }
        return "";
    }
    const StoryCheckpoint& row() const{ return m_row; }
    const OverworldSim& sim() const{ return m_sim; }
    const std::vector<std::string>& floors() const{ return m_floors; }

    //  What the row left out as a line, or empty: the sim's rule breaks and the
    //  trainers it fought, and the gates it has open against the ones the row's
    //  steps declare (gates_after).
    std::string problems() const{
        std::string text;
        if (!m_sim.state().violations.empty()){
            text += std::format(" broke a rule: {};", m_sim.state().violations.front());
        }
        const Pokemon::GateState declared = Pokemon::checkpoint_gates(m_row, m_row.steps.size());
        if (!(declared == m_sim.state().gates)){
            text += std::format(" the sim has {} gates open, the row's gates_after {};", m_sim.state().gates.opened.size(), declared.opened.size());
        }
        return text;
    }

private:
    static SimScript script_of(const StoryCheckpoint& row){
        SimScript script;
        script.card_key_held = std::find(row.start_flags.begin(), row.start_flags.end(), "card_key_held") != row.start_flags.end();
        script.gates = row.start_gates.value_or(Pokemon::GateState());
        return script;
    }

    const StoryCheckpoint& m_row;
    StoryPose m_at;
    OverworldSim m_sim;
    std::set<std::string> m_flags;
    std::vector<std::string> m_floors;
};

std::string opened_doors(const OverworldSim& sim){
    std::string text;
    for (const std::string& key : sim.state().gates.opened){
        text += " " + key;
    }
    return text;
}

//  `maps` with the trainer at `at` on `map_name` beaten (his sight is gone, as in a
//  save made after his fight).
std::vector<MapData> with_beaten(std::vector<MapData> maps, std::string_view map_name, int x, int y){
    for (MapData& map : maps){
        for (MapObject& object : map.objects){
            if (map.name == map_name && object.x == x && object.y == y){
                object.trainer.reset();
            }
        }
    }
    return maps;
}

//  `maps` with the trainer at `at` on `map_name` beaten and standing on
//  (`stop_x`, `stop_y`): where he stopped when he walked up to the player
//  and sight ended, and where the game leaves him after his fight.
std::vector<MapData> with_beaten_at(std::vector<MapData> maps, std::string_view map_name, int x, int y, int stop_x, int stop_y){
    for (MapData& map : maps){
        for (MapObject& object : map.objects){
            if (map.name == map_name && object.x == x && object.y == y){
                object.x = stop_x;
                object.y = stop_y;
                object.wander.reset();
                object.trainer.reset();
            }
        }
    }
    return maps;
}

//  The maps a row after 16.02 starts on: the Rocket at 5F's (9, 21), beaten by
//  16.02 on its way, no longer sees the player (his fight is in the save) and
//  stands on (11, 21), where he stopped beside the player who waited on
//  (12, 21) for him (console B, 2026-11-14 run 4: he blocked 16.03's walk
//  west along row 21).
std::vector<MapData> maps_after_16_02(){
    return with_beaten_at(story_maps(), "SilphCo_5F", 9, 21, 11, 21);
}

//  Row `id` walked on the sim: the steps run, no rule broken, the gates the
//  row declares open, and the Card Key held or not as `card_key` says. The
//  trainers it fought and the tile it ends on are returned for the caller.
struct RowResult{
    std::string failure;
    std::vector<std::string> trainers;
    std::vector<std::string> floors;
    MapPosition end;
    bool card_key = false;
};
RowResult walk_row(std::string_view id, const std::vector<MapData>& maps){
    RowWalk walk(id, maps);
    RowResult out;
    out.failure = walk.run();
    if (out.failure.empty()){
        out.failure = walk.problems();
    }
    out.trainers = walk.sim().state().trainers_beaten;
    out.floors = walk.floors();
    out.end = walk.sim().state().position;
    out.card_key = walk.sim().card_key_held();
    return out;
}

std::string wrong_end(std::string_view id, const RowResult& result, const MapPosition& want){
    if (!result.failure.empty()){
        return std::format("{}: ended on {}; {}", id, to_string(result.end), result.failure);
    }
    if (!(result.end == want)){
        return std::format("{}: ended on {}, not {}.", id, to_string(result.end), to_string(want));
    }
    return "";
}

//  16.02: 5F's pad to 9F and back by the loop to the corridor, east to the
//  ball with the CARD KEY (21, 21); the scientist on the way fights.
UnitTestResult test_card_key_row(){
    const RowResult result = walk_row("16.02", story_maps());
    if (const std::string wrong = wrong_end("16.02", result, MapPosition{"SilphCo_5F", {21, 21}}); !wrong.empty()){
        return wrong;
    }
    if (!result.card_key){
        return "16.02: the Card Key was not picked up.";
    }
    if (result.trainers != std::vector<std::string>{"scientist", "rocket_m"}){
        return "16.02: did not fight just the scientist and the Rocket on 5F.";
    }
    return true;
}

//  16.03: up to 7F by 3F's Card Key door (opened with the key), the rival on
//  his trigger, where his battle leaves the player; then 16.03b to the
//  employee with the Lapras.
UnitTestResult test_rival_row(){
    const RowResult result = walk_row("16.03", maps_after_16_02());
    if (const std::string wrong = wrong_end("16.03", result, MapPosition{"SilphCo_7F", {2, 4}}); !wrong.empty()){
        return wrong;
    }
    const StoryPose after_rival = *story_pose(Pokemon::pose_of(*checkpoint("16.03b").start));
    if (after_rival.map != "SilphCo_7F" || !(after_rival.tile == MapPosition{"SilphCo_7F", {2, 4}}.point)){
        return "16.03b doesn't start where 16.03 ends.";
    }
    const RowResult to_employee = walk_row("16.03b", maps_after_16_02());
    if (const std::string wrong = wrong_end("16.03b", to_employee, MapPosition{"SilphCo_7F", {1, 7}}); !wrong.empty()){
        return wrong;
    }
    const std::vector<std::string> floors_have = {"SilphCo_3F", "SilphCo_7F"};
    for (const std::string& floor : floors_have){
        if (std::find(result.floors.begin(), result.floors.end(), floor) == result.floors.end()){
            return "16.03: the steps never reached " + floor + ".";
        }
    }
    return true;
}

//  16.03 to_rival_trigger from 7F's pad (5, 4): "west 2 (open)" to (3, 4)
//  ends where a hold can't tell a tile short from none, so the walker finds
//  the player at walls and walks back, its last leg "north 1 (to wall)". The
//  route must still end facing west, as planned: the rival_trigger step's
//  tap west then only turned the player, who never stepped onto the
//  trigger (2, 4) (pokefirered data/maps/SilphCo_7F/map.json, coord_events
//  (2, 4) and (2, 5), SilphCo_7F_EventScript_RivalTriggerTop) and the
//  rival's battle never came (console B, 2026-11-15 00:28, 16.03 run 5).
UnitTestResult test_rival_trigger_walk_ends_facing_west(){
    const std::vector<MapData> maps = maps_after_16_02();
    const MapPosition pad{"SilphCo_7F", {5, 4}};
    const MapPosition beside_trigger{"SilphCo_7F", {3, 4}};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps, pad, RouteGoal{beside_trigger, Direction::WEST}, story_route_costs(), Direction::WEST
    );
    if (!route || route->empty()){
        return "No route from 7F's pad to (3, 4).";
    }
    OverworldSim sim(maps, pad, Direction::WEST);
    OverworldSimWalker walker(sim);
    const RoutePlanning planning{.maps = &maps, .costs = story_route_costs(), .start = pad, .label = "to_rival_trigger"};
    if (const std::string failure = walker.run(*route, planning); !failure.empty()){
        return std::format("The walk failed: {}", failure);
    }
    const std::vector<std::string>& log = walker.log();
    const bool found_at_walls = std::any_of(log.begin(), log.end(), [](const std::string& line){
        return line.find("finding the player at walls") != std::string::npos;
    });
    if (!found_at_walls){
        return "Expected the walker to find the player at walls after \"west 2 (open)\", as on the console.";
    }
    if (!(sim.state().position == beside_trigger) || sim.state().facing != Direction::WEST){
        return std::format(
            "Expected {} facing west, got {} facing {}.",
            to_string(beside_trigger), to_string(sim.state().position), direction_name(sim.state().facing)
        );
    }
    return true;
}

//  16.03b's checkpoint: Gift RNG's Lapras, on 7F beside the employee.
UnitTestResult test_lapras_handoff(){
    const StoryCheckpoint& gift = checkpoint("16.03b");
    if (gift.handoff != StoryHandoff::GIFT_RNG || gift.gift != PokemonFRLG_RngTarget::lapras){
        return "16.03b does not hand off to Gift RNG for the Lapras.";
    }
    if (checkpoint("16.03").handoff != StoryHandoff::NONE){
        return "16.03 hands off to Gift RNG; only 16.03b, beside the employee, does.";
    }
    const StoryCheckpoint& live = checkpoint("16.04");
    if (!live.live_start || live.handoff != StoryHandoff::NONE){
        return "16.04 is not the save Gift RNG leaves.";
    }
    return true;
}

//  16.04 runs one step, the summary Gift RNG leaves open; 16.05 starts where it does.
UnitTestResult test_after_gift_row(){
    const RowResult result = walk_row("16.04", maps_after_16_02());
    const std::string wrong = wrong_end("16.04", result, MapPosition{"SilphCo_7F", {1, 7}});
    if (!wrong.empty()){
        return wrong;
    }
    return true;
}

//  16.05: the pad to 11F, down its left wall out of the Rocket's column, the
//  Rocket beside the player (his fight), the Card Key door and Giovanni's
//  trigger; it ends with his battle, where it leaves the player (a boss
//  battle is its own checkpoint), and 16.05b starts there.
UnitTestResult test_giovanni_row(){
    const RowResult result = walk_row("16.05", maps_after_16_02());
    const MapPosition after_giovanni{"SilphCo_11F", {5, 15}};
    if (const std::string wrong = wrong_end("16.05", result, after_giovanni); !wrong.empty()){
        return wrong;
    }
    if (result.trainers.size() != size_t(1)){
        return "16.05: the Rocket at (2, 19) did not fight alone.";
    }
    const std::vector<StoryStep>& steps = checkpoint("16.05").steps;
    if (steps.empty() || steps.back().name != "giovanni"){
        return "16.05 does not end with Giovanni's battle.";
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("16.05b").start));
    if (next.map != after_giovanni.map || !(next.tile == after_giovanni.point) || next.facing != Direction::EAST){
        return "16.05b doesn't start where 16.05 ends, facing east.";
    }
    return true;
}

//  Checkpoint `id`'s step named `name`; none if it has none.
const StoryStep* step_of_row(std::string_view id, std::string_view name){
    for (const StoryStep& step : checkpoint(id).steps){
        if (step.name == name){
            return &step;
        }
    }
    return nullptr;
}

//  16.05b below_president from where Giovanni's battle leaves the player:
//  (5, 15) facing east, to Giovanni, who walked down to (6, 15) (pokefirered
//  data/maps/SilphCo_11F/scripts.inc, SilphCo_11F_Movement_GiovanniApproachLeft
//  and _PlayerFaceGiovanni). The walk is planned from the giovanni step's
//  declared pose. With that pose facing north, "north 5" was held with no
//  turn first: the hold turned the player, went 4 tiles to (5, 11), and
//  "east 4" stopped on the table at (6, 11); the walker found the player
//  again at the wrong tile and ended below the table, where the president's
//  talk opened no message (console B, 2026-11-15 13:02, 16.05).
UnitTestResult test_president_walk_after_giovanni(){
    const StoryStep* giovanni = step_of_row("16.05", "giovanni");
    const StoryStep* below = step_of_row("16.05b", "below_president");
    if (giovanni == nullptr || below == nullptr || !giovanni->pose_after){
        return "16.05 has no giovanni step with a pose after it, or 16.05b no below_president.";
    }
    const StoryPose at = *story_pose(Pokemon::pose_of(*giovanni->pose_after));
    const std::optional<StoryPose> target = story_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(below->args, "target")));
    if (!at.facing || !target){
        return "16.05: the giovanni step's pose has no facing, or below_president has no target.";
    }
    const std::set<std::string> flags{std::string(SILPH_CLEARED_FLAG)};
    const std::vector<MapData>& maps = run_story_maps(flags);
    const MapPosition start{at.map, at.tile};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps, start, RouteGoal{{target->map, target->tile}, target->facing}, story_route_costs(), *at.facing
    );
    if (!route || route->empty()){
        return std::format("No route from {} to the president.", to_string(start));
    }
    OverworldSim sim(maps, MapPosition{"SilphCo_11F", {5, 15}}, Direction::EAST);
    const RoutePlanning planning{.maps = &maps, .costs = story_route_costs(), .start = start, .label = "below_president"};
    if (const std::string failure = OverworldSimWalker(sim).run(*route, planning, at.facing); !failure.empty()){
        return std::format("16.05b: ended on {}; {}", to_string(sim.state().position), failure);
    }
    const MapPosition president_talk{"SilphCo_11F", {9, 10}};
    if (!(sim.state().position == president_talk) || sim.state().facing != Direction::NORTH){
        return std::format(
            "16.05b: ended on {} facing {}, not {} facing north.",
            to_string(sim.state().position), direction_name(sim.state().facing), to_string(president_talk)
        );
    }
    return true;
}

//  16.06: the walk back down to Saffron's Center on the maps with no Rocket in
//  Silph Co. or Saffron City.
UnitTestResult test_return_row(){
    const std::set<std::string> flags{std::string(SILPH_CLEARED_FLAG)};
    const RowResult result = walk_row("16.06", with_beaten(run_story_maps(flags), "SilphCo_5F", 9, 21));
    if (const std::string wrong = wrong_end("16.06", result, MapPosition{"SaffronCity", {24, 39}}); !wrong.empty()){
        return wrong;
    }
    if (!result.trainers.empty()){
        return "16.06: walked into a trainer's line.";
    }
    return true;
}

bool has_object_at(const std::vector<MapData>& maps, std::string_view map_name, int x, int y){
    for (const MapData& map : maps){
        for (const MapObject& object : map.objects){
            if (map.name == map_name && object.x == x && object.y == y){
                return true;
            }
        }
    }
    return false;
}

//  The Rocket at Saffron (46, 13) blocks the Gym's door until the story sets
//  silph_cleared (16.05's Giovanni); the maps of 15.01 and 16.01 keep him.
UnitTestResult test_saffron_rocket_removed(){
    if (!has_object_at(story_maps(), "SaffronCity", 46, 13)){
        return "No Rocket at Saffron (46, 13) before Silph Co. is cleared.";
    }
    if (!has_object_at(run_story_maps({"card_key_held"}), "SaffronCity", 46, 13)){
        return "Another flag hides the Rocket at Saffron (46, 13).";
    }
    if (has_object_at(run_story_maps({std::string(SILPH_CLEARED_FLAG)}), "SaffronCity", 46, 13)){
        return "A Rocket still stands at Saffron (46, 13) after Silph Co.";
    }
    return true;
}

//  16.02 fights the Rocket by 5F's corridor (9, 21) on purpose and sets the
//  flag 16.03 starts with: he faces north or east by turns
//  (MOVEMENT_TYPE_FACE_UP_AND_RIGHT), so a walk that only ended in his sight
//  fought him when he happened to face east; a wait on the edge of his sight
//  (TRAINER_WAIT_VARIANT) fights him every run (user, 2026-11-14).
UnitTestResult test_corridor_rocket_fought(){
    const std::string flag = std::string(TRAINER_BEATEN_FLAG) + "SilphCo_5F/rocket_m@9,21";
    const std::vector<StoryStep>& steps = checkpoint("16.02").steps;
    const auto battle = std::find_if(steps.begin(), steps.end(), [&](const StoryStep& step){
        return step.kind == StoryStepKind::BATTLE && std::find(step.flags_after.begin(), step.flags_after.end(), flag) != step.flags_after.end();
    });
    if (battle == steps.end()){
        return "16.02 has no battle that sets " + flag + ".";
    }
    if (battle == steps.begin() || std::prev(battle)->action != TRAINER_WAIT_VARIANT){
        return "16.02's Rocket battle does not follow a trainer_wait step onto the edge of his sight.";
    }
    const std::vector<std::string>& start = checkpoint("16.03").start_flags;
    if (std::find(start.begin(), start.end(), flag) == start.end()){
        return "16.03 does not start with " + flag + ".";
    }
    return true;
}

//  silph_cleared is set by the Giovanni step, nothing earlier.
UnitTestResult test_silph_cleared_flag(){
    for (const std::string_view id : {"16.01", "16.02", "16.03", "16.03b", "16.04"}){
        for (const StoryStep& step : checkpoint(id).steps){
            if (std::find(step.flags_after.begin(), step.flags_after.end(), SILPH_CLEARED_FLAG) != step.flags_after.end()){
                return std::format("{} sets silph_cleared.", id);
            }
        }
    }
    const StoryCheckpoint& row = checkpoint("16.05");
    const bool set = std::any_of(row.steps.begin(), row.steps.end(), [](const StoryStep& step){
        return step.name == "giovanni" && std::find(step.flags_after.begin(), step.flags_after.end(), SILPH_CLEARED_FLAG) != step.flags_after.end();
    });
    if (!set){
        return "16.05's Giovanni step does not set silph_cleared.";
    }
    for (const std::string_view id : {"16.05b", "16.06"}){
        const std::vector<std::string>& start = checkpoint(id).start_flags;
        if (std::find(start.begin(), start.end(), SILPH_CLEARED_FLAG) == start.end()){
            return std::format("{} does not start with silph_cleared.", id);
        }
    }
    return true;
}

//  The president's Master Ball is skipped by the option or a SPEEDRUN, taken otherwise.
UnitTestResult test_master_ball_option(){
    using Pokemon::StoryCheckLevel;
    if (StoryExecutor::master_ball_skipped(true, StoryCheckLevel::DEFAULT)){
        return "The Master Ball is skipped with the option on.";
    }
    if (!StoryExecutor::master_ball_skipped(false, StoryCheckLevel::DEFAULT)){
        return "The Master Ball is taken with the option off.";
    }
    if (!StoryExecutor::master_ball_skipped(true, StoryCheckLevel::SPEEDRUN)){
        return "The Master Ball is taken in a SPEEDRUN.";
    }
    const StoryCheckpoint& row = checkpoint("16.05b");
    const bool skippable = std::any_of(row.steps.begin(), row.steps.end(), [](const StoryStep& step){
        return step.name == "master_ball" && step.skip_if == std::vector<std::string>{"master_ball_skipped"};
    });
    if (!skippable){
        return "16.05b's master_ball step is not skip_if master_ball_skipped.";
    }
    return true;
}

//  The Rocket 16.02 fights walked up to the player from (9, 21) and stopped on
//  (11, 21); a beaten trainer stays where he stopped and blocks that tile
//  (console B, 2026-11-14 run 4: 16.03's "west 11" ended against him two
//  tiles short of the pad). 16.02's battle records where he stands
//  (TRAINER_AT_FLAG), 16.03 starts with it, and the maps the run plans on
//  (run_story_maps()) have him there, not on his spawn tile.
UnitTestResult test_corridor_rocket_stands_where_he_stopped(){
    const std::string flag = std::string(TRAINER_AT_FLAG) + "SilphCo_5F/rocket_m@9,21=11,21";
    const std::vector<StoryStep>& steps = checkpoint("16.02").steps;
    const bool set_by_battle = std::any_of(steps.begin(), steps.end(), [&](const StoryStep& step){
        return step.kind == StoryStepKind::BATTLE && std::find(step.flags_after.begin(), step.flags_after.end(), flag) != step.flags_after.end();
    });
    if (!set_by_battle){
        return "16.02 has no battle that sets " + flag + ".";
    }
    const std::vector<std::string>& start = checkpoint("16.03").start_flags;
    if (std::find(start.begin(), start.end(), flag) == start.end()){
        return "16.03 does not start with " + flag + ".";
    }
    const std::set<std::string> run_flags{flag};
    const std::vector<MapData>& maps = run_story_maps(run_flags);
    if (!has_object_at(maps, "SilphCo_5F", 11, 21)){
        return "The run's maps have nobody on SilphCo_5F (11, 21), where the Rocket stopped.";
    }
    if (has_object_at(maps, "SilphCo_5F", 9, 21)){
        return "The run's maps still have the Rocket on his spawn tile SilphCo_5F (9, 21).";
    }
    return true;
}

//  16.03's step named `name`; none if it has none.
const StoryStep* rival_row_step(std::string_view name){
    for (const StoryStep& step : checkpoint("16.03").steps){
        if (step.name == name){
            return &step;
        }
    }
    return nullptr;
}

//  The 7F rival by PokeGuy's C2 (PokemonSimulation docs/battles/pokeguy-conditions.md,
//  silph-rival: Sp. Atk +2, accuracy +2, outspeed, Guard Spec first). Without it,
//  console B's lead attacked from turn 1, Venusaur slept it and the battle
//  was lost (2026-11-15 01:22, 16.03 run 6); the sim of that entry wins 12%
//  of 500, and 99% with the policy and Blizzard. The rows are PokemonSimulation's
//  planning_risk() for each foe leading against that entry (Lv43, 136/136).
UnitTestResult test_rival_fought_by_policy(){
    const StoryStep* rival = rival_row_step("rival");
    if (rival == nullptr || rival->kind != StoryStepKind::BATTLE){
        return "16.03 has no rival battle step.";
    }
    const BattlePolicy policy = battle_policy_of(rival->args);
    if (policy.kind != BattlePolicyKind::GOAL_BASED_SETUP){
        return "16.03 rival: expected the goal-based-setup policy.";
    }
    const SetupGoals& goals = policy.goals;
    if (goals.spa != 2 || goals.accuracy != 2 || !goals.outspeed || !goals.guard_accuracy
        || goals.torrent_power || goals.target_torrent
    ){
        return "16.03 rival: expected the doc's goals, Sp. Atk +2, accuracy +2, outspeed and Guard Spec first.";
    }
    const auto found = rival->args.find("foe_threats");
    const auto* rows = found == rival->args.end() ? nullptr : std::get_if<std::vector<std::vector<int64_t>>>(&found->second);
    const std::vector<std::vector<int64_t>> expected = {
        {76, 18, 17, 39, 19, 0, 0},     //  Pidgeot
        {78, 40, 15, 40, 40, 0, 0},     //  Gyarados
        {51, 14, 5, 32, 16, 0, 0},      //  Growlithe
        {93, 0, 4, 44, 22, 0, 0},       //  Alakazam
        {76, 46, 26, 96, 48, 0, 0},     //  Venusaur
    };
    if (rows == nullptr || *rows != expected){
        return "16.03 rival: foe_threats are not the generator's rows for console B's entry.";
    }
    return true;
}

//  Before the walk, TM14 Blizzard over Dig, as 13.03 teaches it: console B's
//  lead came to the rival with Dig (2026-11-15, 16.03 run 6), and Blizzard is
//  what wins against Venusaur. A lead that knows it already is left as it is.
UnitTestResult test_blizzard_before_rival(){
    const std::vector<StoryStep>& steps = checkpoint("16.03").steps;
    if (steps.empty() || steps[0].kind != StoryStepKind::GAME || steps[0].action != TEACH_ACTION){
        return "16.03: expected it to start with a TEACH.";
    }
    const std::optional<TeachPlan> plan = teach_plan_of(steps[0].args);
    if (!plan || plan->item != "tm14" || plan->move != "blizzard" || plan->member != 0){
        return "16.03: expected TM14 Blizzard on the lead.";
    }
    const TeachDecision console_b = decide_teach(*plan, {"bite", "water-pulse", "surf", "dig"});
    if (!console_b.error.empty() || console_b.forget != std::optional<size_t>(3)){
        return std::format("16.03: expected Blizzard over Dig on console B's lead: {}", console_b.error);
    }
    if (!decide_teach(*plan, {"bite", "water-pulse", "surf", "blizzard"}).already_known){
        return "16.03: a lead that knows Blizzard should be left as it is.";
    }
    return true;
}

//  The lead's attack against the rival's Venusaur: Blizzard, the one that
//  is super effective (PokeGuy: one Blizzard, 1:29:39).
UnitTestResult test_blizzard_against_venusaur(){
    PartyMember lead;
    lead.species = "blastoise";
    lead.moves_read = true;
    lead.level = 43;
    lead.moves = {{"bite", 25}, {"water-pulse", 20}, {"surf", 14}, {"blizzard", 5}};
    constexpr size_t BLIZZARD = 3;
    if (attack_slot_against(lead, {"venusaur"}) != BLIZZARD){
        return "Expected Blizzard against Venusaur.";
    }
    return true;
}

const AutoStorySilphCase AUTO_STORY_SILPH_CASES[] = {
    {
        .input = {.name = "16.01 Silph Co.'s stairs to 3F and its pad to 5F", .body = test_pad_to_5f},
        .expectation = {.description = "to 3F's pad (4, 6), onto 5F's (15, 7), nobody met"},
    },
    {
        .input = {.name = "16.02 the pad loop on 5F to the Card Key", .body = test_card_key_row},
        .expectation = {.description = "ends at (21, 21) with the key, the scientist and a Rocket fought"},
    },
    {
        .input = {.name = "16.03 3F's Card Key door, the pad to 7F and the rival; 16.03b to the employee", .body = test_rival_row},
        .expectation = {.description = "opens 3F's door_2, ends by the employee on 7F"},
    },
    {
        .input = {.name = "16.03 the walk to the rival's trigger, found again at walls", .body = test_rival_trigger_walk_ends_facing_west},
        .expectation = {.description = "after the holds and the way back, on (3, 4) facing west, the trigger (2, 4) a tap ahead"},
    },
    {
        .input = {.name = "16.03b/16.04 the Lapras by Gift RNG", .body = test_lapras_handoff},
        .expectation = {.description = "16.03b hands off to Gift RNG for a Lapras; 16.04 is the live save"},
    },
    {
        .input = {.name = "16.04 the save after Gift RNG", .body = test_after_gift_row},
        .expectation = {.description = "one step, still beside the employee"},
    },
    {
        .input = {.name = "16.05 11F's Card Key door and Giovanni", .body = test_giovanni_row},
        .expectation = {.description = "one Rocket fought, 11F's door opened, ends with Giovanni's battle where 16.05b starts"},
    },
    {
        .input = {.name = "16.05b to the president from where Giovanni's battle leaves the player", .body = test_president_walk_after_giovanni},
        .expectation = {.description = "from (5, 15) facing east, ends on (9, 10) facing north"},
    },
    {
        .input = {.name = "16.06 back to Saffron's Center with no Rockets", .body = test_return_row},
        .expectation = {.description = "no trainer met, ends below the Center's door"},
    },
    {
        .input = {.name = "the Saffron Rocket is gone after Silph Co.", .body = test_saffron_rocket_removed},
        .expectation = {.description = "present for no flags, hidden by silph_cleared"},
    },
    {
        .input = {.name = "16.02 fights the Rocket by the corridor for 16.03's start flag", .body = test_corridor_rocket_fought},
        .expectation = {.description = "a trainer_wait step, then his battle with trainer_beaten:SilphCo_5F/rocket_m@9,21 after it; 16.03 starts with the flag"},
    },
    {
        .input = {.name = "the corridor Rocket stands where he stopped after 16.02", .body = test_corridor_rocket_stands_where_he_stopped},
        .expectation = {.description = "16.02's battle sets trainer_at:SilphCo_5F/rocket_m@9,21=11,21; 16.03 starts with it; the run's maps have him on (11, 21), not (9, 21)"},
    },
    {
        .input = {.name = "silph_cleared is set by the Giovanni step", .body = test_silph_cleared_flag},
        .expectation = {.description = "16.05's giovanni sets it; 16.05b and 16.06 start with it"},
    },
    {
        .input = {.name = "the Master Ball option and SPEEDRUN", .body = test_master_ball_option},
        .expectation = {.description = "skipped when the option is off or in a SPEEDRUN"},
    },
    {
        .input = {.name = "16.03 the rival by PokeGuy's C2 policy", .body = test_rival_fought_by_policy},
        .expectation = {.description = "goal-based-setup to Sp. Atk +2 and accuracy +2, outspeed, PokemonSimulation planning_risk() rows"},
    },
    {
        .input = {.name = "16.03 TM14 Blizzard before the rival", .body = test_blizzard_before_rival},
        .expectation = {.description = "a TEACH first: Blizzard over Dig, nothing if the lead knows it"},
    },
    {
        .input = {.name = "16.03 the lead's attack against the rival's Venusaur", .body = test_blizzard_against_venusaur},
        .expectation = {.description = "Blizzard"},
    },
};

} // namespace


void add_tests_AutoStorySilph(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SILPH_CASES){
        database.add<Test_AutoStorySilph>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
