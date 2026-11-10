/*  AutoStory Celadon Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows from 00.26 on (StoryData/frlg-00.26.plan.json and after) on the
 *  overworld sim: 00.26 from where 00.25 ends, to the Fan Club's chairman,
 *  north past Route 6's trainers, through the tree into Cerulean, to the
 *  Bike Shop's clerk, the Mart and the nurse, with Oak's aide on
 *  Vermilion's (25, 7); and what its Mart step buys. 00.27 from where
 *  00.26 ends, east over Route 9 to Route 10's Pokemon Center.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const std::string ROUTE6 = "Route6";
const std::string VERMILION = "VermilionCity";
const std::string BIKE_SHOP = "CeruleanCity_BikeShop";
const std::string CERULEAN_CENTER = "CeruleanCity_PokemonCenter_1F";
const std::string CERULEAN_MART = "CeruleanCity_Mart";
const std::string ROUTE10_CENTER = "Route10_PokemonCenter_1F";
const std::string ROUTE9 = "Route9";

struct AutoStoryCeladonInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryCeladonExpectation{
};

struct AutoStoryCeladonCase{
    AutoStoryCeladonInput input;
    AutoStoryCeladonExpectation expectation;
};

class Test_AutoStoryCeladon : public UnitTest{
public:
    explicit Test_AutoStoryCeladon(AutoStoryCeladonCase test_case)
        : UnitTest("PokemonFRLG::AutoStory - Simulation: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryCeladonCase m_case;
};

size_t row_index(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return row - rows.begin();
}

const StoryCheckpoint& checkpoint(std::string_view id){
    return story_checkpoints()[row_index(id)];
}

MapData& map_named(std::vector<MapData>& maps, const std::string& name){
    for (MapData& map : maps){
        if (map.name == name){
            return map;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No map " + name + ".");
}

//  The maps as 00.25 leaves them: story_maps() with Oak's aide on
//  Vermilion's (25, 7), shown by Lt. Surge's win, and Route 6's Jeff
//  beaten (00.21). Isabelle and Elijah are not.
std::vector<MapData> maps_after_surge(){
    const std::vector<MapData> all = overworld_maps();
    std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
    flags.merge(map_flag_set(all, "new_game"));
    flags.erase("FLAG_HIDE_VERMILION_CITY_OAKS_AIDE");
    std::vector<MapData> maps = overworld_maps(flags);
    for (MapObject& object : map_named(maps, ROUTE6).objects){
        if (object.name == "camper" && object.trainer){
            object.trainer.reset();
        }
    }
    return maps;
}

//  Whoever stands `tiles` ahead of `pose`, or nothing.
const MapObject* object_ahead(const std::vector<MapData>& maps, const StoryPose& pose, int tiles){
    TilePos tile = pose.tile;
    for (int c = 0; c < tiles; c++){
        tile = tile.next(*pose.facing);
    }
    for (const MapData& map : maps){
        if (map.name == pose.map){
            return object_at(map, tile);
        }
    }
    return nullptr;
}

//  The row's last route's target.
std::optional<StoryPose> last_target(const StoryCheckpoint& row){
    for (auto step = row.steps.rbegin(); step != row.steps.rend(); ++step){
        if (step->kind == StoryStepKind::ROUTE && step->args.contains("target")){
            return story_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(step->args, "target")));
        }
    }
    return std::nullopt;
}


//  Row `id` comes right after row `previous` and starts where its last
//  walk ends.
UnitTestResult starts_where_ends(const std::string& previous, const std::string& id){
    if (row_index(id) != row_index(previous) + 1){
        return "Expected " + id + " right after " + previous + ".";
    }
    const std::optional<StoryPose> end = last_target(checkpoint(previous));
    const StoryCheckpoint& row = checkpoint(id);
    const std::optional<StoryPose> start = row.start ? story_pose(Pokemon::pose_of(*row.start)) : std::nullopt;
    if (!end || !start || start->map != end->map || !(start->tile == end->tile) || start->facing != end->facing){
        return "Expected " + id + " to start where " + previous + "'s last walk ends.";
    }
    return true;
}

//  00.26 starts at Vermilion's nurse, where 00.25 ends.
UnitTestResult test_starts_where_lt_surge_ends(){
    return starts_where_ends("00.25", "00.26");
}

//  00.27 starts at Cerulean's nurse, where 00.26 ends.
UnitTestResult test_starts_where_bicycle_ends(){
    return starts_where_ends("00.26", "00.27");
}

//  00.26 on the sim from Vermilion's nurse to Cerulean's, the maps as
//  00.25 leaves them: no trainer meets the player (Isabelle and Elijah
//  stay unfought), no violation, each talk faces the one it's for (the
//  chairman beside the player, the Bike Shop's clerk across the counter),
//  and the walk ends at Cerulean's nurse.
UnitTestResult test_bike_voucher_row(){
    const StoryCheckpoint& row = checkpoint("00.26");
    const std::vector<MapData> maps = maps_after_surge();
    const MapData& city = *std::find_if(maps.begin(), maps.end(), [](const MapData& map){ return map.name == VERMILION; });
    if (object_at(city, {25, 7}) == nullptr){
        return "Expected Oak's aide on (25, 7) after Lt. Surge.";
    }
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(maps, MapPosition{at.map, at.tile}, *at.facing);
    std::vector<std::string> talked_to;
    for (const StoryStep& step : row.steps){
        if (step.kind == StoryStepKind::TALK){
            const StoryPose here{sim.state().position.map, sim.state().position.point, sim.state().facing};
            const bool counter = here.map == BIKE_SHOP;
            const MapObject* person = object_ahead(maps, here, counter ? 2 : 1);
            if (person == nullptr || (counter && object_ahead(maps, here, 1) != nullptr)){
                return std::format("00.26/{}: no one to talk to from {}.", step.name, to_string(sim.state().position));
            }
            talked_to.push_back(person->name);
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
        //  A walk the sim's player didn't finish, stopped by someone the
        //  maps the program plans on don't have.
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return std::format("00.26/{}: expected {}, on {}.", step.name, to_string(MapPosition{at.map, at.tile}), to_string(sim.state().position));
        }
    }
    if (talked_to != std::vector<std::string>{"gentleman", "man"}){
        return "00.26: expected to talk to the chairman, then the Bike Shop's clerk.";
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "00.26: a battle or a violation on the way" + (sim.state().violations.empty() ? std::string(".") : ": " + sim.state().violations.back());
    }
    const MapPosition nurse{CERULEAN_CENTER, {7, 4}};
    if (!(sim.state().position == nurse)){
        return "00.26: ended on " + to_string(sim.state().position) + ", not Cerulean's nurse.";
    }
    return true;
}

//  00.26's Mart step, at its money floor: 5 Repels and 2 Escape Ropes,
//  and 3000 or more left.
UnitTestResult test_repels_and_escape_ropes(){
    const StoryCheckpoint& row = checkpoint("00.26");
    const auto step = std::find_if(row.steps.begin(), row.steps.end(), [](const StoryStep& data){
        return data.action == MART_ACTION;
    });
    if (step == row.steps.end()){
        return "00.26: expected a Mart step.";
    }
    const std::optional<MartPlan> parsed = mart_plan_of(step->args);
    if (!parsed){
        return "00.26: expected a Mart plan.";
    }
    const MartPlan& plan = *parsed;
    if (plan.map != CERULEAN_MART || !plan.money_floor){
        return "00.26: expected the Mart step at Cerulean's Mart, with a money floor.";
    }
    const std::optional<std::vector<MartBuy>> buys = plan_mart_buys(mart_stock(plan.map), plan, *plan.money_floor);
    if (!buys){
        return "00.26: the Mart step names an item Cerulean's Mart doesn't sell.";
    }
    size_t repels = 0;
    size_t escape_ropes = 0;
    size_t spent = 0;
    for (const MartBuy& buy : *buys){
        repels += buy.item == "repel" ? buy.quantity : 0;
        escape_ropes += buy.item == "escape-rope" ? buy.quantity : 0;
        spent += buy.cost;
    }
    if (repels < 5 || escape_ropes != 2 || *plan.money_floor < spent + 3000){
        return std::format("00.26: bought {} Repels and {} Escape Ropes for {} from {}.", repels, escape_ropes, spent, *plan.money_floor);
    }
    return true;
}

//  00.27 on the sim from Cerulean's nurse to Route 10's, the maps as 00.26
//  leaves them, both trees grown back: the trees cut, Alicia, Jeremy and
//  Conner fought where the table steps into their lines and no one else
//  met (Brice, Caitlin and the rest unfought), no violation, each step
//  ending where the table says, and the walk ending at Route 10's nurse.
UnitTestResult test_route10_row(){
    const StoryCheckpoint& row = checkpoint("00.27");
    const std::vector<MapData> maps = maps_after_surge();
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(maps, MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row.steps){
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
        //  A step into a trainer's line ends where the battle after it says.
        const bool into_line = step.kind == StoryStepKind::STEP && !step.action.empty();
        if (!into_line && !(sim.state().position == MapPosition{at.map, at.tile})){
            return std::format("00.27/{}: expected {}, on {}.", step.name, to_string(MapPosition{at.map, at.tile}), to_string(sim.state().position));
        }
    }
    if (sim.state().trainers_beaten != std::vector<std::string>{"picnicker", "hiker", "bug_catcher"}){
        return "00.27: expected to fight Alicia, Jeremy and Conner, no one else; fought " + std::to_string(sim.state().trainers_beaten.size()) + ".";
    }
    if (!sim.state().violations.empty()){
        return "00.27: a violation on the way: " + sim.state().violations.back();
    }
    const MapPosition nurse{ROUTE10_CENTER, {7, 4}};
    if (!(sim.state().position == nurse)){
        return "00.27: ended on " + to_string(sim.state().position) + ", not Route 10's nurse.";
    }
    return true;
}

//  The next row starts from 00.27's save in Route 10's Center, whose save
//  card says ROUTE 10: a place of its own, not Route 1's.
UnitTestResult test_route10_save_place(){
    if (save_card_place(ROUTE10_CENTER) != "route-10" || save_card_place("Route1") == "route-10"){
        return "Wrong save card place for Route 10's Pokemon Center.";
    }
    return true;
}

//  00.27's walk from Route 9 (70, 8) to Route 10's nurse: its steps from
//  route10_ledge_end to route10_nurse. Console B, 2026-11-09, the walk
//  then planned in one ended its open legs a tile from a blocker, and both
//  runs ended "no fade in and out" at the Center's door (13, 20):
//  - run 3: the crossing onto Route 10 wasn't held up, so the player
//    stood a tile further east, row 11 ended on (17, 11), the leg down
//    column 17 met the fence post (17, 12), and the walk ended in the
//    grass on (14, 10).
//  - run 2: the leg down column 16 ended a tile short, on (16, 20), west
//    3 met the Center's east wall (15, 20) and north 1 ended on (16, 19).
std::vector<StoryStep> route10_walk(){
    const StoryCheckpoint& row = checkpoint("00.27");
    const auto named = [&row](std::string_view name){
        return std::find_if(row.steps.begin(), row.steps.end(), [&](const StoryStep& data){ return data.name == name; });
    };
    const auto first = named("route10_ledge_end");
    const auto last = named("route10_nurse");
    if (first == row.steps.end() || last == row.steps.end() || last < first){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "00.27: no steps route10_ledge_end to route10_nurse.");
    }
    return std::vector<StoryStep>(first, last + 1);
}

const StoryPose ROUTE10_WALK_START{ROUTE9, {70, 8}, Direction::EAST};

//  `steps` from `first` on, on `sim` from `at`, where the table says the
//  player stands: the failure, or empty once at Route 10's nurse.
std::string route10_walk_rest(OverworldSim& sim, StoryPose at, const std::vector<StoryStep>& steps, size_t first){
    std::string failure;
    for (size_t c = first; c < steps.size() && failure.empty(); c++){
        failure = row_step_on_sim(sim, at, steps[c], "00.27");
    }
    const MapPosition nurse{ROUTE10_CENTER, {7, 4}};
    if (!failure.empty() || !(sim.state().position == nurse)){
        return std::format("ended on {}, not Route 10's nurse; {}", to_string(sim.state().position), failure);
    }
    return "";
}

std::string route10_walk_on_sim(const SimScript& script){
    OverworldSim sim(maps_after_surge(), MapPosition{ROUTE10_WALK_START.map, ROUTE10_WALK_START.tile}, Direction::EAST, script);
    return route10_walk_rest(sim, ROUTE10_WALK_START, route10_walk(), 0);
}

UnitTestResult test_route10_crossing_not_held_up(){
    SimScript script;
    script.connections_stall = false;
    if (const std::string failure = route10_walk_on_sim(script); !failure.empty()){
        return "The crossing onto Route 10 not held up (run 3): " + failure;
    }
    return true;
}

UnitTestResult test_route10_column_16_short(){
    SimScript script;
    //  Holds: east 7, south 3 and east 12 onto row 11; west 1; south 12
    //  down column 16.
    script.short_holds[5] = 1;
    if (const std::string failure = route10_walk_on_sim(script); !failure.empty()){
        return "The leg down column 16 a tile short (run 2): " + failure;
    }
    return true;
}

//  Where an open leg of two tiles or more ends a tile short or long, the
//  next leg starts a tile off. From each such start (on an open tile), the
//  rest of the walk still reaches the nurse. A leg to a wall ends on its
//  tile, and a one-tile leg is one step (as grass_held() takes it), so the
//  leg after either starts where planned. The one exception is the door's
//  column: the three east from the wall (9, 21) set it, and nothing after
//  them can.
UnitTestResult test_route10_tile_off_after_open_legs(){
    const std::vector<StoryStep> steps = route10_walk();
    const std::vector<MapData>& maps = story_maps();
    //  above_caitlin_line ends east 6 on (70, 8), an open leg.
    RouteStep before{.direction = Direction::EAST, .tiles = 6};
    StoryPose at = ROUTE10_WALK_START;
    for (size_t s = 0; s < steps.size(); s++){
        const std::vector<RouteStep> route = walked_route(at, steps[s]).value_or(std::vector<RouteStep>{});
        MapPosition from{at.map, at.tile};
        for (size_t c = 0; c < route.size(); c++){
            const bool door_column = steps[s].name == "route10_nurse" && c == 0;
            const bool off = before.action == RouteAction::WALK && !before.to_wall && !before.warps && before.tiles > 1 && !door_column;
            const TilePos on = from.point.next(before.direction);
            const TilePos short_of{2 * from.point.x - on.x, 2 * from.point.y - on.y};
            const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapData& data){ return data.name == from.map; });
            for (const TilePos& start : off ? std::vector<TilePos>{short_of, on} : std::vector<TilePos>{}){
                const bool open = map != maps.end() && in_bounds(*map, start) && object_at(*map, start) == nullptr
                    && (tile_at(*map, start) == TileKind::OPEN || tile_at(*map, start) == TileKind::TALL_GRASS);
                if (!open){
                    continue;
                }
                OverworldSim sim(maps_after_surge(), MapPosition{from.map, start}, before.direction);
                const std::vector<RouteStep> rest(route.begin() + c, route.end());
                const RoutePlanning planning{.maps = &maps, .costs = story_route_costs(), .start = from, .label = steps[s].name};
                std::string failure = OverworldSimWalker(sim).run(rest, planning);
                const StoryPose next{route.back().end.map, route.back().end.point, route.back().facing_after};
                failure = failure.empty() ? route10_walk_rest(sim, next, steps, s + 1) : failure;
                if (!failure.empty()){
                    return std::format("{} leg {} ({}) from {}, not {}: {}", steps[s].name, c + 1, to_string(route[c]), to_string(start), to_string(from.point), failure);
                }
            }
            before = route[c];
            from = route[c].end;
        }
        at = route.empty() ? at : StoryPose{route.back().end.map, route.back().end.point, route.back().facing_after};
    }
    return true;
}

} // namespace


const AutoStoryCeladonCase AUTO_STORY_CELADON_CASES[] = {
    {
        .input = {"00.26 starts where 00.25 ends", test_starts_where_lt_surge_ends},
        .expectation = {},
    },
    {
        .input = {"00.26 the Fan Club, Route 6 and the Underground Path north, the Bike Shop, past Oak's aide", test_bike_voucher_row},
        .expectation = {},
    },
    {
        .input = {"00.26 Repels and Escape Ropes, 3000 left", test_repels_and_escape_ropes},
        .expectation = {},
    },
    {
        .input = {"00.27 starts where 00.26 ends", test_starts_where_bicycle_ends},
        .expectation = {},
    },
    {
        .input = {"00.27 the two trees, Route 9's Alicia, Jeremy and Conner, to Route 10's Pokemon Center", test_route10_row},
        .expectation = {},
    },
    {
        .input = {"00.27 saves in Route 10's Pokemon Center, ROUTE 10 on the save card", test_route10_save_place},
        .expectation = {},
    },
    {
        .input = {"00.27 route10_nurse: the crossing onto Route 10 not held up", test_route10_crossing_not_held_up},
        .expectation = {},
    },
    {
        .input = {"00.27 route10_nurse: the leg down column 16 a tile short", test_route10_column_16_short},
        .expectation = {},
    },
    {
        .input = {"00.27 route10_nurse: a tile off after each open leg", test_route10_tile_off_after_open_legs},
        .expectation = {},
    },
};

void add_tests_AutoStoryCeladon(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_CELADON_CASES){
        database.add<Test_AutoStoryCeladon>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
