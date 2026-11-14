/*  AutoStory Safari Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 11.03 on the overworld sim: from below the Safari Zone's door into
 *  its entrance and onto the counter's trigger, then (the script's warp,
 *  where the table says it ends) through the center, the east area to the
 *  FULL RESTORE, the north area and the west area to the GOLD TEETH, into
 *  the Secret House, out, and the Fly to Fuchsia City. And the Safari
 *  Zone's two counts on the way the program walks: its 600 steps, and the
 *  Super Repel's 200, which must last to the last grass tile.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryStartMenu.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySafariInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStorySafariExpectation{
};

struct AutoStorySafariCase{
    AutoStorySafariInput input;
    AutoStorySafariExpectation expectation;
};

class Test_AutoStorySafari : public UnitTest{
public:
    explicit Test_AutoStorySafari(AutoStorySafariCase test_case)
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
    AutoStorySafariCase m_case;
};

//  The Safari Zone's step count (pokefirered SafariZoneTakeStep(): 600 at
//  the entrance) and a Super Repel's (REPEL_STEP_COUNT for ITEM_SUPER_REPEL).
constexpr int SAFARI_STEPS = 600;
constexpr int SUPER_REPEL_STEPS = 200;

const StoryCheckpoint& safari_row(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& data){ return data.id == "11.03"; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint 11.03.");
    }
    return *row;
}

//  The scene after the counter's YES: the script walks the player in and
//  warps them to the center, where the table says.
bool warps_by_script(const StoryStep& step){
    return step.kind == StoryStepKind::MASH && step.pose_after.has_value();
}

bool on_grass(const MapPosition& position){
    for (const MapData& map : story_maps()){
        if (map.name == position.map){
            return map.tiles.at(position.point) == Pokemon::TileKind::TALL_GRASS;
        }
    }
    return false;
}

//  The steps of the Safari Zone the program walks, in order from the
//  center: for each, whether it is on tall grass. The Super Repel's use is
//  where `repel_at` steps had been taken.
struct SafariWalk{
    std::vector<bool> grass;
    std::optional<size_t> repel_at;
};

//  Each ROUTE and LEGS of 11.03 from the center on, planned or laid out as
//  the program walks them (walked_route()), tile by tile.
std::optional<SafariWalk> safari_walk(std::string& failure){
    const StoryCheckpoint& row = safari_row();
    SafariWalk walk;
    std::optional<StoryPose> at;
    for (const StoryStep& step : row.steps){
        if (warps_by_script(step)){
            at = story_pose(Pokemon::pose_of(*step.pose_after));
            continue;
        }
        if (!at){
            continue;
        }
        if (parse_use_item_step(step) == std::optional<std::string>("super-repel")){
            walk.repel_at = walk.grass.size();
            continue;
        }
        if (parse_fly_step(step)){
            return walk;
        }
        if (step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS){
            continue;
        }
        const std::optional<std::vector<RouteStep>> route = walked_route(*at, step);
        if (!route){
            failure = "11.03/" + step.name + ": no route from " + to_string(MapPosition{at->map, at->tile}) + ".";
            return std::nullopt;
        }
        MapPosition position{at->map, at->tile};
        for (const RouteStep& planned : *route){
            for (int tile = 1; tile <= planned.tiles; tile++){
                position.point = position.point.next(planned.direction);
                if (tile == planned.tiles){
                    position = planned.end;
                }
                walk.grass.push_back(on_grass(position));
            }
        }
        if (!route->empty()){
            at = StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after};
        }
    }
    failure = "11.03: no Fly out of the Safari Zone.";
    return std::nullopt;
}

//  11.03 on the sim: in to the counter's trigger, then from the center the
//  Safari Zone's areas to the Secret House and out, and the Fly to Fuchsia
//  City, with no trainer seen and nothing in the way.
UnitTestResult test_safari(){
    const StoryCheckpoint& row = safari_row();
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    auto sim = std::make_unique<OverworldSim>(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row.steps){
        if (warps_by_script(step)){
            const MapPosition trigger{"FuchsiaCity_SafariZone_Entrance", {4, 3}};
            if (!(sim->state().position == trigger)){
                return std::format("11.03: the counter's scene from {}, not its trigger {}.", to_string(sim->state().position), to_string(trigger));
            }
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            sim = std::make_unique<OverworldSim>(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
            continue;
        }
        if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
            return std::format("11.03: ended on {}; {}", to_string(sim->state().position), failure);
        }
    }
    if (!sim->state().trainers_beaten.empty() || !sim->state().violations.empty()){
        return "11.03: a battle or a violation on the way.";
    }
    const MapPosition end{"FuchsiaCity", {25, 32}};
    if (!(sim->state().position == end)){
        return std::format("11.03: ended on {}, not {}.", to_string(sim->state().position), to_string(end));
    }
    return true;
}

//  The way through the Safari Zone fits its 600 steps with a third to spare,
//  and the one Super Repel, used before the first grass, lasts past the
//  last: the lead outlevels every wild Pokemon there, so none shows.
UnitTestResult test_safari_steps(){
    std::string failure;
    const std::optional<SafariWalk> walk = safari_walk(failure);
    if (!walk){
        return failure;
    }
    if (!walk->repel_at){
        return "11.03: no Super Repel in the Safari Zone.";
    }
    const size_t steps = walk->grass.size();
    //  A third left over: a re-anchor or a retried leg doesn't run it out.
    if (steps > SAFARI_STEPS * 2 / 3){
        return std::format("11.03: {} steps in the Safari Zone; at most two thirds of its {}.", steps, SAFARI_STEPS);
    }
    for (size_t step = 0; step < *walk->repel_at; step++){
        if (walk->grass[step]){
            return std::format("11.03: grass on step {}, before the Super Repel (step {}).", step + 1, *walk->repel_at);
        }
    }
    for (size_t step = *walk->repel_at + SUPER_REPEL_STEPS; step < steps; step++){
        if (walk->grass[step]){
            return std::format(
                "11.03: grass on step {}, {} after the Super Repel; it lasts {}.",
                step + 1, step - *walk->repel_at + 1, SUPER_REPEL_STEPS
            );
        }
    }
    return true;
}

//  11.03's Super Repel and Fly open the Safari Zone's START menu; the
//  other rows' keep the full one.
UnitTestResult test_safari_menu(){
    if (start_menu_context(StartMenu::SAFARI) != StartMenuContext::SAFARI_ZONE
        || start_menu_context(StartMenu::FULL) != StartMenuContext::STANDARD
        || start_menu_context(StartMenu::BEFORE_POKEDEX) != StartMenuContext::NO_DEX
    ){
        return "The START menus map to the wrong navigation contexts.";
    }
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            const bool menu_step = parse_use_item_step(step).has_value() || parse_fly_step(step).has_value();
            if (!menu_step){
                continue;
            }
            const StartMenu expected = row.id == "11.03" ? StartMenu::SAFARI : StartMenu::FULL;
            if (step_start_menu(step.args) != expected){
                return std::format("{}/{}: the wrong START menu.", row.id, step.name);
            }
            if (const std::optional<FlyPlan> fly = parse_fly_step(step); fly && fly->start_menu != expected){
                return std::format("{}/{}: the Fly's plan has the wrong START menu.", row.id, step.name);
            }
        }
    }
    return true;
}

const AutoStorySafariCase AUTO_STORY_SAFARI_CASES[] = {
    {.input = {"11.03 the Safari Zone to the Secret House, and Fly to Fuchsia", test_safari}, .expectation = {}},
    {.input = {"11.03 fits the Safari Zone's steps and the Super Repel's", test_safari_steps}, .expectation = {}},
    {.input = {"11.03 opens the Safari Zone's START menu", test_safari_menu}, .expectation = {}},
};

} // namespace


void add_tests_AutoStorySafari(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SAFARI_CASES){
        database.add<Test_AutoStorySafari>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
