/*  AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plans every route of the story table on the map files, from where the
 *  table says the player stands, and checks that each checkpoint starts
 *  where the one before it ends.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_OptionMenuReader.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryMoveData.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryInput{
    std::string name;
    UnitTestResult (*body)();
};

struct AutoStoryExpectation{
    std::string_view description;
};

struct AutoStoryTestCase{
    AutoStoryInput input;
    AutoStoryExpectation expectation;
};

class Test_AutoStory : public UnitTest{
public:
    explicit Test_AutoStory(AutoStoryTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStory - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_scenario.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryTestCase m_scenario;
};

const MapData* find_map_named(const std::string& name){
    for (const MapData& map : story_maps()){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

std::string describe(const std::vector<RouteStep>& route){
    std::string text;
    for (const RouteStep& step : route){
        text += "\n  " + to_string(step);
    }
    return text;
}

std::string describe(const StoryPose& pose){
    return to_string(MapPosition{pose.map, pose.tile})
        + (pose.facing ? std::string(" facing ") + direction_name(*pose.facing) : "");
}

//  A table pose as a StoryPose; FRLG's table has one-tile poses only.
StoryPose one_tile(const Pokemon::GridPose& grid_pose){
    const Pokemon::Pose pose = Pokemon::pose_of(grid_pose);
    const std::optional<StoryPose> ret = story_pose(pose);
    if (!ret){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Not a one-tile pose: " + Pokemon::to_string(pose));
    }
    return *ret;
}

Direction reverse(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return Direction::SOUTH;
    case Direction::EAST:
        return Direction::WEST;
    case Direction::SOUTH:
        return Direction::NORTH;
    case Direction::WEST:
        return Direction::EAST;
    }
    return direction;
}

//  The tiles a route's walking legs pass over on `map`.
std::vector<TilePos> tiles_walked_on(const std::vector<RouteStep>& route, const std::string& map){
    std::vector<TilePos> tiles;
    for (const RouteStep& step : route){
        if (step.action != RouteAction::WALK || step.end.map != map){
            continue;
        }
        TilePos tile = step.end.point;
        for (int c = 0; c < step.tiles; c++){
            tiles.push_back(tile);
            tile = tile.next(reverse(step.direction));
        }
    }
    return tiles;
}

//  Steps that must reach Viridian Forest's gate by legs the walker can hold
//  again after a battle: each ends on a wall or a warp, or is one step.
bool held_legs_only(const std::string& name){
    return name == "forest_east" || name == "forest_exit" || name == "forest_gate";
}
UnitTestResult check_held_legs(const std::string& name, const std::vector<RouteStep>& route){
    for (const RouteStep& step : route){
        if (step.action == RouteAction::WALK && !step.to_wall && !step.warps && step.tiles != 1){
            return name + ": an open leg, " + to_string(step) + ":" + describe(route);
        }
    }
    return true;
}

//  Walks `step`'s fixed legs from `pose` and moves `pose` to their end.
UnitTestResult check_legs(StoryPose& pose, const StoryStep& step){
    const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), MapPosition{pose.map, pose.tile}, Pokemon::story_arg<std::vector<StoryLeg>>(step.args, "legs"));
    if (!route || route->empty()){
        return step.name + ": the legs can't be walked from " + describe(pose) + ".";
    }
    if (held_legs_only(step.name)){
        UnitTestResult result = check_held_legs(step.name, *route);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    pose = StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after};
    return true;
}

//  Plans `step` from `pose` and moves `pose` to its end.
UnitTestResult check_route(StoryPose& pose, const StoryStep& step){
    const MapPosition start{pose.map, pose.tile};
    std::optional<Direction> facing = pose.facing;
    if (!facing){
        const MapData* map = find_map_named(pose.map);
        facing = map ? wall_beside(*map, pose.tile) : std::nullopt;
        if (!facing){
            return step.name + ": the facing is unknown at " + describe(pose) + " and no wall is beside it.";
        }
    }
    const StoryPose target = one_tile(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target"));
    const RouteGoal goal{{target.map, target.tile}, target.facing};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(story_maps(), start, goal, default_route_costs(), facing);
    if (!route){
        return step.name + ": no route from " + to_string(start) + " to " + to_string(goal.position) + ".";
    }
    const MapPosition end = follow_overworld_route(story_maps(), start, *route);
    if (!(end == goal.position)){
        return step.name + ": the route ends at " + to_string(end) + ":" + describe(*route);
    }
    if (goal.facing && !route->empty() && route->back().facing_after != *goal.facing){
        return step.name + ": the route doesn't end facing " + direction_name(*goal.facing) + ":" + describe(*route);
    }
    if (held_legs_only(step.name)){
        UnitTestResult result = check_held_legs(step.name, *route);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    if (step.name == "below_old_man_trigger"){
        for (const TilePos& tile : tiles_walked_on(*route, "ViridianCity")){
            if (tile == TilePos{20, 8} || tile == TilePos{22, 8}){
                return step.name + ": walks over the old man's trigger " + to_string(tile) + ":" + describe(*route);
            }
        }
    }
    pose = StoryPose{target.map, target.tile, route->empty() ? facing : route->back().facing_after};
    return true;
}

//  Follows `checkpoint` from its start; `pose` ends where it leaves the player.
UnitTestResult check_checkpoint(const StoryCheckpoint& checkpoint, StoryPose& pose){
    for (const StoryStep& step : checkpoint.steps){
        if (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS){
            UnitTestResult result = step.kind == StoryStepKind::ROUTE ? check_route(pose, step) : check_legs(pose, step);
            if (result.result != UnitTestResult::PASSED){
                return checkpoint.id + " " + result.message;
            }
        }else if (step.kind == StoryStepKind::READY){
            //  The nurse may be needed: that way must plan too, but the pose
            //  goes on from where the player stands.
            StoryPose nurse_pose = pose;
            StoryStep to_nurse = step;
            UnitTestResult result = check_route(nurse_pose, to_nurse);
            if (result.result != UnitTestResult::PASSED){
                return checkpoint.id + " " + result.message;
            }
        }else if (step.kind == StoryStepKind::STEP){
            const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
            pose = StoryPose{pose.map, pose.tile.next(direction), direction};
        }
        if (step.pose_after){
            pose = one_tile(Pokemon::grid_pose_of(*step.pose_after));
        }
    }
    return true;
}

UnitTestResult test_routes_and_chain(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    if (checkpoints.size() != 10 || checkpoints[0].start){
        return "Expected 10 checkpoints, the first from a new game.";
    }
    StoryPose pose;
    for (size_t c = 0; c < checkpoints.size(); c++){
        const StoryCheckpoint& checkpoint = checkpoints[c];
        if (c > 0){
            const StoryPose start = one_tile(Pokemon::grid_pose_of(*checkpoint.start));
            const bool same_facing = !start.facing || start.facing == pose.facing;
            if (start.map != pose.map || !(start.tile == pose.tile) || !same_facing){
                return checkpoint.id + " starts at " + describe(start) + ", but " + checkpoints[c - 1].id + " ends at " + describe(pose) + ".";
            }
            pose = start;
        }
        UnitTestResult result = check_checkpoint(checkpoint, pose);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    return true;
}

struct ExpectedLeg{
    Direction direction;
    int tiles;
    TilePos end;
};

//  Plans `start` to `goal` on the story maps and checks the walking legs
//  on `map`, except the step that warps onto it, against `expected`.
UnitTestResult check_legs_on(
    const MapPosition& start, Direction start_facing, const RouteGoal& goal,
    const std::string& map, const std::vector<ExpectedLeg>& expected
){
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        story_maps(), start, goal, default_route_costs(), start_facing
    );
    if (!route){
        return "No route from " + to_string(start) + " to " + to_string(goal.position) + ".";
    }
    std::vector<RouteStep> legs;
    for (const RouteStep& step : *route){
        if (step.action == RouteAction::WALK && !step.warps && step.end.map == map){
            legs.push_back(step);
        }
    }
    bool same = legs.size() == expected.size();
    for (size_t c = 0; same && c < legs.size(); c++){
        same = legs[c].direction == expected[c].direction && legs[c].tiles == expected[c].tiles && legs[c].end.point == expected[c].end;
    }
    if (!same){
        return "Unexpected legs on " + map + " from " + to_string(start) + " to " + to_string(goal.position) + ":" + describe(*route);
    }
    return true;
}

//  00.09, each walk from where the one before leaves the player.
UnitTestResult test_pewter_gym_walks(){
    const MapPosition nurse{"PewterCity_PokemonCenter_1F", {7, 4}};
    //  In by the door's mat (6, 14), straight up column 6 to below Liam's row.
    UnitTestResult result = check_legs_on(
        nurse, Direction::NORTH, RouteGoal{{"PewterCity_Gym", {6, 9}}, Direction::NORTH},
        "PewterCity_Gym", {{Direction::NORTH, 5, {6, 9}}}
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    //  The step onto (6, 8), into Liam's line.
    const MapData& gym = *find_map_named("PewterCity_Gym");
    if (classify_step(gym, PlayerState{{6, 9}, elevation_at(gym, {6, 9}), false}, Direction::NORTH) != StepKind::WALK){
        return "Can't step north from (6, 9) onto (6, 8).";
    }
    //  After Liam, on to Brock: two tiles north.
    result = check_legs_on(
        MapPosition{"PewterCity_Gym", {6, 8}}, Direction::NORTH, RouteGoal{{"PewterCity_Gym", {6, 6}}, Direction::NORTH},
        "PewterCity_Gym", {{Direction::NORTH, 2, {6, 6}}}
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    //  The nurse's way: from her counter back in and up column 6 to Brock.
    return check_legs_on(
        nurse, Direction::NORTH, RouteGoal{{"PewterCity_Gym", {6, 6}}, Direction::NORTH},
        "PewterCity_Gym", {{Direction::NORTH, 8, {6, 6}}}
    );
}

//  00.10: out of the Gym to the aide's top trigger. The route keeps off his
//  three trigger tiles (46, 21..23), which would start the scene a step
//  early, and the step east from its end lands on (46, 21).
UnitTestResult test_pewter_aide_walk(){
    const MapPosition gym{"PewterCity_Gym", {6, 6}};
    const MapPosition goal{"PewterCity", {45, 21}};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        story_maps(), gym, RouteGoal{goal, Direction::EAST}, default_route_costs(), Direction::NORTH
    );
    if (!route || route->empty()){
        return "No route from " + to_string(gym) + " to " + to_string(goal) + ".";
    }
    MapPosition from = gym;
    for (const RouteStep& step : *route){
        if (step.action != RouteAction::WALK){
            continue;
        }
        for (const MapPosition& tile : leg_tiles(story_maps(), from, step)){
            if (tile.map == "PewterCity" && tile.point.x == 46 && tile.point.y >= 21 && tile.point.y <= 23){
                return "The route crosses the aide's trigger " + to_string(tile) + ":" + describe(*route);
            }
        }
        from = step.end;
    }
    if (route->back().end != goal){
        return "Expected the route to end at " + to_string(goal) + ", got " + to_string(route->back().end) + ".";
    }
    const MapData& pewter = *find_map_named("PewterCity");
    if (classify_step(pewter, PlayerState{{45, 21}, elevation_at(pewter, {45, 21}), false}, Direction::EAST) != StepKind::WALK){
        return "Can't step east from (45, 21) onto the trigger (46, 21).";
    }
    return true;
}

//  Every ROUTE and LEGS after the Running Shoes (00.10) runs; none before.
UnitTestResult test_runs_after_shoes(){
    bool has_shoes = false;
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
            if (step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS){
                continue;
            }
            const MoveSpeed expected = has_shoes ? MoveSpeed::RUN : MoveSpeed::WALK;
            if (move_speed(step) != expected){
                return std::format("{}/{}: expected to {}, it would {}.", checkpoint.id, step.name, move_speed_name(expected), move_speed_name(move_speed(step)));
            }
        }
        has_shoes = has_shoes || checkpoint.id == "00.10";
    }
    if (!has_shoes){
        return "No checkpoint 00.10, where the player gets the Running Shoes.";
    }
    return true;
}

//  Every checkpoint that starts from a save has a place the save card
//  names, so a save left elsewhere is caught before it runs; and the
//  places of the starts that matter differ.
UnitTestResult test_save_places(){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        if (checkpoint.start && save_card_place(Pokemon::grid_pose_of(*checkpoint.start).map).empty()){
            return checkpoint.id + " starts on " + Pokemon::grid_pose_of(*checkpoint.start).map + ", which has no save card place.";
        }
    }
    if (save_card_place("ViridianForest") != "viridian-forest" || save_card_place("ViridianCity_Mart") != "viridian-city"
        || save_card_place("PewterCity_PokemonCenter_1F") != "pewter-city" || save_card_place("PalletTown_ProfessorOaksLab") != "pallet-town"
    ){
        return "Wrong save card places.";
    }
    if (!save_card_place("Route22").empty()){
        return "A place for a map the table doesn't use.";
    }
    return true;
}

//  A reload's save checked against the checkpoint's start: a save left in
//  Pewter City (a new game whose save didn't happen) can't start 00.02 in
//  the bedroom. The check takes no checks level: Fast runs it too.
UnitTestResult test_save_place_error(){
    struct Case{
        std::string start_map;
        std::string place;
        bool error;
    };
    const Case CASES[] = {
        {.start_map = "PalletTown_PlayersHouse_2F", .place = "pewter-city", .error = true},
        {.start_map = "PalletTown_PlayersHouse_2F", .place = "pallet-town", .error = false},
        //  A card that can't be read is let pass.
        {.start_map = "PalletTown_PlayersHouse_2F", .place = "", .error = false},
        {.start_map = "ViridianForest", .place = "viridian-city", .error = true},
        //  A map with no known place isn't checked.
        {.start_map = "Route22", .place = "pewter-city", .error = false},
    };
    for (const Case& c : CASES){
        const std::string error = save_place_error(c.start_map, c.place);
        if (error.empty() == c.error){
            return "A save in \"" + c.place + "\" for a start on " + c.start_map + ": expected " +
                (c.error ? "an error" : "no error") + ", got \"" + error + "\".";
        }
    }
    return true;
}

std::string_view save_action_name(SaveAction action){
    switch (action){
    case SaveAction::WAIT:
        return "WAIT";
    case SaveAction::ANSWER_YES:
        return "ANSWER_YES";
    case SaveAction::NEXT_PAGE:
        return "NEXT_PAGE";
    case SaveAction::CHECK_OVERWORLD:
        return "CHECK_OVERWORLD";
    case SaveAction::FAIL_CANCELLED:
        return "FAIL_CANCELLED";
    case SaveAction::FAIL_ERROR:
        return "FAIL_ERROR";
    }
    return "?";
}

//  save_game()'s answers to the save dialogs of pokefirered's start_menu.c,
//  screen by screen from the first YES/NO.
UnitTestResult test_save_dialog(){
    using S = SaveScreen;
    using A = SaveAction;
    struct Case{
        std::string name;
        std::vector<std::pair<SaveScreen, SaveAction>> screens;
    };
    const std::vector<Case> CASES = {
        {
            .name = "first save",
            .screens = {{S::YES_NO, A::ANSWER_YES}, {S::TEXT, A::WAIT}, {S::TEXT, A::WAIT}, {S::NOTHING, A::WAIT}, {S::NOTHING, A::CHECK_OVERWORLD}},
        },
        {
            .name = "over this game's save",
            .screens = {{S::YES_NO, A::ANSWER_YES}, {S::TEXT, A::WAIT}, {S::YES_NO, A::ANSWER_YES}, {S::TEXT, A::WAIT}, {S::NOTHING, A::WAIT}, {S::NOTHING, A::CHECK_OVERWORLD}},
        },
        {
            //  WARNING!, four pages, then the YES/NO that starts on NO.
            .name = "a new game over another game's save",
            .screens = {
                {S::YES_NO, A::ANSWER_YES},
                {S::PAGED_TEXT, A::NEXT_PAGE}, {S::NOTHING, A::WAIT}, {S::PAGED_TEXT, A::NEXT_PAGE}, {S::PAGED_TEXT, A::NEXT_PAGE},
                {S::TEXT, A::WAIT}, {S::YES_NO, A::ANSWER_YES}, {S::TEXT, A::WAIT}, {S::NOTHING, A::WAIT}, {S::NOTHING, A::CHECK_OVERWORLD},
            },
        },
        {
            .name = "cancelled: the START menu is back",
            .screens = {{S::YES_NO, A::ANSWER_YES}, {S::TEXT, A::WAIT}, {S::START_MENU, A::FAIL_CANCELLED}},
        },
        {
            .name = "Save error.",
            .screens = {{S::YES_NO, A::ANSWER_YES}, {S::TEXT, A::WAIT}, {S::PAGED_TEXT, A::NEXT_PAGE}, {S::NOTHING, A::WAIT}, {S::NOTHING, A::FAIL_ERROR}},
        },
    };
    for (const Case& c : CASES){
        SaveDialog dialog;
        for (size_t i = 0; i < c.screens.size(); i++){
            const SaveAction action = dialog.next(c.screens[i].first);
            if (action != c.screens[i].second){
                return std::format("{}, screen {}: expected {}, got {}.", c.name, i + 1, save_action_name(c.screens[i].second), save_action_name(action));
            }
        }
    }
    return true;
}

UnitTestResult test_lint(){
    //  FRLG's own action args (a battle's trainer and foes), with no console.
    const Pokemon::StoryActionTable table = make_story_action_table([](StoryActionId){
        return Pokemon::StoryAction{};
    });
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(checkpoint, table);
        if (!problems.empty()){
            return problems[0];
        }
    }
    return true;
}

//  Every battle step in the table names its trainer and that trainer's
//  party, each species one the battle data has.
UnitTestResult test_story_trainers(){
    size_t battles = 0;
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
            if (step.kind != StoryStepKind::BATTLE){
                continue;
            }
            battles++;
            const std::string& trainer = Pokemon::story_arg<std::string>(step.args, "trainer");
            const std::vector<std::string>& foes = Pokemon::story_arg<std::vector<std::string>>(step.args, "foes");
            if (trainer.empty() || foes.empty()){
                return std::format("{}, {}: no trainer or no foes.", checkpoint.id, step.name);
            }
            for (const std::string& foe : foes){
                if (species_data(foe) == nullptr){
                    return std::format("{}, {}: unknown foe {}.", checkpoint.id, step.name, foe);
                }
            }
        }
    }
    if (battles == 0){
        return "Expected battle steps in the table.";
    }
    return true;
}

UnitTestResult test_unique_names(){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (size_t i = 0; i < checkpoint.steps.size(); i++){
            for (size_t j = i + 1; j < checkpoint.steps.size(); j++){
                if (checkpoint.steps[i].name == checkpoint.steps[j].name){
                    return checkpoint.id + " has two steps named " + checkpoint.steps[i].name + ".";
                }
            }
        }
    }
    return true;
}
//  The intro's names: the player's keyboard first, then the rival's list,
//  where a preset is picked; a keyboard after the player's means the list
//  was missed.
UnitTestResult test_intro_names(){
    struct Case{
        std::string_view what;
        IntroNameScreen screen;
        size_t keyboards_seen;
        size_t presets_picked;
        IntroNameStep expected;
    };
    const std::vector<Case> cases{
        {.what = "the first keyboard", .screen = IntroNameScreen::KEYBOARD, .keyboards_seen = 0, .presets_picked = 0, .expected = IntroNameStep::PLAYER_KEYBOARD},
        {.what = "the list after the player's keyboard", .screen = IntroNameScreen::RIVAL_LIST, .keyboards_seen = 1, .presets_picked = 0, .expected = IntroNameStep::RIVAL_PRESET},
        {.what = "a list read before the player's keyboard", .screen = IntroNameScreen::RIVAL_LIST, .keyboards_seen = 0, .presets_picked = 0, .expected = IntroNameStep::NONE},
        {.what = "the second keyboard", .screen = IntroNameScreen::KEYBOARD, .keyboards_seen = 1, .presets_picked = 0, .expected = IntroNameStep::RIVAL_KEYBOARD},
        {.what = "a keyboard after a preset was picked", .screen = IntroNameScreen::KEYBOARD, .keyboards_seen = 1, .presets_picked = 1, .expected = IntroNameStep::RIVAL_KEYBOARD},
        {.what = "the list again after a NO", .screen = IntroNameScreen::RIVAL_LIST, .keyboards_seen = 1, .presets_picked = 1, .expected = IntroNameStep::RIVAL_PRESET},
        {.what = "the list after every try", .screen = IntroNameScreen::RIVAL_LIST, .keyboards_seen = 1, .presets_picked = RIVAL_PRESET_TRIES, .expected = IntroNameStep::NONE},
        {.what = "a message box", .screen = IntroNameScreen::OTHER, .keyboards_seen = 1, .presets_picked = 0, .expected = IntroNameStep::NONE},
    };
    for (const Case& c : cases){
        const IntroNameStep step = intro_name_step(c.screen, c.keyboards_seen, c.presets_picked);
        if (step != c.expected){
            return std::string(c.what) + ": expected step " + std::to_string(static_cast<int>(c.expected))
                + ", got " + std::to_string(static_cast<int>(step)) + ".";
        }
    }
    //  A black screen is no list.
    ImageRGB32 black(1920, 1080);
    black.fill(0xff000000);
    if (rival_name_list_cursor(black).has_value()){
        return "Read the rival's name list on a black screen.";
    }
    return true;
}

//  set_options()'s RIGHT presses from each value an old save can leave:
//  on console B a save with FAST and SET became SLOW and SHIFT when the
//  presses assumed MID and SHIFT.
UnitTestResult test_option_presses(){
    const std::vector<std::string>& speed = option_menu_values(OptionMenuItem::TEXT_SPEED);
    const std::vector<std::string>& scene = option_menu_values(OptionMenuItem::BATTLE_SCENE);
    const std::vector<std::string>& style = option_menu_values(OptionMenuItem::BATTLE_STYLE);
    struct Case{
        const std::vector<std::string>& values;
        std::string current;
        std::string target;
        std::optional<size_t> expected;
    };
    const std::vector<Case> cases{
        {.values = speed, .current = "slow", .target = "fast", .expected = 2},
        {.values = speed, .current = "mid", .target = "fast", .expected = 1},
        {.values = speed, .current = "fast", .target = "fast", .expected = 0},
        {.values = scene, .current = "on", .target = "off", .expected = 1},
        {.values = scene, .current = "off", .target = "off", .expected = 0},
        {.values = style, .current = "shift", .target = "set", .expected = 1},
        {.values = style, .current = "set", .target = "set", .expected = 0},
        //  RIGHT wraps from the last value to the first.
        {.values = speed, .current = "fast", .target = "slow", .expected = 1},
        //  Unread, or another row's value: no guess.
        {.values = speed, .current = "", .target = "fast", .expected = std::nullopt},
        {.values = style, .current = "fast", .target = "set", .expected = std::nullopt},
        {.values = scene, .current = "set", .target = "off", .expected = std::nullopt},
    };
    for (const Case& c : cases){
        const std::optional<size_t> presses = option_right_presses(c.values, c.current, c.target);
        if (presses != c.expected){
            auto text = [](const std::optional<size_t>& n){
                return n ? std::to_string(*n) + " RIGHT" : std::string("no presses");
            };
            return "\"" + c.current + "\" to " + c.target + ": expected " + text(c.expected) + ", got " + text(presses) + ".";
        }
    }
    return true;
}

const AutoStoryTestCase AUTO_STORY_CASES[] = {
    {
        .input = {.name = "every route plans, and checkpoints chain", .body = test_routes_and_chain},
        .expectation = {.description = "verifies route planning validity across all chained checkpoints"},
    },
    {
        .input = {.name = "step names are unique per checkpoint", .body = test_unique_names},
        .expectation = {.description = "ensures step identifier uniqueness within checkpoints"},
    },
    {
        .input = {.name = "every save has a place on the save card", .body = test_save_places},
        .expectation = {.description = "validates save location identifiers on save card"},
    },
    {
        .input = {.name = "a reload's save is checked against the start", .body = test_save_place_error},
        .expectation = {.description = "detects mismatch between checkpoint initial state and save point"},
    },
    {
        .input = {.name = "save dialogs: every YES/NO is YES, a cancel fails", .body = test_save_dialog},
        .expectation = {.description = "verifies save confirmation prompt interactions"},
    },
    {
        .input = {.name = "every checkpoint lints clean", .body = test_lint},
        .expectation = {.description = "validates semantic lint rules across story step sequence"},
    },
    {
        .input = {.name = "every story battle's plan names a trainer and its party", .body = test_story_trainers},
        .expectation = {.description = "ensures all scripted story battles list known foes"},
    },
    {
        .input = {.name = "Pewter Gym walks", .body = test_pewter_gym_walks},
        .expectation = {.description = "verifies Pewter Gym route planning and obstacle avoidance"},
    },
    {
        .input = {.name = "Pewter: to the aide's trigger", .body = test_pewter_aide_walk},
        .expectation = {.description = "plans path to Running Shoes aide trigger tile"},
    },
    {
        .input = {.name = "routes run after the Running Shoes", .body = test_runs_after_shoes},
        .expectation = {.description = "switches movement speed to running once shoes acquired"},
    },
    {
        .input = {.name = "intro: names, the rival's from the list", .body = test_intro_names},
        .expectation = {.description = "verifies character and rival naming inputs"},
    },
    {
        .input = {.name = "options: RIGHT presses from the shown value", .body = test_option_presses},
        .expectation = {.description = "calculates Dpad RIGHT inputs for setting game options"},
    },
};

} // namespace


void add_tests_AutoStory(UnitTestDatabase& database){
    for (const auto& scenario : AUTO_STORY_CASES){
        database.add<Test_AutoStory>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
