/*  AutoStory Viridian Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Viridian Gym's spinner tiles and rows 17.02 to 17.04b
 *  (StoryData/frlg-17.0N.plan.json) on the overworld sim.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSpin.h"
#include "PokemonFRLG_AutoStoryMovedTrainers.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows17_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryViridianInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryViridianExpectation{
    std::string_view description;
};

struct AutoStoryViridianCase{
    AutoStoryViridianInput input;
    AutoStoryViridianExpectation expectation;
};

class Test_AutoStoryViridian : public UnitTest{
public:
    explicit Test_AutoStoryViridian(AutoStoryViridianCase test_case)
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
    AutoStoryViridianCase m_case;
};

//  Viridian Gym's arrow tiles (the map file's glyphs): stepped onto, the
//  player slides until a stop tile or a wall, turned by each arrow met.
UnitTestResult check_gym_slide(const TilePos& from, const TilePos& end, Direction facing, int tiles){
    const MapData* gym = nullptr;
    for (const MapData& map : story_maps()){
        if (map.name == "ViridianCity_Gym"){
            gym = &map;
        }
    }
    if (gym == nullptr){
        return "No ViridianCity_Gym map.";
    }
    const std::optional<SpinSlide> slide = spin_slide(*gym, PlayerState{from, ELEVATION_GROUND, false});
    if (!slide){
        return std::format("No slide from {}.", to_string(from));
    }
    if (!(slide->end.position == end) || slide->facing != facing || slide->tiles != tiles){
        return std::format(
            "From {}: slid {} tile(s) to {} facing {}; expected {} to {} facing {}.",
            to_string(from), slide->tiles, to_string(slide->end.position), direction_name(slide->facing),
            tiles, to_string(end), direction_name(facing)
        );
    }
    return true;
}

UnitTestResult test_gym_east_arrow(){
    return check_gym_slide({11, 3}, {17, 3}, Direction::EAST, 6);
}
UnitTestResult test_gym_south_arrow_chain(){
    return check_gym_slide({18, 3}, {18, 14}, Direction::SOUTH, 11);
}
UnitTestResult test_gym_north_arrow(){
    return check_gym_slide({0, 20}, {0, 11}, Direction::NORTH, 9);
}

constexpr std::string_view GYM = "ViridianCity_Gym";

const StoryCheckpoint& table_row(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No row {}.", id));
}

//  Whether row `row`'s step `c` is a battle a talk starts: the sim has no
//  talk, and no trainer's message up for it.
bool talk_battle(const StoryCheckpoint& row, size_t c){
    return row.steps[c].kind == StoryStepKind::BATTLE && c > 0 && row.steps[c - 1].kind == StoryStepKind::TALK;
}

//  Whether the overworld sim plays row `row`'s step `c`; after any other
//  step the player is put on its pose_after.
bool sim_walks(const StoryCheckpoint& row, size_t c){
    const StoryStep& step = row.steps[c];
    return !talk_battle(row, c) && (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS
        || step.kind == StoryStepKind::STEP || step.kind == StoryStepKind::BATTLE);
}

//  story_maps() after Giovanni's win: every trainer of the gym beaten with
//  him (set_gym_trainers 8).
std::vector<MapData> beaten_gym_maps(){
    std::vector<MapData> maps = story_maps();
    for (MapData& map : maps){
        if (map.name != GYM){
            continue;
        }
        for (MapObject& object : map.objects){
            object.trainer.reset();
        }
    }
    return maps;
}

//  Row `id`'s steps after `after` (from the row's start if empty) up to
//  `last`, on the sim over `maps` from where `after` leaves the player. It
//  must fight only its own battles, break no rule and end on `end`. The run
//  flags go from step to step, as in the program's run state; a battle a
//  talk starts beats the trainers its flags_after name, on the sim too.
UnitTestResult walk_row(
    std::string_view id, std::string_view after, std::string_view last,
    const std::vector<MapData>& maps, const MapPosition& end
){
    const StoryCheckpoint& row = table_row(id);
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    size_t first = 0;
    for (size_t c = 0; !after.empty() && c < row.steps.size(); c++){
        if (row.steps[c].name == after){
            at = *story_pose(Pokemon::pose_of(*row.steps[c].pose_after));
            first = c + 1;
        }
    }
    std::set<std::string> flags(row.start_flags.begin(), row.start_flags.end());
    std::optional<OverworldSim> sim;
    sim.emplace(with_trainers_beaten(maps, beaten_trainers(flags)), MapPosition{at.map, at.tile}, at.facing.value_or(Direction::SOUTH));
    size_t battles = 0;
    //  The trainers fought on the sims before `sim`, and the talks' battles.
    size_t fought = 0;
    for (size_t c = first; c < row.steps.size(); c++){
        const StoryStep& step = row.steps[c];
        battles += step.kind == StoryStepKind::BATTLE ? 1 : 0;
        if (sim_walks(row, c)){
            if (const std::string failure = row_step_carrying_flags(*sim, at, step, row.id, flags); !failure.empty()){
                return failure;
            }
        }else if (step.pose_after){
            fought += sim->state().trainers_beaten.size() + (talk_battle(row, c) ? 1 : 0);
            flags.insert(step.flags_after.begin(), step.flags_after.end());
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            sim.emplace(with_trainers_beaten(maps, beaten_trainers(flags)), MapPosition{at.map, at.tile}, at.facing.value_or(sim->state().facing));
        }
        if (!sim->state().violations.empty()){
            return std::format("{} {}: broke a rule: {}", id, step.name, sim->state().violations.front());
        }
        if (step.name == last){
            break;
        }
    }
    if (!(sim->state().position == end)){
        return std::format("{}: ended on {}, not {}.", id, to_string(sim->state().position), to_string(end));
    }
    fought += sim->state().trainers_beaten.size();
    if (fought != battles){
        return std::format("{}: fought {} trainer(s), not the row's {}.", id, fought, battles);
    }
    return true;
}

//  17.02 from the Fly to Viridian City: to the gym's door, in, up to
//  below Warren's line and into it; his battle leaves the player on (15, 7).
UnitTestResult test_row_17_02(){
    return walk_row("17.02", "fly_viridian", "warren", story_maps(), MapPosition{std::string(GYM), {15, 7}});
}

//  Takashi, facing south on (10, 2) with a sight of 3, sees the player
//  step onto (10, 5) and walks up to (10, 4), where he stays after the
//  battle (console B, 2026-11-15 16:52); once beaten he sees no one.
UnitTestResult test_takashi_walks_up(){
    const std::optional<TrainerMove> takashi = trainer_walking_up(story_maps(), MapPosition{std::string(GYM), {10, 5}}, {});
    if (!takashi || takashi->trainer != "ViridianCity_Gym/black_belt@10,2" || takashi->x != 10 || takashi->y != 4){
        return "Expected Takashi (ViridianCity_Gym/black_belt@10,2) to walk up to (10, 4).";
    }
    if (trainer_walking_up(story_maps(), MapPosition{std::string(GYM), {10, 5}}, {takashi->trainer})){
        return "Expected no trainer to walk up once Takashi is beaten.";
    }
    return true;
}

//  17.03: Takashi, facing south on (10, 2) with a sight of 3, is fought
//  from his right, (11, 2), where the arrows (19, 14) and (19, 2) slide the
//  player; he stays on (10, 2), and below_giovanni goes up column 10 past
//  him. Fought from his line, (10, 5), he walked up to (10, 4) and stood
//  across that way (console B, 2026-11-15 16:52).
UnitTestResult test_row_17_03(){
    return walk_row("17.03", "", "below_giovanni", story_maps(), MapPosition{std::string(GYM), {2, 3}});
}

const std::string WARREN = "ViridianCity_Gym/cooltrainer_m@13,7";
const std::string TAKASHI = "ViridianCity_Gym/black_belt@10,2";

//  The directions of the legs the walker's [Leg] records say it walked, in
//  order; turns in place (0 tiles) left out.
std::vector<std::string> walked_directions(const std::vector<std::string>& log){
    std::vector<std::string> directions;
    for (const std::string& line : log){
        const size_t dir = line.find("\"dir\":\"");
        if (line.find("[Leg]") == std::string::npos || dir == std::string::npos || line.find("\"tiles\":0") != std::string::npos){
            continue;
        }
        const size_t from = dir + 7;
        directions.push_back(line.substr(from, line.find('"', from) - from));
    }
    return directions;
}

bool opposite(std::string_view a, std::string_view b){
    return (a == "north" && b == "south") || (a == "south" && b == "north")
        || (a == "east" && b == "west") || (a == "west" && b == "east");
}

//  17.03's route `event` planned from `from` over the gym with `beaten`
//  beaten, as the program plans it, and walked on the sim with nothing
//  injected. A clean walk needs no position check: it never plans again
//  from an unknown tile, and no leg walks back the way the one before went
//  (console B, 2026-11-15 21:39, below_giovanni: "west 3 (open)" to (7, 3)
//  and "north 3 (open)" up to Giovanni each arrived, and each was followed
//  by holds to walls and back).
UnitTestResult walk_event_clean(std::string_view event, const StoryPose& from, const std::set<std::string>& beaten){
    const StoryCheckpoint& row = table_row("17.03");
    const StoryStep* step = nullptr;
    for (const StoryStep& candidate : row.steps){
        if (candidate.name == event){
            step = &candidate;
        }
    }
    if (step == nullptr){
        return std::format("17.03 has no step {}.", event);
    }
    const std::vector<MapData> maps = with_trainers_beaten(story_maps(), beaten);
    const std::optional<std::vector<RouteStep>> route = walked_route(from, *step, maps);
    if (!route || route->empty()){
        return std::format("17.03 {}: no route from {}.", event, to_string(from.tile));
    }
    OverworldSim sim(maps, MapPosition{from.map, from.tile}, from.facing.value_or(Direction::SOUTH));
    OverworldSimWalker walker(sim);
    const RoutePlanning planning{
        .maps = &maps, .costs = story_route_costs(), .start = MapPosition{from.map, from.tile}, .label = std::string(event),
        .beaten_trainers = beaten
    };
    const std::string failure = walker.run(*route, planning);
    std::string log;
    for (const std::string& line : walker.log()){
        log += "\n  " + line;
    }
    if (!failure.empty() || !(sim.state().position == route->back().end)){
        return std::format("17.03 {}: ended on {}, not {}: \"{}\"{}", event, to_string(sim.state().position), to_string(route->back().end), failure, log);
    }
    if (log.find("plan from an unknown tile") != std::string::npos){
        return std::format("17.03 {}: a clean walk planned again from an unknown tile:{}", event, log);
    }
    const std::vector<std::string> directions = walked_directions(walker.log());
    for (size_t c = 1; c < directions.size(); c++){
        if (opposite(directions[c - 1], directions[c])){
            return std::format("17.03 {}: leg {} ({}) walks back the way leg {} ({}) went:{}", event, c + 1, directions[c], c, directions[c - 1], log);
        }
    }
    return true;
}

//  right_of_takashi from Warren's battle tile, Warren beaten (17.02).
UnitTestResult test_17_03_right_of_takashi_clean(){
    return walk_event_clean("right_of_takashi", StoryPose{std::string(GYM), {15, 7}, Direction::NORTH}, {WARREN});
}

//  below_giovanni from beside Takashi, him beaten too.
UnitTestResult test_17_03_below_giovanni_clean(){
    return walk_event_clean("below_giovanni", StoryPose{std::string(GYM), {11, 2}, Direction::WEST}, {WARREN, TAKASHI});
}

//  17.04b after Giovanni's win: back through the gym past its beaten
//  trainers (Takashi's line on row 3 among them), out of the door and the
//  Fly to below the Center's door (26, 27).
UnitTestResult test_row_17_04b(){
    return walk_row("17.04b", "", "below_center_door", beaten_gym_maps(), MapPosition{"ViridianCity", {26, 27}});
}

const AutoStoryViridianCase AUTO_STORY_VIRIDIAN_CASES[] = {
    {
        .input = {.name = "17.02: from the Fly to Viridian through the gym into Warren's line", .body = test_row_17_02},
        .expectation = {.description = "Warren's battle won on (15, 7), no other trainer met"},
    },
    {
        .input = {.name = "Takashi seen from his line on (10, 5)", .body = test_takashi_walks_up},
        .expectation = {.description = "walks up to (10, 4); beaten, sees no one"},
    },
    {
        .input = {.name = "17.03: Takashi fought from his right, then up column 10 to Giovanni", .body = test_row_17_03},
        .expectation = {.description = "on ViridianCity_Gym (2, 3), Takashi's battle the only one"},
    },
    {
        .input = {.name = "17.03 right_of_takashi walked clean", .body = test_17_03_right_of_takashi_clean},
        .expectation = {.description = "arrives on (11, 2) with no position check and no leg walked back"},
    },
    {
        .input = {.name = "17.03 below_giovanni walked clean", .body = test_17_03_below_giovanni_clean},
        .expectation = {.description = "arrives below Giovanni on (2, 3) with no position check and no leg walked back"},
    },
    {
        .input = {.name = "17.04b: out of the beaten gym and by Fly to below Viridian's Center", .body = test_row_17_04b},
        .expectation = {.description = "on ViridianCity (26, 27), no trainer met"},
    },
    {
        .input = {.name = "Viridian Gym: the east arrow at (11, 3) slides to its stop", .body = test_gym_east_arrow},
        .expectation = {.description = "6 tiles east to (17, 3)"},
    },
    {
        .input = {.name = "Viridian Gym: the south arrows at (18, 3) slide to the stop below", .body = test_gym_south_arrow_chain},
        .expectation = {.description = "11 tiles south to (18, 14)"},
    },
    {
        .input = {.name = "Viridian Gym: the north arrow at (0, 20) slides to its stop", .body = test_gym_north_arrow},
        .expectation = {.description = "9 tiles north to (0, 11)"},
    },
};

} // namespace


void add_tests_AutoStoryViridian(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_VIRIDIAN_CASES){
        database.add<Test_AutoStoryViridian>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
