/*  AutoStory Surf Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 12.03's water (StoryData/frlg-12.03.plan.json) on the overworld sim:
 *  each leg on Route 21's water lands on a sand bar, an island or the shore
 *  and ends at its bank, or at a wall, or goes a single tile, never further
 *  on open water, where a stop part-way can't be placed (console B, 2026-11-13 04:15, 06:55 and 07:02).
 *  The faults seen there are injected at every step on the water.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStorySurf_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Where 12.03 ends: below the Pokemon Mansion's door, (8, 4).
const MapPosition MANSION_DOOR{"CinnabarIsland", {8, 4}};

const StoryCheckpoint& row_12_03(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& data){ return data.id == "12.03"; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint 12.03.");
    }
    return *row;
}

//  A table pose as a StoryPose; FRLG's table has one-tile poses only.
StoryPose one_tile(const Pokemon::GridPose& grid_pose){
    const std::optional<StoryPose> ret = story_pose(Pokemon::pose_of(grid_pose));
    if (!ret){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Not a one-tile pose.");
    }
    return *ret;
}

//  A leg on the water that ends where the player can be found again: at a
//  wall (the bank of a sand bar it lands on, or the map's edge) or a warp,
//  or a tile on, which can't stop part-way.
bool ends_found(const RouteStep& step){
    return step.action != RouteAction::WALK || !step.surfing || step.to_wall || step.warps || step.tiles == 1;
}

//  What a 12.03 run on the sim did: its failure (empty if none), where it
//  ended, the steps it surfed and the legs it planned on the water that
//  end in the open.
struct SurfRun{
    std::string failure;
    MapPosition end;
    int surf_steps = 0;
    std::vector<int> surf_holds;
    std::vector<std::string> open_water_legs;
    std::vector<std::string> trainers;
    //  The ROUTE steps that walk back the way they came.
    std::vector<std::string> back_tracks;
};

Direction opposite(Direction direction){
    return (Direction)(((int)direction + 2) % 4);
}

//  The first leg of `route` that walks back against an earlier leg's way,
//  other than the one tile off a wall a hold just ended at, the way to
//  start a leg on a column or row with no wall of its own. Nothing if none.
std::optional<RouteStep> walks_back(const std::vector<RouteStep>& route){
    bool walked[4] = {};
    const RouteStep* before = nullptr;
    for (const RouteStep& step : route){
        if (step.action != RouteAction::WALK){
            continue;
        }
        const bool off_wall = step.tiles == 1 && before != nullptr && before->to_wall;
        if (walked[(int)opposite(step.direction)] && !off_wall){
            return step;
        }
        walked[(int)step.direction] = true;
        before = &step;
    }
    return std::nullopt;
}

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n  " + line;
    }
    return text;
}

//  Adds to `back_tracks` the leg of `route`, ROUTE step `name`'s, that walks
//  back (walks_back()), if any.
void note_walk_back(const std::string& name, const std::vector<RouteStep>& route, std::vector<std::string>& back_tracks){
    const std::optional<RouteStep> back = walks_back(route);
    if (!back){
        return;
    }
    std::string legs;
    for (const RouteStep& planned : route){
        legs += std::format("; {}", to_string(planned));
    }
    back_tracks.push_back(std::format("{}: {}, in{}", name, to_string(*back), legs));
}

//  12.03 on a sim with `script`, from Fuchsia's nurse to the Mansion's
//  door on Cinnabar, as test_fuchsia_to_cinnabar() runs it. Each ROUTE
//  step is also planned from where the sim stands, for its water legs.
SurfRun run_12_03(SimScript script = {}){
    const StoryCheckpoint& row = row_12_03();
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing, std::move(script));
    SurfRun run;
    for (const StoryStep& step : row.steps){
        if (step.action == CENTER_ENTER_VARIANT){
            break;
        }
        if (step.kind == StoryStepKind::ROUTE){
            const StoryPose target = one_tile(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target"));
            const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
                story_maps(), sim.state().position, RouteGoal{{target.map, target.tile}, target.facing},
                story_route_costs(), sim.state().facing
            );
            for (const RouteStep& planned : route.value_or(std::vector<RouteStep>())){
                if (!ends_found(planned)){
                    run.open_water_legs.push_back(step.name + ": " + to_string(planned));
                }
            }
            if (route){
                note_walk_back(step.name, *route, run.back_tracks);
            }
        }
        run.failure = row_step_on_sim(sim, at, step, row.id);
        if (!run.failure.empty()){
            break;
        }
    }
    run.end = sim.state().position;
    run.surf_steps = sim.state().surf_steps;
    run.surf_holds = sim.state().surf_holds;
    if (run.failure.empty() && !sim.state().violations.empty()){
        run.failure = "a violation: " + sim.state().violations.back();
    }
    run.trainers = sim.state().trainers_beaten;
    return run;
}

//  No leg on Route 21's water ends in the open: each lands and is held to
//  the bank, or ends at a wall. Before, "south 15 (open)" from (7, 0) was
//  stopped 8 tiles in by a message, and the player wasn't found again
//  (console B, 2026-11-13 06:55 and 07:02).
UnitTestResult test_no_open_water_legs(){
    const SurfRun run = run_12_03();
    if (!run.failure.empty()){
        return "12.03 failed on the sim: " + run.failure;
    }
    if (run.surf_steps == 0){
        return "12.03 surfed no step on the sim.";
    }
    if (!run.open_water_legs.empty()){
        return "12.03 plans legs on the water that end in the open:" + joined(run.open_water_legs);
    }
    //  Swimmer Roland wanders Route21_South's (8..12, 27..31) and sees 4
    //  tiles each way: every way from sand bar to sand bar south of
    //  (5..11, 21..25) crosses his sight. He is the one trainer fought.
    if (run.trainers.size() > 1){
        return "12.03 meets more than Swimmer Roland:" + joined(run.trainers);
    }
    return true;
}

//  No ROUTE step of 12.03 walks back the way it came, but for a tile off a
//  wall it was held to.
//  Before, land_middle_sand_bar went south 5, west 1, north 3 and east 2,
//  a loop, to start its hold down column 6 from a wall (12.03 run 7,
//  console B 2026-11-13 13:31), and land_south_sand_bar went east 11 and
//  west 10 round the rocks below (7, 32) where one tile east would do: the
//  planner weighed a one-tile leg on the water ending in the open as
//  if a message could stop it part-way.
UnitTestResult test_no_back_tracking(){
    const SurfRun run = run_12_03();
    if (!run.failure.empty()){
        return std::format("12.03 failed on the sim: {}", run.failure);
    }
    if (!run.back_tracks.empty()){
        return std::format("12.03 plans legs that walk back:{}", joined(run.back_tracks));
    }
    return true;
}

//  A message stops the hold after every step on the water in turn, as
//  "a message" did 8 tiles into "south 15 (open)" (console B, 2026-11-13
//  06:55 and 07:02): each run still ends below the Mansion's door, the
//  leg held on to its bank after the message.
UnitTestResult test_message_at_every_surf_step(){
    const SurfRun clean = run_12_03();
    if (!clean.failure.empty()){
        return "12.03 failed on the sim: " + clean.failure;
    }
    std::vector<std::string> failed;
    for (int step = 1; step <= clean.surf_steps; step++){
        SimScript script;
        script.messages_at_surf_steps = {step};
        const SurfRun run = run_12_03(std::move(script));
        if (!run.failure.empty() || !(run.end == MANSION_DOOR)){
            failed.push_back(std::format(
                "message after surfed step {}: ended on {}{}", step, to_string(run.end),
                run.failure.empty() ? "" : "; " + run.failure
            ));
        }
    }
    if (!failed.empty()){
        return std::format("{} of {} runs with a message on the water fail:{}", failed.size(), clean.surf_steps, joined(failed));
    }
    return true;
}

//  A hold on the water that stops short with nothing seen, as "south 86
//  (open)" did (console B, 2026-11-13 04:15), at each in turn: the run
//  ends below the Mansion's door, or fails (a leg that lands checks the
//  player is on foot); it never goes on from where it wrongly takes the
//  player to be and ends elsewhere.
UnitTestResult test_short_hold_never_goes_on_from_a_guess(){
    const SurfRun clean = run_12_03();
    if (!clean.failure.empty()){
        return "12.03 failed on the sim: " + clean.failure;
    }
    std::vector<std::string> wrong;
    if (clean.surf_holds.empty()){
        return "12.03 held no leg on the water on the sim.";
    }
    for (const int hold : clean.surf_holds){
        SimScript script;
        script.short_holds = {{hold, 3}};
        const SurfRun run = run_12_03(std::move(script));
        if (run.failure.empty() && !(run.end == MANSION_DOOR)){
            wrong.push_back(std::format("hold {} 3 tiles short: passed, ended on {}", hold, to_string(run.end)));
        }
        if (run.failure.find("trainer") != std::string::npos || run.failure.find("violation") != std::string::npos){
            wrong.push_back(std::format("hold {} 3 tiles short: {}", hold, run.failure));
        }
    }
    if (!wrong.empty()){
        return "12.03 went on from a guess after a short hold:" + joined(wrong);
    }
    return true;
}

//  Swimmer Spencer goes round Route21_North's (8..16, 42..46), looking a
//  tile ahead (pokefirered: a walk sequence from (12, 44), range 4 by 2):
//  column 10 crosses his line, which the map pack, with him on (12, 44)
//  alone, left out. 12.03's hold down column 10 met him after a wild
//  Tentacool (console B, 2026-11-13 10:07).
UnitTestResult test_spencer_walks(){
    for (const MapData& map : story_maps()){
        if (map.name != "Route21_North"){
            continue;
        }
        for (const MapObject& object : map.objects){
            if (object.name != "swimmer_m_water" || object.x != 12 || object.y != 44){
                continue;
            }
            const bool box = object.wander && object.wander->x0 == 8 && object.wander->y0 == 42
                && object.wander->x1 == 16 && object.wander->y1 == 46;
            if (!box){
                return "Swimmer Spencer stands on (12, 44) alone; he walks (8..16, 42..46).";
            }
            for (const TilePos& tile : {TilePos{10, 41}, TilePos{10, 47}, TilePos{7, 44}}){
                if (!trainer_may_see(map, object, tile)){
                    return std::format("Swimmer Spencer doesn't see {}.", to_string(tile));
                }
            }
            if (trainer_may_see(map, object, TilePos{10, 40})){
                return "Swimmer Spencer sees (10, 40), two tiles from his walk.";
            }
            return true;
        }
        return "No Swimmer Spencer on Route21_North.";
    }
    return "No Route21_North.";
}

//  A box open as each Surf step of 12.03 starts, from where the ROUTE step
//  before it ends, as one was before surf_off_middle_sand_bar: Surf's A
//  closed it and no prompt came (console B, 2026-11-13). Each step clears
//  the box with B first and surfs onto the tile it declares.
UnitTestResult test_box_open_before_each_surf(){
    const StoryCheckpoint& row = row_12_03();
    std::optional<StoryPose> before;
    std::vector<std::string> failed;
    int surfs = 0;
    for (const StoryStep& step : row.steps){
        if (step.kind == StoryStepKind::ROUTE){
            before = one_tile(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target"));
            continue;
        }
        const std::optional<Direction> water = parse_surf_step(step);
        if (!water || !before){
            continue;
        }
        surfs++;
        SimScript script;
        script.message_open = true;
        OverworldSim sim(story_maps(), MapPosition{before->map, before->tile}, before->facing.value_or(*water), std::move(script));
        StoryPose at = *before;
        std::string failure = row_step_on_sim(sim, at, step, row.id);
        if (failure.empty() && !sim.state().violations.empty()){
            failure = std::format("a violation: {}", sim.state().violations.back());
        }
        if (!failure.empty()){
            failed.push_back(failure);
        }
    }
    if (surfs == 0){
        return "12.03 has no Surf step after a ROUTE step.";
    }
    if (!failed.empty()){
        return std::format("{} of {} Surf steps fail with a box open:{}", failed.size(), surfs, joined(failed));
    }
    return true;
}

struct SurfCase{
    std::string_view name;
    struct Input{
        UnitTestResult (*body)();
    };
    Input input;
    struct Expectation{
        std::string_view description;
    };
    Expectation expectation;
};

class Test_AutoStorySurf : public UnitTest{
public:
    explicit Test_AutoStorySurf(SurfCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Surf: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    SurfCase m_case;
};

const SurfCase SURF_CASES[] = {
    {
        .name = "Swimmer Spencer walks (8..16, 42..46) on Route21_North",
        .input = {.body = test_spencer_walks},
        .expectation = {.description = "his walk and line are in the trainer data the planner reads"},
    },
    {
        .name = "12.03's legs on the water land or end at a wall",
        .input = {.body = test_no_open_water_legs},
        .expectation = {.description = "no leg on Route 21's water ends in the open"},
    },
    {
        .name = "12.03's routes go straight",
        .input = {.body = test_no_back_tracking},
        .expectation = {.description = "no route step walks back, but a tile off a wall"},
    },
    {
        .name = "12.03 with a message after each step on the water",
        .input = {.body = test_message_at_every_surf_step},
        .expectation = {.description = "every run ends below the Mansion's door"},
    },
    {
        .name = "12.03 with a hold stopped short, unseen",
        .input = {.body = test_short_hold_never_goes_on_from_a_guess},
        .expectation = {.description = "each run ends below the Mansion's door or fails, never elsewhere"},
    },
    {
        .name = "12.03 with a box open as each Surf step starts",
        .input = {.body = test_box_open_before_each_surf},
        .expectation = {.description = "each clears the box with B and surfs onto its declared tile"},
    },
};

} // namespace


void add_tests_AutoStorySurf(UnitTestDatabase& database){
    for (const SurfCase& item : SURF_CASES){
        database.add<Test_AutoStorySurf>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
