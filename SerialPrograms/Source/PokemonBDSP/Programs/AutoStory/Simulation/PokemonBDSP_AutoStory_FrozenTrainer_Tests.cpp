/*  BDSP AutoStory Grid Simulation - Frozen Trainer Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A trainer the map lacks sees the player one tile short of a leg's wall:
 *  the "!" freezes the player, the hold reads as stopped, as at the wall,
 *  and the dialogue opens later. Lab-B 2026-11-12 17:45, 01.02 by_the_gate:
 *  after the Youngster's battle the walk east along Route 203's row 18 was
 *  frozen at (54, 18), the stop was taken for the wall at (55, 18), and
 *  every later leg ran one column off.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_TrainerSight.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_FrozenTrainer_Tests.h"
#include "PokemonBDSP_AutoStory_GridSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace detail;


namespace{

using Pokemon::Direction;
using Pokemon::TilePos;


//  Row 16 of 18, with a block between the columns 1-2 and 11-12 above it:
//  east along the bottom rows to the wall, then north up column 12.
StoryWorld make_room(){
    Pokemon::MapData map;
    map.name = "Test";
    map.tiles = Pokemon::Grid<Pokemon::TileKind>{14, 18, {}};
    for (int y = 0; y < 18; y++){
        for (int x = 0; x < 14; x++){
            const bool wall = x == 0 || x == 13 || y == 0 || y == 17 || (y <= 12 && x >= 3 && x <= 10);
            map.tiles.cells.push_back(*Pokemon::tile_from_glyph(wall ? '#' : '.'));
        }
    }
    StoryWorld world;
    world.add("Test", map, {0, 0});
    return world;
}

//  The hold toward a wall that a trainer's sight stops reads as stopped, as
//  at a wall, when the trainer is `trainer`; `seen`: whether a look right
//  after finds their dialogue open. If not, it is open on the next input.
class FrozenTrainerRouteIO : public GridSimRouteIO{
public:
    FrozenTrainerRouteIO(GridSim& sim, std::vector<std::string>& notes, std::string trainer, bool seen)
        : GridSimRouteIO(sim, notes)
        , m_sim(sim)
        , m_trainer(std::move(trainer))
        , m_seen(seen)
    {}

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override{
        m_frozen = false;
        const bool open = m_sim.player().dialog;
        WalkResult result = GridSimRouteIO::walk_to_wall(walk);
        if (!open && result.stop == WalkStop::DIALOG && m_sim.player().trainer == m_trainer){
            result.stop = WalkStop::BLOCKED;
            m_frozen = !m_seen;
        }
        return result;
    }
    virtual void press(const RouteInput& input) override{
        m_frozen = false;
        GridSimRouteIO::press(input);
    }
    virtual RouteSight sight_within(const RouteWatch& watch) override{
        RouteSight sight = GridSimRouteIO::sight_within(watch);
        sight.dialog = sight.dialog && !m_frozen;
        return sight;
    }

private:
    GridSim& m_sim;
    std::string m_trainer;
    bool m_seen;
    bool m_frozen = false;
};

struct FrozenTrainerInput{
    std::string name;
    //  Whether a look right after the frozen stop finds the dialogue open.
    bool seen;
};

struct FrozenTrainerExpectation{
    //  Where the route must end, by the wall at x = 12.
    TilePos end;
};

struct FrozenTrainerCase{
    FrozenTrainerInput input;
    FrozenTrainerExpectation expectation;
};

//  East along row 16 meets a known trainer at (6, 14), whose sight covers
//  (6, 15) and (6, 16); after the battle the walk on to the wall is frozen
//  at (11, 16), a tile short, by a trainer the map lacks at (11, 13). The
//  route must still end on (12, 4), by the wall at (12, 16) and the wall at
//  (12, 1).
UnitTestResult run_frozen_trainer(Logger& logger, const FrozenTrainerCase& item){
    const StoryWorld room = make_room();
    const TilePos start{1, 16};
    const TilePos target = item.expectation.end;
    const TrainerSight known{.name = "known", .pos = {6, 14}, .facing = Direction::SOUTH, .range = 2};
    const TrainerSight hidden{.name = "hidden", .pos = {11, 13}, .facing = Direction::SOUTH, .range = 3};
    GridSim sim({.world = &room, .trainers = {known, hidden}}, start, Direction::NORTH, true);
    std::vector<std::string> notes;
    FrozenTrainerRouteIO sim_io(sim, notes, "hidden", item.input.seen);
    RouteTrainers trainers{{known}, [&sim](const std::string&){ sim.end_dialogue(); return true; }};
    std::vector<TilePos> believed;
    std::string failure;
    try{
        believed = walk_route(sim_io, room, {start}, trainers, {
            .goal = [&](const TilePos& pos){ return pos == target; },
            .label = "by_the_gate",
            .avoid = {},
            .costs = *story_route_costs("town"),
            .speed = MoveSpeed::RUN,
        });
    }catch (const GridSimFailure& e){
        failure = "failed: " + e.message;
    }
    for (const std::string& note : notes){
        logger.log(note);
    }
    const TilePos at = nearest_tile(sim.player().pos);
    if (!failure.empty()){
        return failure;
    }
    if (at != target || believed != std::vector<TilePos>{target}){
        return std::format("ended on ({}, {}), believed {} tile(s), not on ({}, {}).", at.x, at.y, believed.size(), target.x, target.y);
    }
    return true;
}

class Test_FrozenTrainer : public UnitTest{
public:
    explicit Test_FrozenTrainer(FrozenTrainerCase item)
        : UnitTest("PokemonBDSP::AutoStory - Frozen trainer: " + item.input.name)
        , m_case(std::move(item))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return run_frozen_trainer(logger, m_case);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    FrozenTrainerCase m_case;
};

const FrozenTrainerCase FROZEN_TRAINER_CASES[] = {
    {.input = {.name = "01.02, a trainer the map lacks freezes us a tile short of the wall, found on the stop (lab-B, 2026-11-12 17:45)", .seen = true}, .expectation = {.end = {12, 4}}},
    {.input = {.name = "01.02, a trainer the map lacks freezes us a tile short of the wall, talks on the next leg (lab-B, 2026-11-12 17:45)", .seen = false}, .expectation = {.end = {12, 4}}},
};

} // namespace


void add_tests_AutoStory_FrozenTrainer(UnitTestDatabase& database){
    for (const FrozenTrainerCase& item : FROZEN_TRAINER_CASES){
        database.add<Test_FrozenTrainer>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
