/*  AutoStory Cinnabar Gym Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 14.01, 14.02 and 14.02b (StoryData/frlg-14.01.plan.json, 14.02, 14.02b) on the
 *  overworld sim: from outside the Pokemon Mansion to Cinnabar's nurse,
 *  into the gym and through its six quiz doors to below Blaine; and from
 *  Blaine back out through the doors, past Bill, to the nurse. And the
 *  lead's attacks against Blaine's party.
 *
 */

#include <format>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_MapUtil.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows14_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const std::string GYM = "CinnabarIsland_Gym";

struct AutoStoryCinnabarGymInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryCinnabarGymExpectation{
    std::string_view description;
};

struct AutoStoryCinnabarGymCase{
    AutoStoryCinnabarGymInput input;
    AutoStoryCinnabarGymExpectation expectation;
};

class Test_AutoStoryCinnabarGym : public UnitTest{
public:
    explicit Test_AutoStoryCinnabarGym(AutoStoryCinnabarGymCase test_case)
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
    AutoStoryCinnabarGymCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  The tiles each quiz door's script turns to floor, door N's at [N - 1]
//  (pokefirered CinnabarIsland_Gym scripts.inc, OpenDoorN's setmetatile
//  calls without the collision bit). The map has the doors shut.
const std::vector<TilePos> QUIZ_DOOR_FLOORS[] = {
    {{26, 8}, {27, 8}, {26, 9}, {27, 9}, {26, 10}, {27, 10}, {28, 10}},
    {{17, 8}, {18, 8}, {17, 9}, {18, 9}, {17, 10}, {18, 10}, {19, 10}},
    {{17, 15}, {18, 15}, {17, 16}, {18, 16}, {17, 17}, {18, 17}, {19, 17}},
    {{11, 22}, {11, 23}},
    {{5, 16}, {6, 16}, {5, 17}, {6, 17}, {5, 18}, {6, 18}, {7, 18}},
    {{5, 8}, {6, 8}, {5, 9}, {6, 9}, {5, 10}, {6, 10}, {7, 10}},
};

//  The rows on the sim, whose gym opens each quiz door as the console does
//  once its quiz is answered, while the walker plans on the story's maps,
//  where the doors are shut: a leg held to a shut door runs through the
//  open one, as on the console. The sim can't walk into a Center with no
//  map. Kept across sims: the trainers fought.
struct GymWalk{
    StoryPose at;
    std::vector<MapData> maps = story_maps();
    std::optional<OverworldSim> sim;
    std::vector<std::string> fought;

    explicit GymWalk(StoryPose start)
        : at(std::move(start))
    {
        place();
    }
    //  A fresh sim on `at`, after a step it can't walk.
    void place(){
        place_on(MapPosition{at.map, at.tile}, *at.facing);
    }
    //  Quiz door `door` (1 to 6) opens where the player stands.
    void open_door(size_t door){
        for (MapData& map : maps){
            if (map.name != GYM){
                continue;
            }
            for (const TilePos& tile : QUIZ_DOOR_FLOORS[door - 1]){
                Pokemon::cell_at(tile, map.tiles) = TileKind::OPEN;
            }
        }
        const SimState& state = sim->state();
        place_on(state.position, state.facing);
    }
    void place_on(const MapPosition& position, Direction facing){
        if (sim){
            const std::vector<std::string>& beaten = sim->state().trainers_beaten;
            fought.insert(fought.end(), beaten.begin(), beaten.end());
        }
        sim.emplace(maps, position, facing);
    }
    std::vector<std::string> all_fought() const{
        std::vector<std::string> all = fought;
        const std::vector<std::string>& beaten = sim->state().trainers_beaten;
        all.insert(all.end(), beaten.begin(), beaten.end());
        return all;
    }
};

//  A step of a row on the sim: row_step_on_sim(), but the way into the
//  Center and Blaine's battle, which a talk starts, leave the player where
//  the table says, and "door_N_opens" opens quiz door N.
std::string gym_step_on_sim(const StoryStep& step, const std::string& id, GymWalk& walk){
    const bool center = step.action == CENTER_ENTER_VARIANT;
    if (center || (step.kind == StoryStepKind::BATTLE && step.name == "blaine")){
        walk.at = *story_pose(Pokemon::pose_of(*step.pose_after));
        if (center){
            walk.place();
        }
        return "";
    }
    const std::string failure = row_step_on_sim(*walk.sim, walk.at, step, id);
    constexpr std::string_view OPENS_PREFIX = "door_";
    constexpr std::string_view OPENS_SUFFIX = "_opens";
    if (failure.empty() && step.name.starts_with(OPENS_PREFIX) && step.name.ends_with(OPENS_SUFFIX)){
        walk.open_door((size_t)(step.name[OPENS_PREFIX.size()] - '0'));
    }
    return failure;
}

//  Row `id`'s steps on the sim from `walk.at`: the failure, or empty.
std::string row_on_sim(std::string_view id, GymWalk& walk){
    const StoryCheckpoint& row = checkpoint(id);
    for (const StoryStep& step : row.steps){
        if (const std::string failure = gym_step_on_sim(step, row.id, walk); !failure.empty()){
            return std::format("{}: ended on {}; {}", id, to_string(walk.sim->state().position), failure);
        }
        if (!walk.sim->state().violations.empty()){
            return std::format("{}/{}: a violation on the way: {}", id, step.name, walk.sim->state().violations.back());
        }
    }
    return "";
}

//  Whether `tile` of the gym is a wall on the map.
bool gym_wall(const TilePos& tile){
    for (const MapData& map : story_maps()){
        if (map.name == GYM){
            return map.tiles.at(tile) == TileKind::WALL;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No map {}.", GYM));
}

Direction behind(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return Direction::SOUTH;
    case Direction::SOUTH:
        return Direction::NORTH;
    case Direction::EAST:
        return Direction::WEST;
    default:
        return Direction::EAST;
    }
}

//  Each quiz door is shut on the map, one barrier tile, with floor on both
//  sides: the tiles 14.01 steps over.
UnitTestResult test_quiz_doors_on_map(){
    const StoryCheckpoint& row = checkpoint("14.01");
    int doors = 0;
    for (const StoryStep& step : row.steps){
        if (!step.name.starts_with("into_door_")){
            continue;
        }
        const StoryPose door = *story_pose(Pokemon::pose_of(*step.pose_after));
        const TilePos before = door.tile.next(behind(*door.facing));
        const TilePos after = door.tile.next(*door.facing);
        if (!gym_wall(door.tile) || gym_wall(before) || gym_wall(after)){
            return std::format("{}: expected a barrier on {} between floor tiles.", step.name, to_string(MapPosition{GYM, door.tile}));
        }
        doors++;
    }
    return doors == 6 ? UnitTestResult(true) : UnitTestResult(std::format("Expected 6 quiz doors, found {}.", doors));
}

//  Whether a person of the gym stands on `tile`.
bool gym_person_on(const TilePos& tile){
    for (const MapData& map : story_maps()){
        if (map.name != GYM){
            continue;
        }
        for (const Pokemon::MapObject& object : map.objects){
            if (object.x == tile.x && object.y == tile.y){
                return true;
            }
        }
    }
    return false;
}

//  A LEGS or ROUTE step of a row inside the gym, as the walker walks it:
//  its route from where the step before it leaves the player.
struct GymLegs{
    const StoryStep* step = nullptr;
    std::vector<RouteStep> route;
};

//  Row `id`'s ROUTE steps that start inside the gym, each planned from the
//  pose the step before it declares or its target. A step with no route
//  has an empty one.
std::vector<GymLegs> gym_routes(std::string_view id){
    const StoryCheckpoint& row = checkpoint(id);
    std::vector<GymLegs> all;
    std::optional<StoryPose> at = story_pose(Pokemon::pose_of(*row.start));
    for (const StoryStep& step : row.steps){
        const Pokemon::GridPose* target = Pokemon::value_if_at_it<Pokemon::GridPose>(step.args, step.args.find("target"));
        const std::optional<StoryPose> to = target == nullptr ? std::nullopt : story_pose(Pokemon::pose_of(*target));
        if (step.kind == StoryStepKind::ROUTE && at && to && at->map == GYM){
            const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
                story_maps(), MapPosition{at->map, at->tile}, RouteGoal{MapPosition{to->map, to->tile}, to->facing},
                story_route_costs(), at->facing.value_or(Direction::NORTH)
            );
            all.push_back(GymLegs{.step = &step, .route = route.value_or(std::vector<RouteStep>{})});
        }
        if (step.pose_after){
            at = story_pose(Pokemon::pose_of(*step.pose_after));
        }else if (to){
            at = to;
        }else{
            at.reset();
        }
    }
    return all;
}

//  The planned route of ROUTE step `planned.step`: for each open hold
//  (no wall ends it) that a turn follows, the person on the next leg's
//  tiles if the hold ends a tile short. Empty if there is none. Console B,
//  2026-11-14, 14.01 below_door_5: "east 3 (open)" ended a tile short, on
//  Burglar Dusty's column, and "north 4" stopped below him.
std::string short_hold_meets_person(const GymLegs& planned){
    const StoryStep& step = *planned.step;
    const std::vector<RouteStep>& route = planned.route;
    if (route.empty()){
        return std::format("{}: no route.", step.name);
    }
    for (size_t i = 0; i + 1 < route.size(); i++){
        const RouteStep& hold = route[i];
        const RouteStep& next = route[i + 1];
        if (hold.action != RouteAction::WALK || next.action != RouteAction::WALK || hold.to_wall || hold.tapped || hold.direction == next.direction){
            continue;
        }
        TilePos tile = hold.end.point.next(behind(hold.direction));
        for (int n = 0; n < next.tiles; n++){
            tile = tile.next(next.direction);
            if (gym_person_on(tile)){
                return std::format(
                    "{}: \"{}\" a tile short leaves {} in the way of \"{}\".",
                    step.name, to_string(hold), to_string(tile), to_string(next)
                );
            }
        }
    }
    return "";
}

//  No ROUTE step inside the gym in 14.01 and 14.02b plans an open hold
//  that, a tile short, puts a trainer in the way of the leg after it: the
//  trainers stand beside the door columns, so those steps go a tap a tile.
UnitTestResult test_short_holds_clear_of_trainers(){
    for (std::string_view id : {"14.01", "14.02b"}){
        for (const GymLegs& planned : gym_routes(id)){
            if (const std::string failure = short_hold_meets_person(planned); !failure.empty()){
                return std::format("{} {}", id, failure);
            }
        }
    }
    return true;
}

//  Row `id`'s LEGS steps inside the gym, each from the pose the step before
//  it declares, a route's target or the end of the legs before it. A step
//  whose legs leave the map or meet a wall has an empty route.
std::vector<GymLegs> gym_legs(std::string_view id){
    const StoryCheckpoint& row = checkpoint(id);
    std::vector<GymLegs> all;
    std::optional<StoryPose> at = story_pose(Pokemon::pose_of(*row.start));
    for (const StoryStep& step : row.steps){
        const Pokemon::GridPose* target = Pokemon::value_if_at_it<Pokemon::GridPose>(step.args, step.args.find("target"));
        const std::vector<Pokemon::StoryLeg>* legs = Pokemon::value_if_at_it<std::vector<Pokemon::StoryLeg>>(step.args, step.args.find("legs"));
        std::optional<StoryPose> after = step.pose_after ? story_pose(Pokemon::pose_of(*step.pose_after)) : std::nullopt;
        if (!after && target != nullptr){
            after = story_pose(Pokemon::pose_of(*target));
        }
        if (step.kind == StoryStepKind::LEGS && legs != nullptr && at && at->map == GYM){
            const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), MapPosition{at->map, at->tile}, *legs);
            all.push_back(GymLegs{.step = &step, .route = route.value_or(std::vector<RouteStep>{})});
            if (!after && route && !route->empty()){
                after = StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after};
            }
        }
        if (after || step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS){
            at = after;
        }
    }
    return all;
}

//  The quiz door, 1 to 6, whose floor `tile` is, if any.
std::optional<size_t> quiz_door_on(const TilePos& tile){
    for (size_t door = 0; door < std::size(QUIZ_DOOR_FLOORS); door++){
        for (const TilePos& floor : QUIZ_DOOR_FLOORS[door]){
            if (floor == tile){
                return door + 1;
            }
        }
    }
    return std::nullopt;
}

//  The leg of `legs` (row `id`) that ends at a wall that is a quiz door,
//  as a line, or empty if none does.
std::string wall_tap_into_quiz_door(std::string_view id, const GymLegs& legs){
    if (legs.route.empty()){
        return std::format("{} {}: its legs don't fit the map.", id, legs.step->name);
    }
    for (const RouteStep& leg : legs.route){
        const TilePos beyond = leg.end.point.next(leg.direction);
        const std::optional<size_t> door = leg.to_wall ? quiz_door_on(beyond) : std::nullopt;
        if (door){
            return std::format(
                "{} {}: \"{}\" ends against quiz door {} on {}; the tap after it walks through the open door.",
                id, legs.step->name, to_string(leg), *door, to_string(MapPosition{GYM, beyond})
            );
        }
    }
    return "";
}
//  No leg of 14.01's or 14.02b's gym LEGS steps ends at a wall that is a
//  quiz door shut on the map: after a leg to a wall the walker taps once
//  more toward it, and once the door is open that tap walks through it.
//  Console B, 2026-11-14, 14.01 run 5: east_of_door_4's "west 2 (to wall)"
//  to (12, 22) tapped on through door 4, the player stood a tile west of
//  the plan from there on, and below_door_5 ended under Burglar Dusty.
UnitTestResult test_wall_taps_clear_of_quiz_doors(){
    for (std::string_view id : {"14.01", "14.02b"}){
        for (const GymLegs& legs : gym_legs(id)){
            if (const std::string failure = wall_tap_into_quiz_door(id, legs); !failure.empty()){
                return failure;
            }
        }
    }
    return true;
}

//  Each quiz machine's talk in 14.01 follows a LEGS step counted a tap a
//  tile whose last leg ends against the machine: no open hold that can run
//  a tile on ends the approach to the machine's stand tile.
UnitTestResult test_quiz_approaches_end_at_machine(){
    const StoryCheckpoint& row = checkpoint("14.01");
    const std::vector<GymLegs> all = gym_legs("14.01");
    int quizzes = 0;
    for (size_t c = 1; c < row.steps.size(); c++){
        const StoryStep& talk = row.steps[c];
        if (talk.kind != StoryStepKind::TALK || !talk.name.starts_with("quiz_")){
            continue;
        }
        quizzes++;
        const StoryStep& before = row.steps[c - 1];
        const GymLegs* legs = nullptr;
        for (const GymLegs& item : all){
            if (item.step == &before){
                legs = &item;
            }
        }
        if (legs == nullptr || before.action != TAPS_VARIANT || legs->route.empty() || !legs->route.back().to_wall){
            return std::format("14.01 {}: expected taps ending against the machine before it, found {}.", talk.name, before.name);
        }
    }
    return quizzes == 6 ? UnitTestResult(true) : UnitTestResult(std::format("Expected 6 quiz talks, found {}.", quizzes));
}

//  The last walk of `route` if it is a hold that no wall, warp or tap
//  count ends.
std::optional<RouteStep> open_last_hold(const std::vector<RouteStep>& route){
    for (auto it = route.rbegin(); it != route.rend(); ++it){
        if (it->action != RouteAction::WALK){
            continue;
        }
        if (it->to_wall || it->warps || it->tapped){
            return std::nullopt;
        }
        return *it;
    }
    return std::nullopt;
}

//  No walk inside the gym in 14.01 and 14.02b ends on an open hold. The
//  walker re-checks an open leg that ends a walk with holds to walls, and
//  near a quiz door that wall may be the door, shut on the map and open on
//  the console. Console B, 2026-11-14, 14.01 below_blaine: "north 3 (open)"
//  from door 6 to (5, 5) was re-checked with a hold "south 3 (to wall)" to
//  (5, 8), whose wall is door 6's barrier (5, 9); the hold ran through the
//  open door, north 3 then left the player in the door's gap, and 14.02's
//  talk to Blaine opened no message.
UnitTestResult test_gym_walks_end_clear_of_rechecks(){
    for (std::string_view id : {"14.01", "14.02b"}){
        std::vector<GymLegs> all = gym_legs(id);
        std::vector<GymLegs> routes = gym_routes(id);
        all.insert(all.end(), routes.begin(), routes.end());
        for (const GymLegs& planned : all){
            const bool tapped = planned.step->kind == StoryStepKind::LEGS && planned.step->action == TAPS_VARIANT;
            const std::optional<RouteStep> open = tapped ? std::nullopt : open_last_hold(planned.route);
            if (open){
                return std::format(
                    "{} {}: ends on \"{}\", an open hold the walker re-checks with holds to walls, which may be quiz doors.",
                    id, planned.step->name, to_string(*open)
                );
            }
        }
    }
    return true;
}

//  14.01 from outside the Mansion: the nurse, the gym and its six quiz
//  doors, each walked through once open, nobody fought, to below Blaine, where 14.02 starts; 14.02,
//  Blaine's battle, which ends where it starts; then 14.02b from there out
//  of the gym to the nurse.
UnitTestResult test_gym_rows(){
    GymWalk walk(*story_pose(Pokemon::pose_of(*checkpoint("14.01").start)));
    if (const std::string failure = row_on_sim("14.01", walk); !failure.empty()){
        return failure;
    }
    const MapPosition below_blaine{GYM, {5, 5}};
    if (!(walk.sim->state().position == below_blaine) || walk.sim->state().facing != Direction::NORTH){
        return std::format("14.01: ended on {}, not below Blaine {} facing north.", to_string(walk.sim->state().position), to_string(below_blaine));
    }
    if (!walk.all_fought().empty()){
        return "14.01: fought a gym trainer; the quiz is answered instead.";
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("14.02").start));
    if (next.map != GYM || !(next.tile == below_blaine.point)){
        return "14.02 doesn't start where 14.01 ends.";
    }
    if (const std::string failure = row_on_sim("14.02", walk); !failure.empty()){
        return failure;
    }
    if (!(walk.sim->state().position == below_blaine)){
        return std::format("14.02: ended on {}, not in front of Blaine where 14.02b starts.", to_string(walk.sim->state().position));
    }
    const StoryPose after_blaine = *story_pose(Pokemon::pose_of(*checkpoint("14.02b").start));
    if (after_blaine.map != GYM || !(after_blaine.tile == below_blaine.point)){
        return "14.02b doesn't start where 14.02 ends.";
    }
    if (const std::string failure = row_on_sim("14.02b", walk); !failure.empty()){
        return failure;
    }
    if (!walk.all_fought().empty()){
        return "14.02b: fought a gym trainer on the way out.";
    }
    if (walk.at.map != "CinnabarIsland_PokemonCenter_1F"){
        return std::format("14.02b: ended on {}, not at Cinnabar's nurse.", walk.at.map);
    }
    return true;
}

//  The lead as pokeguy brings it to Blaine: Surf (12.01, over Water Gun),
//  Water Pulse, Bite, Mega Kick. It picks Surf against all four of
//  Blaine's Fire types.
UnitTestResult test_lead_moves_against_blaine(){
    PartyMember lead;
    lead.species = "blastoise";
    lead.moves_read = true;
    lead.level = 40;
    lead.moves = {{"surf", 15}, {"water-pulse", 20}, {"bite", 25}, {"mega-kick", 5}};
    for (std::string_view foe : {"growlithe", "ponyta", "rapidash", "arcanine"}){
        if (attack_slot_against(lead, {std::string(foe)}) != 0){
            return std::format("Against {}: expected Surf.", foe);
        }
    }
    return true;
}

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

//  Battle checkpoint `id`'s precondition: a LEVEL step on the lead before
//  its battle `battle`, at least `pokeguy`, pokeguy's level there (whose
//  sims stand for the fight at that level or above). A lead at that level
//  passes it; one a level below stops the checkpoint before the fight.
UnitTestResult check_lead_level_precondition(Logger& logger, std::string_view id, std::string_view battle, unsigned pokeguy){
    std::optional<LevelCheck> check;
    for (const StoryStep& step : checkpoint(id).steps){
        if (step.kind == StoryStepKind::BATTLE && step.name == battle){
            break;
        }
        if (step.action == LEVEL_ACTION){
            check = level_check_of(step.args);
        }
    }
    if (!check || check->slot != 0 || check->level != pokeguy){
        return std::format("{}: expected a LEVEL step on the lead, at least Lv{}, before {}.", id, pokeguy, battle);
    }
    for (const unsigned level : {pokeguy, pokeguy - 1}){
        const MemberReader lead = [level](size_t slot) -> std::optional<MemberInfo>{
            if (slot != 0){
                return std::nullopt;
            }
            return MemberInfo{.species = "blastoise", .level = level};
        };
        const LevelResult result = run_level_check(logger, lead, *check);
        const LevelEnd expected = level >= pokeguy ? LevelEnd::REACHED : LevelEnd::BELOW;
        if (result.end != expected || result.level != level){
            return std::format("{}: a Lv{} lead {} the Lv{} check.", id, level, level >= pokeguy ? "fails" : "passes", pokeguy);
        }
    }
    return true;
}

const AutoStoryCinnabarGymCase AUTO_STORY_CINNABAR_GYM_CASES[] = {
    {
        .input = {.name = "Cinnabar Gym's quiz doors are shut on the map", .body = test_quiz_doors_on_map},
        .expectation = {.description = "each of 14.01's door steps crosses one barrier tile between floor"},
    },
    {
        .input = {.name = "Cinnabar Gym's open holds a tile short meet no trainer", .body = test_short_holds_clear_of_trainers},
        .expectation = {.description = "no gym route in 14.01 or 14.02b turns onto a trainer after an open hold a tile short"},
    },
    {
        .input = {.name = "Cinnabar Gym's legs to a wall end clear of the quiz doors", .body = test_wall_taps_clear_of_quiz_doors},
        .expectation = {.description = "no gym leg in 14.01 or 14.02b ends against a quiz door, which the tap after it would walk through"},
    },
    {
        .input = {.name = "Cinnabar Gym's quiz approaches end against the machine", .body = test_quiz_approaches_end_at_machine},
        .expectation = {.description = "each of the six quiz talks follows taps whose last leg ends at the machine"},
    },
    {
        .input = {.name = "Cinnabar Gym's walks end clear of the open-leg re-check", .body = test_gym_walks_end_clear_of_rechecks},
        .expectation = {.description = "no gym route or held legs step in 14.01 or 14.02b ends on an open hold"},
    },
    {
        .input = {.name = "14.01 through the quiz to below Blaine, 14.02 on the spot and 14.02b back to the nurse", .body = test_gym_rows},
        .expectation = {.description = "no gym trainer fought; ends at Cinnabar's nurse"},
    },
    {
        .input = {.name = "the lead's attacks against Blaine", .body = test_lead_moves_against_blaine},
        .expectation = {.description = "Surf against each Fire type"},
    },
    {
        .input = {.name = "14.02 stops before Blaine unless the lead is at least pokeguy's Lv40", .body = []{
            QuietLogger logger;
            return check_lead_level_precondition(logger, "14.02", "blaine", 40);
        }},
        .expectation = {.description = "a Lv40 lead passes the LEVEL step, a Lv39 one stops there"},
    },
};

} // namespace


void add_tests_AutoStoryCinnabarGym(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_CINNABAR_GYM_CASES){
        database.add<Test_AutoStoryCinnabarGym>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
