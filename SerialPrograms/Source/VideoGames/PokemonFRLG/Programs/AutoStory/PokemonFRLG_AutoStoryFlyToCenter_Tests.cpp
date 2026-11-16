/*  AutoStory Fly To Center Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A lint over FRLG's story from 10.01, where the Pidgey learns Fly, on:
 *  every trip to a Pokemon Center flies there unless the Center is really
 *  close (user, 2026-11-15: "as soon as we can fly, every to_pc action
 *  should be a fly unless it's really close (cinnabar gym to cinnabar
 *  PC)"), and a trip that starts in a building an Escape Rope works in
 *  leaves it by rope. The routes are planned as the program plans them,
 *  each row from its own start.
 *
 */

#include <format>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The row that teaches Fly; the lint starts there.
constexpr std::string_view FIRST_FLY_ROW = "10.01";

//  A walk to a Center this many tiles or fewer stays a walk. A Fly costs
//  about 10 s of menus and animation, more than 20 tiles of running, but
//  the long walks are the ones that re-planned and failed on console B
//  (2026-11-15: 18.02b's 80 tiles round Saffron's blocks). 20 keeps a walk
//  down one street (Fuchsia's Warden's house to its Center, 8 tiles) and
//  flags every walk across a town (Viridian Gym to its Center, 58 tiles).
constexpr int MAX_WALK_TO_CENTER = 20;

//  The story's maps from 10.01 on that let an Escape Rope work (pokefirered
//  map.json: allow_escaping true).
const std::set<std::string, std::less<>>& escape_rope_maps(){
    static const std::set<std::string, std::less<>> MAPS{
        "PokemonMansion_1F", "PokemonMansion_3F", "PokemonMansion_B1F",
        "PokemonTower_2F", "PokemonTower_3F", "PokemonTower_4F", "PokemonTower_5F", "PokemonTower_6F", "PokemonTower_7F",
        "SaffronCity_Gym",
        "SilphCo_3F", "SilphCo_5F", "SilphCo_7F", "SilphCo_11F",
        "VictoryRoad_1F", "VictoryRoad_2F", "VictoryRoad_3F",
    };
    return MAPS;
}

//  A Center visit that walks the whole way, and why that is right.
struct CloseCenter{
    std::string_view row;
    std::string_view step;
    std::string_view why;
};

const CloseCenter CLOSE_CENTERS[] = {
    {"14.01", "cinnabar_nurse",
        "Cinnabar Island: the Mansion's door (8, 4) to the Center's (14, 11) across the island's square, as close as the Gym's"},
    {"14.02b", "cinnabar_nurse",
        "Cinnabar Gym's door to Cinnabar's Center, the user's own \"really close\" (2026-11-15)"},
    {"19.05", "indigo_nurse",
        "Indigo Plateau is no Fly point until the player first steps onto it (IndigoPlateau_Exterior's setworldmapflag), "
        "and from its south edge the Center's door is 12 tiles straight north"},
};

bool close_center(std::string_view row, std::string_view step){
    for (const CloseCenter& close : CLOSE_CENTERS){
        if (close.row == row && close.step == step){
            return true;
        }
    }
    return false;
}

//  The nurse, the Center's script in, or a walk up to her.
bool is_center_step(const StoryStep& step){
    return step.kind == StoryStepKind::HEAL || step.action == CENTER_ENTER_VARIANT || step.name.ends_with("_nurse");
}

//  A step that moves the player without a menu or a talk in between.
bool moves(const StoryStep& step){
    static const std::set<std::string, std::less<>> GAME_MOVES{
        "fly", "use_item", "cut_tree", "push_boulder", "lift_floor", "surf", "ride_bike",
    };
    if (is_center_step(step)){
        return false;
    }
    switch (step.kind){
    case StoryStepKind::ROUTE:
    case StoryStepKind::LEGS:
    case StoryStepKind::STEP:
        return true;
    case StoryStepKind::GAME:
        return GAME_MOVES.contains(step.action);
    default:
        return false;
    }
}

//  A Fly or an Escape Rope: the player lands elsewhere without walking.
bool hops(const StoryStep& step){
    return step.action == "fly" || parse_use_item_step(step) == "escape-rope";
}

//  The maps, doors and rules a row's routes are planned on.
struct RowPlanning{
    const std::vector<MapData>& maps;
    Pokemon::GateState gates;
    Pokemon::GateRules rules;
};

//  One step of a row: where it starts, if known, and the tiles it walks.
struct StepWalk{
    std::optional<StoryPose> from;
    int tiles = 0;
    std::string failure;
};

int route_tiles(const std::vector<RouteStep>& route){
    int tiles = 0;
    for (const RouteStep& leg : route){
        tiles += leg.action == RouteAction::WALK ? leg.tiles : 0;
    }
    return tiles;
}

//  `step` walked from `walk.from`: its tiles into `walk`. Where it leaves
//  the player, if known.
std::optional<StoryPose> walk_step(const RowPlanning& planning, const StoryStep& step, StepWalk& walk){
    std::optional<StoryPose> at = walk.from;
    if (at && (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS)){
        const std::optional<std::vector<RouteStep>> route = walked_route(*at, step, planning.maps, planning.gates, planning.rules);
        if (!route){
            walk.failure = std::format("{}: no route from {}.", step.name, to_string(MapPosition{at->map, at->tile}));
            at.reset();
        }else if (!route->empty()){
            walk.tiles = route_tiles(*route);
            at = StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after};
        }
    }else if (at && step.kind == StoryStepKind::STEP){
        const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
        walk.tiles = 1;
        at = StoryPose{at->map, at->tile.next(direction), direction};
    }
    if (step.pose_after){
        at = story_pose(Pokemon::pose_of(*step.pose_after));
    }
    return at;
}

//  Each of `row`'s steps walked in turn from the row's start, on the maps
//  of its start flags.
std::vector<StepWalk> walk_row(const StoryCheckpoint& row){
    const std::set<std::string> flags(row.start_flags.begin(), row.start_flags.end());
    const RowPlanning planning{
        .maps = run_story_maps(flags),
        .gates = row.start_gates.value_or(Pokemon::GateState()),
        .rules = Pokemon::GateRules{.card_key_held = flags.contains("card_key_held")},
    };
    std::vector<StepWalk> walks;
    std::optional<StoryPose> at = story_pose(Pokemon::pose_of(*row.start));
    for (const StoryStep& step : row.steps){
        walks.push_back(StepWalk{.from = at});
        at = walk_step(planning, step, walks.back());
    }
    return walks;
}

//  The first step of the trip that ends before step `end`: the run of
//  steps that only move the player.
size_t trip_start(const StoryCheckpoint& row, size_t end){
    size_t start = end;
    while (start > 0 && moves(row.steps[start - 1])){
        start--;
    }
    return start;
}

//  The trip that ends before row `row`'s step `center` (a Center step, or
//  the row's end) checked: an offender line, or empty.
std::string check_trip(const StoryCheckpoint& row, const std::vector<StepWalk>& walks, size_t center){
    const size_t start = trip_start(row, center);
    const std::optional<StoryPose>& from = walks[start].from;
    const bool in_rope_building = from && escape_rope_maps().contains(from->map);
    int since_hop = 0;
    int before_hop = 0;
    bool hopped = false;
    for (size_t c = start; c < center; c++){
        if (!walks[c].failure.empty()){
            return std::format("{}/{}", row.id, walks[c].failure);
        }
        if (hops(row.steps[c])){
            hopped = true;
            since_hop = 0;
            continue;
        }
        since_hop += walks[c].tiles;
        before_hop += hopped ? 0 : walks[c].tiles;
    }
    const std::string step = center < row.steps.size() ? row.steps[center].name : "end";
    if (in_rope_building && before_hop > MAX_WALK_TO_CENTER){
        return std::format("{}/{}: walks {} tiles out of {}, where an Escape Rope works, then to the Center: use an Escape Rope and Fly.", row.id, step, before_hop, from->map);
    }
    if (since_hop > MAX_WALK_TO_CENTER && !close_center(row.id, step)){
        return std::format("{}/{}: walks {} tiles to the Center{}: Fly there.", row.id, step, since_hop, hopped ? " after its last Fly" : "");
    }
    return "";
}

//  The tile in front of a Pokemon Center's door: one below a warp into a
//  Center.
bool below_center_door(const std::vector<MapData>& maps, const StoryPose& at){
    for (const MapData& map : maps){
        if (map.name != at.map){
            continue;
        }
        for (const MapWarp& warp : map.warps){
            if (warp.to && warp.to->map.find("PokemonCenter") != std::string::npos && warp.x == at.tile.x && warp.y + 1 == at.tile.y){
                return true;
            }
        }
    }
    return false;
}

//  Where each Center trip of `row` ends: the first of a run of Center
//  steps, or the step after a walk that ends in front of a Center's door
//  with no Center step after it (the next row goes in).
std::vector<size_t> center_trip_ends(const StoryCheckpoint& row, const std::vector<StepWalk>& walks){
    std::vector<size_t> ends;
    const std::vector<StoryStep>& steps = row.steps;
    for (size_t c = 0; c < steps.size(); c++){
        if (is_center_step(steps[c]) && (c == 0 || !is_center_step(steps[c - 1]))){
            ends.push_back(c);
            continue;
        }
        const bool trip_goes_on = c + 1 < steps.size() && (moves(steps[c + 1]) || is_center_step(steps[c + 1]));
        const std::optional<StoryPose>& after = c + 1 < walks.size() ? walks[c + 1].from : std::nullopt;
        if (moves(steps[c]) && !trip_goes_on && after && below_center_door(story_maps(), *after)){
            ends.push_back(c + 1);
        }
    }
    return ends;
}

std::vector<std::string> offenders_from(std::string_view first_row){
    std::vector<std::string> offenders;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id < first_row || !row.start){
            continue;
        }
        const std::vector<StepWalk> walks = walk_row(row);
        for (size_t end : center_trip_ends(row, walks)){
            if (std::string offender = check_trip(row, walks, end); !offender.empty()){
                offenders.push_back(std::move(offender));
            }
        }
    }
    return offenders;
}

struct FlyToCenterInput{
    std::string_view first_row;
};

struct FlyToCenterExpectation{
    std::vector<std::string> offenders;
};

struct FlyToCenterCase{
    std::string name;
    FlyToCenterInput input;
    FlyToCenterExpectation expectation;
};

class Test_AutoStoryFlyToCenter : public UnitTest{
public:
    explicit Test_AutoStoryFlyToCenter(FlyToCenterCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Fly to Center: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::string> offenders = offenders_from(m_case.input.first_row);
        if (offenders == m_case.expectation.offenders){
            return true;
        }
        std::string message = std::format("{} offender(s), expected {}:", offenders.size(), m_case.expectation.offenders.size());
        for (const std::string& offender : offenders){
            message += " " + offender;
        }
        return message;
    }

private:
    FlyToCenterCase m_case;
};

const FlyToCenterCase FLY_TO_CENTER_CASES[] = {
    {
        .name = "every Center trip from Fly on flies, or ropes out of a building",
        .input = {.first_row = FIRST_FLY_ROW},
        .expectation = {.offenders = {}},
    },
};

} // namespace


void add_tests_AutoStoryFlyToCenter(UnitTestDatabase& database){
    for (const FlyToCenterCase& item : FLY_TO_CENTER_CASES){
        database.add<Test_AutoStoryFlyToCenter>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
