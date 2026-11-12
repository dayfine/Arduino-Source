/*  BDSP AutoStory Wander Wait Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Mart clown wanders (54..56, 32..34) round (55, 33). 00.05b and
 *  00.05c wait for him in his box's end columns, 56 facing west, then 54
 *  facing east (wait_for_wanderer): these run that wait on the simulation
 *  with him starting, or staying, on each tile of his box, and pick the
 *  tile to wait on first. He never steps onto the player's tile.
 *
 */

#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_WanderWait.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_SimWanderWait.h"
#include "PokemonBDSP_AutoStory_WanderWait_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::TileBox;
using Pokemon::TilePos;


namespace{

//  Jubilife alone, with the Mart clown wandering his box instead of
//  standing on (55, 33). One map at (0, 0): world tiles are its own.
StoryWorld clown_world(){
    Pokemon::MapData map = story_map("JubilifeCity");
    for (Pokemon::MapObject& object : map.objects){
        if (object.name == "clown_mart"){
            object.wander = jubilife_mart_clown_box();
        }
    }
    StoryWorld world;
    world.add("JubilifeCity", map, {0, 0});
    return world;
}

//  The step `name` of row `row`.
const Pokemon::StoryStep& story_step(const std::string& row, const std::string& name){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        if (checkpoint.id != row){
            continue;
        }
        for (const Pokemon::StoryStep& step : checkpoint.steps){
            if (step.name == name){
                return step;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No step {} in row {}.", name, row));
}

//  Where the clown may start: each tile of his box but `player`'s.
std::vector<TilePos> clown_starts(const TilePos& player){
    std::vector<TilePos> starts;
    const TileBox box = jubilife_mart_clown_box();
    for (int y = box.y0; y <= box.y1; y++){
        for (int x = box.x0; x <= box.x1; x++){
            if (TilePos{x, y} != player){
                starts.push_back({x, y});
            }
        }
    }
    return starts;
}

//  The wait from `start` with the clown starting on `clown`, his walk
//  seeded by `seed`; with `stays`, he never steps.
WanderWaitResult run_wait(const StoryWorld& world, const TilePos& start, const TilePos& clown, uint64_t seed, const WanderWaitPlan& plan, bool stays = false){
    SimWanderer wanderer{.name = "clown_mart", .box = jubilife_mart_clown_box(), .pos = clown, .seed = seed};
    if (stays){
        wanderer.step_interval = Milliseconds(0);
    }
    GridSim sim({.world = &world, .wanderers = {wanderer}}, start, Direction::NORTH, false);
    SimWanderWait body(sim);
    const WanderWaitResult result = wait_beside_wanderer(body, start, plan);
    if (nearest_tile(sim.player().pos) != result.at){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("the wait counted {}, the player is on {}.",
            Pokemon::to_string(result.at), Pokemon::to_string(nearest_tile(sim.player().pos))));
    }
    return result;
}

constexpr uint64_t SEEDS = 8;

//  00.05b comes up column 56 from the wall (56, 36), 00.05c down it from
//  (56, 31): from each, the wait talks to the clown wherever in his box he
//  starts, over seeds 1 to 8, and ends on (56, 31).
UnitTestResult test_mart_clown_reached_from_column_56(Logger& logger){
    const StoryWorld world = clown_world();
    std::string out;
    size_t runs = 0;
    for (const auto& [row, start] : {std::pair<std::string, TilePos>{"00.05b", {56, 36}}, {"00.05c", {56, 31}}}){
        const WanderWaitPlan plan = wander_wait_plan(story_step(row, "mart_clown"));
        for (const TilePos& clown : clown_starts(start)){
            for (uint64_t seed = 1; seed <= SEEDS; seed++){
                runs++;
                const WanderWaitResult result = run_wait(world, start, clown, seed, plan);
                if (result.end != WanderWaitEnd::TALKED || result.at != plan.exit){
                    out += std::format("\n  {} with the clown on {}, seed {}: ended {} on {}.", row,
                        Pokemon::to_string(clown), seed, (int)result.end, Pokemon::to_string(result.at));
                }
            }
        }
    }
    logger.log(std::format("{} waits run.", runs));
    return out.empty() ? UnitTestResult(true) : UnitTestResult(std::format("The wait missed the clown:{}", out));
}

//  On lab-B (2026-11-12 11:00) the clown stayed out of reach of column 56
//  for the whole wait, on his box's west column. From either start, the
//  wait talks to him on whichever tile of his box he stays.
UnitTestResult test_mart_clown_reached_when_he_stays(Logger& logger){
    const StoryWorld world = clown_world();
    std::string out;
    size_t runs = 0;
    for (const auto& [row, start] : {std::pair<std::string, TilePos>{"00.05b", {56, 36}}, {"00.05c", {56, 31}}}){
        const WanderWaitPlan plan = wander_wait_plan(story_step(row, "mart_clown"));
        for (const TilePos& clown : clown_starts(start)){
            runs++;
            const WanderWaitResult result = run_wait(world, start, clown, 1, plan, true);
            if (result.end != WanderWaitEnd::TALKED || result.at != plan.exit){
                out += std::format("\n  {} with the clown staying on {}: ended {} on {}.", row,
                    Pokemon::to_string(clown), (int)result.end, Pokemon::to_string(result.at));
            }
        }
    }
    logger.log(std::format("{} waits run.", runs));
    return out.empty() ? UnitTestResult(true) : UnitTestResult(std::format("The wait missed the clown:{}", out));
}

//  The simulation's wait, counting its asks.
class CountingWanderWait : public SimWanderWait{
public:
    using SimWanderWait::SimWanderWait;
    virtual bool ask() override{
        asks++;
        return SimWanderWait::ask();
    }
    size_t asks = 0;
};

//  On lab-B (2026-11-12 11:03) 00.05b ended "no talk from any stand tile;
//  on (56, 32)" with the clown diagonal, on (55, 33): A faced nobody, a
//  dozen times. With him staying there and the wait on (56, 32), it steps
//  to his row and talks within a few asks.
UnitTestResult test_mart_clown_diagonal_reached(Logger& logger){
    const StoryWorld world = clown_world();
    std::string out;
    for (const std::string row : {"00.05b", "00.05c"}){
        const WanderWaitPlan table_plan = wander_wait_plan(story_step(row, "mart_clown"));
        WanderWaitPlan plan = table_plan;
        //  Its east side's walk, from (56, 32) on.
        std::vector<TilePos>& stands = plan.sides.at(0).stands;
        const auto at = std::find(stands.begin(), stands.end(), TilePos{56, 32});
        if (at == stands.end()){
            return row + ": no stand on (56, 32).";
        }
        std::rotate(stands.begin(), at, stands.end());
        SimWanderer clown{.name = "clown_mart", .box = jubilife_mart_clown_box(), .pos = {55, 33}, .seed = 1};
        clown.step_interval = Milliseconds(0);
        GridSim sim({.world = &world, .wanderers = {clown}}, {56, 32}, Direction::WEST, false);
        CountingWanderWait body(sim);
        const WanderWaitResult result = wait_beside_wanderer(body, {56, 32}, plan);
        logger.log(std::format("{}: ended {} after {} asks, talked on {}.", row, (int)result.end, body.asks, Pokemon::to_string(result.talked)));
        if (result.end != WanderWaitEnd::TALKED || result.talked != TilePos{56, 33} || body.asks > 3){
            out += std::format("\n  {}: ended {} on {} after {} asks, talked on {}.", row, (int)result.end,
                Pokemon::to_string(result.at), body.asks, Pokemon::to_string(result.talked));
        }
    }
    return out.empty() ? UnitTestResult(true) : UnitTestResult(std::format("The wait stayed off his row:{}", out));
}

//  How many of the clown's starts and seeds one stand tile alone talks
//  to him from, facing west, within the plan's asks.
size_t talks_from(const StoryWorld& world, const TilePos& stand, const WanderWaitPlan& table_plan){
    WanderWaitPlan plan = table_plan;
    plan.sides = {{.stands = {stand}, .facing = Direction::WEST}};
    plan.asks = 12;
    plan.passes = 1;
    plan.rounds = 1;
    size_t talks = 0;
    for (const TilePos& clown : clown_starts(stand)){
        for (uint64_t seed = 1; seed <= SEEDS; seed++){
            talks += run_wait(world, stand, clown, seed, plan).end == WanderWaitEnd::TALKED ? 1 : 0;
        }
    }
    return talks;
}

//  The first stand tile is the one of (56, 32..34) the simulation finds
//  the clown level with most often.
UnitTestResult test_mart_clown_first_stand(Logger& logger){
    const StoryWorld world = clown_world();
    const WanderWaitPlan plan = wander_wait_plan(story_step("00.05b", "mart_clown"));
    size_t best = 0;
    int best_y = 0;
    for (int y = 32; y <= 34; y++){
        const size_t talks = talks_from(world, {56, y}, plan);
        logger.log(std::format("From (56, {}): {} talks of {} starts and seeds.", y, talks, clown_starts({56, y}).size() * SEEDS));
        if (talks > best){
            best = talks;
            best_y = y;
        }
    }
    for (const std::string row : {"00.05b", "00.05c"}){
        const TilePos first = wander_wait_plan(story_step(row, "mart_clown")).sides.at(0).stands.at(0);
        if (first != TilePos{56, best_y}){
            return std::format("{} waits first on {}; the simulation talks most from (56, {}).", row, Pokemon::to_string(first), best_y);
        }
    }
    return true;
}

struct WanderWaitInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};
struct WanderWaitExpectation{};
struct WanderWaitCase{
    WanderWaitInput input;
    WanderWaitExpectation expectation;
};

class Test_AutoStoryWanderWait : public UnitTest{
public:
    explicit Test_AutoStoryWanderWait(WanderWaitCase test_case)
        : UnitTest(std::format("PokemonBDSP::AutoStory - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    WanderWaitCase m_case;
};

const WanderWaitCase WANDER_WAIT_CASES[] = {
    {.input = {"Wander wait: the Mart clown is reached from column 56 wherever he starts (00.05b, 00.05c)",
        test_mart_clown_reached_from_column_56}, .expectation = {}},
    {.input = {"Wander wait: the Mart clown is reached on whichever tile he stays (lab-B 2026-11-12, column 54)",
        test_mart_clown_reached_when_he_stays}, .expectation = {}},
    {.input = {"Wander wait: the Mart clown diagonal to (56, 32) is reached on his row within 3 asks (lab-B 2026-11-12 11:03)",
        test_mart_clown_diagonal_reached}, .expectation = {}},
    {.input = {"Wander wait: the Mart clown's first stand tile is where the simulation meets him most",
        test_mart_clown_first_stand}, .expectation = {}},
};

} // namespace


void add_tests_AutoStory_WanderWait(UnitTestDatabase& database){
    for (const auto& item : WANDER_WAIT_CASES){
        database.add<Test_AutoStoryWanderWait>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
