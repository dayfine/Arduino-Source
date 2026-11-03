/*  AutoStory Run Plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which rows of the story table a run takes (plan_story_run()) with the
 *  RNG programs' handoffs on and off, the rows the handoffs split or
 *  join, and what 00.15 runs before the rival.
 *
 */

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_AutoStory : public UnitTest{
public:
    Test_AutoStory(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::AutoStory - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};


size_t row_index(std::string_view id){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    for (size_t c = 0; c < checkpoints.size(); c++){
        if (checkpoints[c].id == id){
            return c;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint " + std::string(id));
}

std::string joined_ids(const std::vector<StoryCheckpoint>& rows){
    std::string text;
    for (const StoryCheckpoint& row : rows){
        text += (text.empty() ? "" : " ") + row.id;
    }
    return text;
}

const char* handoff_name(StoryHandoff handoff){
    switch (handoff){
    case StoryHandoff::NONE:
        return "none";
    case StoryHandoff::SID_HELPER:
        return "SID Helper";
    case StoryHandoff::STARTER_RNG:
        return "Starter RNG";
    case StoryHandoff::GIFT_RNG:
        return "Gift RNG";
    }
    return "?";
}

struct RunPlanScenario{
    std::string name = "";
    std::string first = "";
    //  The table's last row if empty.
    std::string last = "";
    StoryHandoffs handoffs = {};
    std::string expected_ids = "";
    StoryHandoff expected_stop = StoryHandoff::NONE;
};

class Test_RunPlan : public UnitTest{
public:
    Test_RunPlan(RunPlanScenario scenario)
        : UnitTest("PokemonFRLG::AutoStory - run plan: " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            const size_t last = m_scenario.last.empty() ? story_checkpoints().size() - 1 : row_index(m_scenario.last);
            const StoryRunPlan plan = plan_story_run(story_checkpoints(), row_index(m_scenario.first), last, m_scenario.handoffs);
            const std::string ids = joined_ids(plan.rows);
            if (ids != m_scenario.expected_ids || plan.stop != m_scenario.expected_stop){
                return "Expected rows \"" + m_scenario.expected_ids + "\", stop " + handoff_name(m_scenario.expected_stop)
                    + "; got \"" + ids + "\", stop " + handoff_name(plan.stop) + ".";
            }
            return true;
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    const RunPlanScenario m_scenario;
};

void add_tests_RunPlan(UnitTestDatabase& database){
    const std::string TODAY = "00.01 00.02 00.03 00.04 00.05 00.06 00.07 00.08 00.09 00.10 00.11 00.12 00.12c 00.13 00.14 00.15 00.15b 00.16";
    const RunPlanScenario scenarios[] = {
        {.name = "handoffs off: the story as before", .first = "00.01", .expected_ids = TODAY},
        {.name = "handoffs off: a range", .first = "00.02", .last = "00.04", .expected_ids = "00.02 00.03 00.04"},
        {.name = "handoffs off: Mt. Moon to Nugget Bridge", .first = "00.13", .last = "00.16", .expected_ids = "00.13 00.14 00.15 00.15b 00.16"},
        {
            .name = "SID Helper: 00.01 stops on the keyboard",
            .first = "00.01",
            .handoffs = {.sid_helper = true},
            .expected_ids = "",
            .expected_stop = StoryHandoff::SID_HELPER,
        },
        {
            .name = "SID Helper: back in at 00.01b",
            .first = "00.01b",
            .last = "00.03",
            .handoffs = {.sid_helper = true},
            .expected_ids = "00.01b 00.02 00.03",
        },
        {.name = "00.01b runs when started at, option off", .first = "00.01b", .last = "00.02", .expected_ids = "00.01b 00.02"},
        {.name = "SID Helper: a later start passes over it", .first = "00.02", .last = "00.03", .handoffs = {.sid_helper = true}, .expected_ids = "00.02 00.03"},
        {.name = "handoffs off: an end at 00.04 still takes the rival's battle", .first = "00.03", .last = "00.04", .expected_ids = "00.03 00.04"},
        {
            .name = "Starter RNG: stops after 00.04's save",
            .first = "00.02",
            .handoffs = {.starter_rng = true},
            .expected_ids = "00.02 00.03 00.04",
            .expected_stop = StoryHandoff::STARTER_RNG,
        },
        {.name = "00.04b runs from its own save", .first = "00.04b", .last = "00.05", .expected_ids = "00.04b 00.05"},
        {.name = "Starter RNG: a later start doesn't stop", .first = "00.05", .last = "00.06", .handoffs = {.starter_rng = true}, .expected_ids = "00.05 00.06"},
        {
            .name = "both handoffs, back in after SID Helper",
            .first = "00.01b",
            .handoffs = {.sid_helper = true, .starter_rng = true},
            .expected_ids = "00.01b 00.02 00.03 00.04",
            .expected_stop = StoryHandoff::STARTER_RNG,
        },
    };
    for (const RunPlanScenario& scenario : scenarios){
        database.add<Test_RunPlan>(scenario);
    }
}

void add_tests_GiftRunPlan(UnitTestDatabase& database){
    const RunPlanScenario scenarios[] = {
        {
            .name = "Gift RNG: stops after 00.12's save",
            .first = "00.11",
            .handoffs = {.gift_rng = true},
            .expected_ids = "00.11 00.12",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
        {.name = "Gift RNG off: 00.12b is passed over", .first = "00.11", .expected_ids = "00.11 00.12 00.12c 00.13 00.14 00.15 00.15b 00.16"},
        {.name = "Gift RNG off: 00.12b runs when started at", .first = "00.12b", .expected_ids = "00.12b 00.12c 00.13 00.14 00.15 00.15b 00.16"},
        {
            .name = "Gift RNG: started at 00.12b, from the save beside the salesman",
            .first = "00.12b",
            .handoffs = {.gift_rng = true},
            .expected_ids = "",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
        {
            .name = "Gift RNG: started at 00.12b with Starter RNG on too",
            .first = "00.12b",
            .handoffs = {.sid_helper = true, .starter_rng = true, .gift_rng = true},
            .expected_ids = "",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
        {.name = "Starter RNG alone: 00.12b runs when started at", .first = "00.12b", .handoffs = {.starter_rng = true}, .expected_ids = "00.12b 00.12c 00.13 00.14 00.15 00.15b 00.16"},
        {
            .name = "Starter RNG first, with both on",
            .first = "00.03",
            .handoffs = {.starter_rng = true, .gift_rng = true},
            .expected_ids = "00.03 00.04",
            .expected_stop = StoryHandoff::STARTER_RNG,
        },
    };
    for (const RunPlanScenario& scenario : scenarios){
        database.add<Test_RunPlan>(scenario);
    }
}

std::string step_names(const StoryCheckpoint& checkpoint){
    std::string text;
    for (const StoryStep& step : checkpoint.steps){
        text += (text.empty() ? "" : " ") + step.name;
    }
    return text;
}

//  With Starter RNG's handoff off, 00.04 and 00.04b run as the one 00.04
//  the story had before the split: from the lab's table to the rival's
//  battle, saving once with the starter's START menu.
UnitTestResult test_starter_rows_joined(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const StoryRunPlan plan = plan_story_run(checkpoints, row_index("00.04"), row_index("00.04b"), StoryHandoffs{});
    const std::string expected_steps =
        "below_squirtle squirtle_ball choose_squirtle no_nickname rival_takes_bulbasaur above_rival_trigger rival_trigger rival_battle";
    if (plan.rows.size() != 1){
        return "Expected one checkpoint, got \"" + joined_ids(plan.rows) + "\".";
    }
    const StoryCheckpoint& row = plan.rows[0];
    if (row.id != "00.04" || row.title != "Squirtle, then the rival's battle" || step_names(row) != expected_steps){
        return "Expected 00.04 \"Squirtle, then the rival's battle\": " + expected_steps
            + "; got " + row.id + " \"" + row.title + "\": " + step_names(row);
    }
    const std::optional<StoryPose> start = story_pose(*row.start);
    if (!start || start->map != "PalletTown_ProfessorOaksLab" || !(start->tile == TilePos{6, 4}) || row.menu != StartMenu::BEFORE_POKEDEX){
        return "Expected the joined 00.04 to start at the table (6, 4) and save with the starter's START menu.";
    }
    //  Split, 00.04 saves under the ball facing it, before the starter.
    const StoryCheckpoint& to_ball = checkpoints[row_index("00.04")];
    const std::optional<StoryPose> ball = story_pose(*checkpoints[row_index("00.04b")].start);
    if (to_ball.handoff != StoryHandoff::STARTER_RNG || to_ball.menu != StartMenu::BEFORE_POKEMON
        || !ball || !(ball->tile == TilePos{9, 5}) || ball->facing != Direction::NORTH
    ){
        return "Expected 00.04 to end under Squirtle's ball (9, 5) facing north, saving before the starter.";
    }
    return true;
}

//  The ways back in after SID Helper and Gift RNG load no save and start
//  where 00.01 and 00.12 leave the player, so the rows after them follow
//  unchanged.
UnitTestResult test_after_sid_helper_row(){
    const StoryCheckpoint& sid = story_checkpoints()[row_index("00.01b")];
    if (!sid.live_start || story_checkpoints()[0].handoff != StoryHandoff::SID_HELPER){
        return "Expected 00.01 to be SID Helper's handoff and 00.01b to start from the live game.";
    }
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        if (checkpoint.live_start != (checkpoint.id == "00.01b" || checkpoint.id == "00.12b")){
            return checkpoint.id + ": only 00.01b and 00.12b start from the live game.";
        }
    }
    return true;
}

//  The index of the first step of `steps` from `from` that `matches`, or
//  steps.size().
size_t find_step(const std::vector<StoryStep>& steps, size_t from, const std::function<bool(const StoryStep&)>& matches){
    for (size_t c = from; c < steps.size(); c++){
        if (matches(steps[c])){
            return c;
        }
    }
    return steps.size();
}

//  Whether a route plans from `from` to `to` (ending facing as `to` does).
bool plans(const Pokemon::Pose& from, const Pokemon::Pose& to){
    const std::optional<StoryPose> start = story_pose(from);
    const std::optional<StoryPose> goal = story_pose(to);
    if (!start || !goal){
        return false;
    }
    return plan_overworld_route(
        story_maps(), MapPosition{start->map, start->tile}, RouteGoal{MapPosition{goal->map, goal->tile}, goal->facing},
        default_route_costs(), start->facing.value_or(Direction::NORTH)
    ).has_value();
}

//  00.15, before the rival: Potions from Cerulean's Mart (a loaded save's
//  bag may hold too few for the wild battles a GRIND fights), then a GRIND
//  on Route 4's grass east of Mt. Moon to Lv22, where Bite beats his party
//  (#211), healing at Cerulean's Center; its trip there plans both ways.
UnitTestResult test_cerulean_mart_and_grind(){
    const std::vector<StoryStep>& steps = story_checkpoints()[row_index("00.15")].steps;
    const size_t mart = find_step(steps, 0, [](const StoryStep& step){
        const std::optional<MartPlan> plan = parse_mart_variant(step.variant);
        return plan && plan->map == "CeruleanCity_Mart" && !plan->lines.empty() && plan->lines[0].item == "potion";
    });
    const size_t grind = find_step(steps, mart, [](const StoryStep& step){
        const std::optional<GrindPlan> plan = parse_grind_variant(step.variant);
        return plan && plan->member == 0 && plan->level >= 22;
    });
    if (mart == steps.size() || grind == steps.size()){
        return "00.15: expected a Cerulean Mart step buying Potions, then a GRIND of the lead to Lv22.";
    }
    const std::optional<StoryPose> clerk = mart > 0 && steps[mart - 1].target ? story_pose(*steps[mart - 1].target) : std::nullopt;
    if (!clerk || clerk->map != "CeruleanCity_Mart"){
        return "00.15: expected the Mart step right after a route to the clerk.";
    }
    if (grind == 0 || !steps[grind - 1].target || !steps[grind].target){
        return "00.15: expected the GRIND right after a route to its patch, with the nurse as its target.";
    }
    const Pokemon::Pose& patch = *steps[grind - 1].target;
    const Pokemon::Pose& nurse = *steps[grind].target;
    const std::optional<StoryPose> nurse_pose = story_pose(nurse);
    if (!nurse_pose || nurse_pose->map != "CeruleanCity_PokemonCenter_1F" || !plans(patch, nurse) || !plans(nurse, patch)){
        return "00.15: expected the GRIND's trips to Cerulean's nurse and back to plan.";
    }
    return true;
}

//  00.15b, the rival's battle as its own checkpoint, as every battle lost
//  twice or more gets: it starts from a save, 00.15's on (23, 7) below his
//  trigger with the nurse just before, so a verify run or a retry reloads
//  right before him; and his is its only battle.
UnitTestResult test_cerulean_rival_row(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t index = row_index("00.15b");
    const StoryCheckpoint& rival = checkpoints[index];
    const std::optional<StoryPose> start = rival.start ? story_pose(*rival.start) : std::nullopt;
    if (!start || rival.live_start || start->map != "CeruleanCity" || !(start->tile == TilePos{23, 7})
        || save_card_place(start->map).empty()
    ){
        return "00.15b: expected it to start from a save on (23, 7) in Cerulean City.";
    }
    std::string battles;
    for (const StoryStep& step : rival.steps){
        battles += step.kind == StoryStepKind::BATTLE ? " " + step.name : "";
    }
    if (battles != " rival_cerulean" || rival.steps.size() != 2 || rival.steps[0].name != "rival_trigger"){
        return "00.15b: expected only the step onto his trigger and his battle; its battles:" + battles;
    }
    const StoryCheckpoint& before = checkpoints[index - 1];
    const size_t n = before.steps.size();
    const std::optional<StoryPose> end = n > 0 && before.steps[n - 1].target ? story_pose(*before.steps[n - 1].target) : std::nullopt;
    if (before.id != "00.15" || n < 2 || before.steps[n - 2].kind != StoryStepKind::HEAL
        || !end || end->map != start->map || !(end->tile == start->tile)
    ){
        return "00.15: expected it to end with the nurse, then the walk to (23, 7) where 00.15b starts.";
    }
    for (const StoryStep& step : before.steps){
        if (step.kind == StoryStepKind::BATTLE){
            return "00.15: expected no trainer battle before its save; found " + step.name + ".";
        }
    }
    return true;
}

}


void add_tests_AutoStoryRunPlan(UnitTestDatabase& database){
    add_tests_RunPlan(database);
    add_tests_GiftRunPlan(database);
    database.add<Test_AutoStory>("00.01b and 00.12b: back in after SID Helper and Gift RNG", test_after_sid_helper_row);
    database.add<Test_AutoStory>("00.04 and 00.04b: one checkpoint with Starter RNG off", test_starter_rows_joined);
    database.add<Test_AutoStory>("00.15: Potions at Cerulean's Mart and a GRIND before the rival", test_cerulean_mart_and_grind);
    database.add<Test_AutoStory>("00.15b: the rival's battle alone, from a save before him", test_cerulean_rival_row);
}



}
}
}
