/*  AutoStory League Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 20.01a to 20.05 (StoryData/frlg-20.0N.plan.json) on the overworld
 *  sim: Indigo Plateau's Mart, the Elite Four's rooms one way north, the
 *  Champion's door; and the battles' foe rows against their generator.
 *
 */

#include <cstdint>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows20_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryLeagueInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryLeagueExpectation{
    std::string_view description;
};

struct AutoStoryLeagueCase{
    AutoStoryLeagueInput input;
    AutoStoryLeagueExpectation expectation;
};

class Test_AutoStoryLeague : public UnitTest{
public:
    explicit Test_AutoStoryLeague(AutoStoryLeagueCase test_case)
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
    AutoStoryLeagueCase m_case;
};

const StoryCheckpoint& league_row(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  Runs the walking steps of `row` on `sim` up to (not including) the step
//  named `until`, or all of them if `until` is empty. The other steps (the
//  Mart, the level checks, the heals, the scenes and the battles) are the
//  console's.
std::string walk_league_row(OverworldSim& sim, StoryPose& at, const StoryCheckpoint& row, std::string_view until){
    for (const StoryStep& step : row.steps){
        if (step.name == until){
            return "";
        }
        if (step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS && step.kind != StoryStepKind::STEP){
            continue;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
    }
    return until.empty() ? "" : std::format("{}: no step {}.", row.id, until);
}

//  Walks row `id` from its start to the step `until` (the room's entry
//  scene) and checks the door before it took the player into `room`, with
//  no trainer met on the way.
UnitTestResult walk_to_room(std::string_view id, std::string_view until, std::string_view room){
    const StoryCheckpoint& row = league_row(id);
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_league_row(sim, at, row, until); !failure.empty()){
        return failure;
    }
    if (sim.state().position.map != room){
        return std::format("{}: the door led to {}, not {}.", id, to_string(sim.state().position), room);
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return std::format("{}: met a trainer or broke a rule on the way.", id);
    }
    return true;
}

//  Walks row `id` from in front of the beaten trainer round them to below
//  their door, up to the step `door` into it. The map pack draws the door
//  closed (a wall: the game opens it with setmetatile when the trainer is
//  beaten, PokemonLeague_EventScript_OpenDoor), so the step through it and
//  the next room's walk are the console's.
UnitTestResult walk_to_door(std::string_view id, std::string_view door, const MapPosition& below){
    const StoryCheckpoint& row = league_row(id);
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_league_row(sim, at, row, door); !failure.empty()){
        return failure;
    }
    if (!(sim.state().position == below) || sim.state().facing != Direction::NORTH){
        return std::format("{}: ended on {}, not below the door {} facing north.", id, to_string(sim.state().position), to_string(below));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return std::format("{}: met a trainer or broke a rule on the way.", id);
    }
    return true;
}

//  20.01a: the lobby to the Mart's counter, where 20.01 starts.
UnitTestResult test_indigo_mart_counter(){
    const StoryCheckpoint& shop = league_row("20.01a");
    StoryPose at = *story_pose(Pokemon::pose_of(*shop.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_league_row(sim, at, shop, ""); !failure.empty()){
        return failure;
    }
    const MapPosition counter{"IndigoPlateau_PokemonCenter_1F", {2, 7}};
    if (!(sim.state().position == counter)){
        return std::format("20.01a: ended on {}, not the Mart's counter {}.", to_string(sim.state().position), to_string(counter));
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*league_row("20.01").start));
    if (next.map != counter.map || !(next.tile == counter.point)){
        return "20.01 doesn't start where 20.01a ends.";
    }
    return true;
}

//  The step `name` of row `id`.
const StoryStep& league_step(std::string_view id, std::string_view name){
    for (const StoryStep& step : league_row(id).steps){
        if (step.name == name){
            return step;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No step {} in {}.", name, id));
}

//  The battle's foe_threats are PokemonSimulation's planning_risk() for
//  each foe leading against the entry state of its pokeguy case
//  (src/pokesim/games/frlg/battle/player_strategy/planning.py, as 19.01's
//  rival): speed, the 80th percentile loss, the mean (rounded), the worst
//  case and the worst normal hit, then whether the foe lowers accuracy
//  (Sand Attack) and whether it explodes.
UnitTestResult check_foe_threats(std::string_view id, std::string_view battle, const std::vector<std::vector<int64_t>>& expected){
    const StoryStep& step = league_step(id, battle);
    const auto found = step.args.find("foe_threats");
    const auto* rows = found == step.args.end() ? nullptr : std::get_if<std::vector<std::vector<int64_t>>>(&found->second);
    if (rows == nullptr || *rows != expected){
        return std::format("{} {}: foe_threats are not the generator's rows.", id, battle);
    }
    return true;
}

UnitTestResult test_foe_threats_are_the_generators(){
    struct Fight{
        std::string_view id;
        std::string_view battle;
        std::vector<std::vector<int64_t>> rows;
    };
    const Fight fights[] = {
        {"20.01", "lorelei", {
            {93, 21, 13, 45, 22, 0, 0},     //  Dewgong
            {91, 0, 0, 0, 15, 0, 0},        //  Cloyster
            {45, 27, 9, 60, 30, 0, 0},      //  Slowbro
            {110, 25, 9, 57, 28, 0, 0},     //  Jynx
            {94, 36, 15, 78, 39, 0, 0},     //  Lapras
        }},
        {"20.02", "bruno", {
            {81, 32, 19, 75, 37, 0, 0},     //  Onix
            {101, 51, 28, 132, 66, 0, 0},   //  Hitmonchan
            {113, 65, 52, 136, 68, 0, 0},   //  Hitmonlee
            {96, 41, 41, 84, 42, 0, 0},     //  Onix
            {83, 0, 5, 183, 91, 0, 0},      //  Machamp
        }},
        {"20.03", "agatha", {
            {126, 32, 10, 72, 36, 0, 0},    //  Gengar
            {129, 33, 31, 69, 34, 0, 0},    //  Golbat
            {108, 0, 0, 0, 48, 0, 0},       //  Haunter
            {111, 61, 61, 126, 63, 0, 0},   //  Arbok
            {150, 47, 40, 99, 49, 0, 0},    //  Gengar
        }},
        {"20.04", "lance", {
            {112, 40, 32, 42, 93, 0, 0},    //  Gyarados
            {96, 40, 17, 87, 62, 0, 0},     //  Dragonair
            {96, 0, 0, 0, 62, 0, 0},        //  Dragonair
            {173, 47, 47, 96, 78, 0, 0},    //  Aerodactyl
            {107, 64, 24, 144, 114, 0, 0},  //  Dragonite
        }},
        {"20.05", "champion", {
            {130, 41, 27, 87, 43, 1, 0},    //  Pidgeot (Sand Attack)
            {159, 77, 70, 159, 79, 0, 0},   //  Alakazam
            {70, 0, 10, 210, 105, 0, 0},    //  Rhydon
            {118, 51, 46, 110, 55, 0, 0},   //  Gyarados
            {139, 50, 51, 104, 52, 0, 0},   //  Arcanine
            {125, 217, 218, 444, 222, 0, 0},//  Venusaur
        }},
    };
    for (const Fight& fight : fights){
        UnitTestResult result = check_foe_threats(fight.id, fight.battle, fight.rows);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    return true;
}

const AutoStoryLeagueCase AUTO_STORY_LEAGUE_CASES[] = {
    {
        .input = {.name = "20.01a Indigo Plateau's Mart counter", .body = test_indigo_mart_counter},
        .expectation = {.description = "the counter (2, 7), where 20.01 starts"},
    },
    {
        .input = {.name = "20.01 the League's door, into Lorelei's room", .body = []{
            return walk_to_room("20.01", "enter_loreleis_room", "PokemonLeague_LoreleisRoom");
        }},
        .expectation = {.description = "Lorelei's room, no trainer met"},
    },
    {
        .input = {.name = "20.02 around Lorelei to below her door", .body = []{
            return walk_to_door("20.02", "loreleis_door", MapPosition{"PokemonLeague_LoreleisRoom", {6, 3}});
        }},
        .expectation = {.description = "Lorelei's room (6, 3) facing north"},
    },
    {
        .input = {.name = "20.03 around Bruno to below his door", .body = []{
            return walk_to_door("20.03", "brunos_door", MapPosition{"PokemonLeague_BrunosRoom", {6, 3}});
        }},
        .expectation = {.description = "Bruno's room (6, 3) facing north"},
    },
    {
        .input = {.name = "20.04 around Agatha to below her door", .body = []{
            return walk_to_door("20.04", "agathas_door", MapPosition{"PokemonLeague_AgathasRoom", {6, 3}});
        }},
        .expectation = {.description = "Agatha's room (6, 3) facing north"},
    },
    {
        .input = {.name = "20.05 around Lance to below the Champion's door", .body = []{
            return walk_to_door("20.05", "champions_door", MapPosition{"PokemonLeague_LancesRoom", {6, 6}});
        }},
        .expectation = {.description = "Lance's room (6, 6) facing north"},
    },
    {
        .input = {.name = "20.01 to 20.05 the League's foe rows are the generator's", .body = test_foe_threats_are_the_generators},
        .expectation = {.description = "PokemonSimulation planning_risk() rows"},
    },
};

} // namespace


void add_tests_AutoStoryLeague(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_LEAGUE_CASES){
        database.add<Test_AutoStoryLeague>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
