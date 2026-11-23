/*  AutoStory Run Plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which rows of the story table a run takes (plan_story_run()) with the
 *  RNG programs' handoffs on and off, the rows the handoffs split or
 *  join, Misty in Cerulean Gym, and what 05.03 runs before the rival.
 *
 */

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeach.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Whether a Water Pulse teach step forgets Tackle: its keep list holds
//  other moves and not Tackle.
bool forgets_tackle(const TeachPlan& plan){
    return !plan.keep.empty() && std::find(plan.keep.begin(), plan.keep.end(), "tackle") == plan.keep.end();
}

struct AutoStoryInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryExpectation{
};

struct AutoStoryCase{
    AutoStoryInput input;
    AutoStoryExpectation expectation;
};

class Test_AutoStory : public UnitTest{
public:
    explicit Test_AutoStory(AutoStoryCase test_case)
        : UnitTest("PokemonFRLG::AutoStory - " + test_case.input.name)
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
    AutoStoryCase m_case;
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

std::string handoff_name(StoryHandoff handoff){
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

struct RunPlanInput{
    std::string name = "";
    std::string first = "";
    //  The table's last row if empty.
    std::string last = "";
    StoryHandoffs handoffs = {};
};

struct RunPlanExpectation{
    std::string expected_ids = "";
    StoryHandoff expected_stop = StoryHandoff::NONE;
};

struct RunPlanScenario{
    RunPlanInput input;
    RunPlanExpectation expectation;
};

class Test_RunPlan : public UnitTest{
public:
    explicit Test_RunPlan(RunPlanScenario scenario)
        : UnitTest("PokemonFRLG::AutoStory - run plan: " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            const size_t last = m_scenario.input.last.empty() ? story_checkpoints().size() - 1 : row_index(m_scenario.input.last);
            const StoryRunPlan plan = plan_story_run(story_checkpoints(), row_index(m_scenario.input.first), last, m_scenario.input.handoffs);
            const std::string ids = joined_ids(plan.rows);
            if (ids != m_scenario.expectation.expected_ids || plan.stop != m_scenario.expectation.expected_stop){
                return "Expected rows \"" + m_scenario.expectation.expected_ids + "\", stop " + handoff_name(m_scenario.expectation.expected_stop)
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

const std::string TODAY = "01.01 01.03 01.04 01.05 02.01 02.02 02.03 03.01 03.02 03.03 04.01 04.02 04.04 04.05 04.06 05.01 05.02 05.02b 05.02c 05.03 05.04 05.05 05.06 05.07 05.08 05.09 05.10 06.01 06.02 06.03 06.04 07.01 07.02 08.01 08.02 08.03 09.01 09.02 09.03 10.01 10.02 10.03 10.04 10.05 10.06 11.01 11.02 11.03 12.01 12.02 12.02b 12.03 13.01 13.02 13.03 13.04 14.01 14.02 14.02b 14.03 14.04 14.04b 15.01 16.01 16.02 16.03 16.03b 16.05 16.05b 16.06 16.06b 18.01 18.02 18.02b 18.03 17.02 17.03 17.04 17.04b 19.01a 19.01 19.01b 19.02 19.03 19.04 19.04b 19.04c 19.04d 19.05 20.01a 20.01 20.02 20.03 20.04 20.05";

const RunPlanScenario RUN_PLAN_CASES[] = {
    {
        .input = {.name = "handoffs off: the story as before", .first = "01.01"},
        .expectation = {.expected_ids = TODAY},
    },
    {
        .input = {.name = "handoffs off: a range", .first = "01.03", .last = "01.05"},
        .expectation = {.expected_ids = "01.03 01.04 01.05"},
    },
    {
        .input = {.name = "handoffs off: Mt. Moon to Nugget Bridge", .first = "04.05", .last = "05.05"},
        .expectation = {.expected_ids = "04.05 04.06 05.01 05.02 05.02b 05.02c 05.03 05.04 05.05"},
    },
    {
        .input = {            .name = "SID Helper: 01.01 stops on the keyboard",
            .first = "01.01",
            .handoffs = {.sid_helper = true},
        },
        .expectation = {
            .expected_ids = "",
            .expected_stop = StoryHandoff::SID_HELPER,
        },
    },
    {
        .input = {
            .name = "SID Helper: back in at 01.02",
            .first = "01.02",
            .last = "01.04",
            .handoffs = {.sid_helper = true},
        },
        .expectation = {
            .expected_ids = "01.02 01.03 01.04",
        },
    },
    {
        .input = {.name = "01.02 runs when started at, option off", .first = "01.02", .last = "01.03"},
        .expectation = {.expected_ids = "01.02 01.03"},
    },
    {
        .input = {.name = "SID Helper: a later start passes over it", .first = "01.03", .last = "01.04", .handoffs = {.sid_helper = true}},
        .expectation = {.expected_ids = "01.03 01.04"},
    },
    {
        .input = {.name = "handoffs off: an end at 01.05 still takes the rival's battle", .first = "01.04", .last = "01.05"},
        .expectation = {.expected_ids = "01.04 01.05"},
    },
    {
        .input = {
            .name = "Starter RNG: stops after 01.05's save",
            .first = "01.03",
            .handoffs = {.starter_rng = true},
        },
        .expectation = {
            .expected_ids = "01.03 01.04 01.05",
            .expected_stop = StoryHandoff::STARTER_RNG,
        },
    },
    {
        .input = {.name = "01.06 runs from its own save", .first = "01.06", .last = "02.01"},
        .expectation = {.expected_ids = "01.06 02.01"},
    },
    {
        .input = {.name = "Starter RNG: a later start doesn't stop", .first = "02.01", .last = "02.02", .handoffs = {.starter_rng = true}},
        .expectation = {.expected_ids = "02.01 02.02"},
    },
    {
        .input = {
            .name = "both handoffs, back in after SID Helper",
            .first = "01.02",
            .handoffs = {.sid_helper = true, .starter_rng = true},
        },
        .expectation = {
            .expected_ids = "01.02 01.03 01.04 01.05",
            .expected_stop = StoryHandoff::STARTER_RNG,
        },
    },
    {
        .input = {
            .name = "Gift RNG: stops after 04.02's save",
            .first = "04.01",
            .handoffs = {.gift_rng = true},
        },
        .expectation = {
            .expected_ids = "04.01 04.02",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
    },
    {
        .input = {.name = "Gift RNG off: 04.03 is passed over", .first = "04.01"},
        .expectation = {.expected_ids = "04.01 04.02 04.04 04.05 04.06 05.01 05.02 05.02b 05.02c 05.03 05.04 05.05 05.06 05.07 05.08 05.09 05.10 06.01 06.02 06.03 06.04 07.01 07.02 08.01 08.02 08.03 09.01 09.02 09.03 10.01 10.02 10.03 10.04 10.05 10.06 11.01 11.02 11.03 12.01 12.02 12.02b 12.03 13.01 13.02 13.03 13.04 14.01 14.02 14.02b 14.03 14.04 14.04b 15.01 16.01 16.02 16.03 16.03b 16.05 16.05b 16.06 16.06b 18.01 18.02 18.02b 18.03 17.02 17.03 17.04 17.04b 19.01a 19.01 19.01b 19.02 19.03 19.04 19.04b 19.04c 19.04d 19.05 20.01a 20.01 20.02 20.03 20.04 20.05"},
    },
    {
        .input = {.name = "Gift RNG off: 04.03 runs when started at", .first = "04.03"},
        .expectation = {.expected_ids = "04.03 04.04 04.05 04.06 05.01 05.02 05.02b 05.02c 05.03 05.04 05.05 05.06 05.07 05.08 05.09 05.10 06.01 06.02 06.03 06.04 07.01 07.02 08.01 08.02 08.03 09.01 09.02 09.03 10.01 10.02 10.03 10.04 10.05 10.06 11.01 11.02 11.03 12.01 12.02 12.02b 12.03 13.01 13.02 13.03 13.04 14.01 14.02 14.02b 14.03 14.04 14.04b 15.01 16.01 16.02 16.03 16.03b 16.05 16.05b 16.06 16.06b 18.01 18.02 18.02b 18.03 17.02 17.03 17.04 17.04b 19.01a 19.01 19.01b 19.02 19.03 19.04 19.04b 19.04c 19.04d 19.05 20.01a 20.01 20.02 20.03 20.04 20.05"},
    },
    {
        .input = {            .name = "Gift RNG: started at 04.03, from the save beside the salesman",
            .first = "04.03",
            .handoffs = {.gift_rng = true},
        },
        .expectation = {
            .expected_ids = "",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
    },
    {
        .input = {
            .name = "Gift RNG: started at 04.03 with Starter RNG on too",
            .first = "04.03",
            .handoffs = {.sid_helper = true, .starter_rng = true, .gift_rng = true},
        },
        .expectation = {
            .expected_ids = "",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
    },
    {
        .input = {.name = "Gift RNG: the Eevee's stop at 09.03", .first = "09.01", .handoffs = {.gift_rng = true}},
        .expectation = {
            .expected_ids = "09.01 09.02 09.03",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
    },
    {
        .input = {.name = "Gift RNG: started at 09.04, from the save facing the Eevee's ball", .first = "09.04", .handoffs = {.gift_rng = true}},
        .expectation = {
            .expected_ids = "",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
    },
    {
        .input = {.name = "Starter RNG alone: 04.03 runs when started at", .first = "04.03", .handoffs = {.starter_rng = true}},
        .expectation = {.expected_ids = "04.03 04.04 04.05 04.06 05.01 05.02 05.02b 05.02c 05.03 05.04 05.05 05.06 05.07 05.08 05.09 05.10 06.01 06.02 06.03 06.04 07.01 07.02 08.01 08.02 08.03 09.01 09.02 09.03 10.01 10.02 10.03 10.04 10.05 10.06 11.01 11.02 11.03 12.01 12.02 12.02b 12.03 13.01 13.02 13.03 13.04 14.01 14.02 14.02b 14.03 14.04 14.04b 15.01 16.01 16.02 16.03 16.03b 16.05 16.05b 16.06 16.06b 18.01 18.02 18.02b 18.03 17.02 17.03 17.04 17.04b 19.01a 19.01 19.01b 19.02 19.03 19.04 19.04b 19.04c 19.04d 19.05 20.01a 20.01 20.02 20.03 20.04 20.05"},
    },
    {
        .input = {            .name = "Starter RNG first, with both on",
            .first = "01.04",
            .handoffs = {.starter_rng = true, .gift_rng = true},
        },
        .expectation = {
            .expected_ids = "01.04 01.05",
            .expected_stop = StoryHandoff::STARTER_RNG,
        },
    },
};

void add_tests_RunPlan(UnitTestDatabase& database){
    for (const auto& item : RUN_PLAN_CASES){
        database.add<Test_RunPlan>(item);
    }
}

std::string step_names(const StoryCheckpoint& checkpoint){
    std::string text;
    for (const StoryStep& step : checkpoint.steps){
        text += (text.empty() ? "" : " ") + step.name;
    }
    return text;
}

//  With Starter RNG's handoff off, 01.05 and 01.06 run as the one 01.05
//  the story had before the split: from the lab's table to the rival's
//  battle, saving once with the starter's START menu.
UnitTestResult test_starter_rows_joined(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const StoryRunPlan plan = plan_story_run(checkpoints, row_index("01.05"), row_index("01.06"), StoryHandoffs{});
    const std::string expected_steps =
        "below_squirtle squirtle_ball choose_squirtle no_nickname rival_takes_bulbasaur above_rival_trigger rival_trigger rival_battle";
    if (plan.rows.size() != 1){
        return "Expected one checkpoint, got \"" + joined_ids(plan.rows) + "\".";
    }
    const StoryCheckpoint& row = plan.rows[0];
    if (row.id != "01.05" || row.title != "Squirtle, then the rival's battle" || step_names(row) != expected_steps){
        return "Expected 01.05 \"Squirtle, then the rival's battle\": " + expected_steps
            + "; got " + row.id + " \"" + row.title + "\": " + step_names(row);
    }
    const std::optional<StoryPose> start = story_pose(*row.start);
    if (!start || start->map != "PalletTown_ProfessorOaksLab" || !(start->tile == TilePos{6, 4}) || row.menu != StartMenu::BEFORE_POKEDEX){
        return "Expected the joined 01.05 to start at the table (6, 4) and save with the starter's START menu.";
    }
    //  Split, 01.05 saves under the ball facing it, before the starter.
    const StoryCheckpoint& to_ball = checkpoints[row_index("01.05")];
    const std::optional<StoryPose> ball = story_pose(*checkpoints[row_index("01.06")].start);
    if (to_ball.handoff != StoryHandoff::STARTER_RNG || to_ball.menu != StartMenu::BEFORE_POKEMON
        || !ball || !(ball->tile == TilePos{9, 5}) || ball->facing != Direction::NORTH
    ){
        return "Expected 01.05 to end under Squirtle's ball (9, 5) facing north, saving before the starter.";
    }
    return true;
}

//  The ways back in after SID Helper and Gift RNG load no save and start
//  where 01.01, 04.02 and 09.03 leave the player, so the rows after them follow
//  unchanged.
UnitTestResult test_after_sid_helper_row(){
    const StoryCheckpoint& sid = story_checkpoints()[row_index("01.02")];
    if (!sid.live_start || story_checkpoints()[0].handoff != StoryHandoff::SID_HELPER){
        return "Expected 01.01 to be SID Helper's handoff and 01.02 to start from the live game.";
    }
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        const bool live = checkpoint.id == "01.02" || checkpoint.id == "04.03" || checkpoint.id == "09.04" || checkpoint.id == "16.04";
        if (checkpoint.live_start != live){
            return checkpoint.id + ": only 01.02, 04.03, 09.04 and 16.04 start from the live game.";
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

//  `step`'s pose arg `key`, or nothing.
std::optional<Pokemon::GridPose> pose_arg(const StoryStep& step, std::string_view key){
    const auto it = step.args.find(key);
    if (it == step.args.end()){
        return std::nullopt;
    }
    const Pokemon::GridPose* pose = std::get_if<Pokemon::GridPose>(&it->second);
    return pose == nullptr ? std::nullopt : std::optional<Pokemon::GridPose>(*pose);
}

//  `step`'s pose arg `key` as a StoryPose, or nothing.
std::optional<StoryPose> story_pose_arg(const StoryStep& step, std::string_view key){
    const std::optional<Pokemon::GridPose> pose = pose_arg(step, key);
    return pose ? story_pose(*pose) : std::nullopt;
}

//  `step`'s MART, TEACH or GRIND plan, or nothing when it is another step.
std::optional<MartPlan> mart_of(const StoryStep& step){
    return step.action == MART_ACTION ? mart_plan_of(step.args) : std::nullopt;
}
std::optional<TeachPlan> teach_of(const StoryStep& step){
    return step.action == TEACH_ACTION ? teach_plan_of(step.args) : std::nullopt;
}
std::optional<GrindPlan> grind_of(const StoryStep& step){
    return step.action == GRIND_ACTION ? grind_plan_of(step.args) : std::nullopt;
}

//  The GRIND at `grind`'s patch, the target of the route right before it,
//  and its nurse; nothing without either.
std::optional<std::pair<Pokemon::GridPose, Pokemon::GridPose>> grind_trip(const std::vector<StoryStep>& steps, size_t grind){
    const std::optional<Pokemon::GridPose> patch = grind == 0 || grind >= steps.size() ? std::nullopt : pose_arg(steps[grind - 1], "target");
    const std::optional<Pokemon::GridPose> nurse = grind >= steps.size() ? std::nullopt : pose_arg(steps[grind], "nurse");
    if (!patch || !nurse){
        return std::nullopt;
    }
    return std::pair{*patch, *nurse};
}

//  Whether a route plans from `from` to `to` (ending facing as `to` does).
bool plans(const Pokemon::GridPose& from, const Pokemon::GridPose& to){
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

//  05.01, before Misty: Poke Balls and Potions from Cerulean's Mart, then
//  catch Rattata on Route 4's grass and heal at Cerulean's Center.
UnitTestResult test_cerulean_mart_and_capture(){
    const StoryCheckpoint& row = story_checkpoints()[row_index("05.01")];
    const std::vector<StoryStep>& steps = row.steps;
    const std::optional<StoryPose> start = row.start ? story_pose(*row.start) : std::nullopt;
    if (!start || start->map != "CeruleanCity_PokemonCenter_1F"){
        return "05.01: expected it to start in Cerulean's Center.";
    }
    const size_t mart = find_step(steps, 0, [](const StoryStep& step){
        const std::optional<MartPlan> plan = mart_of(step);
        return plan && plan->map == "CeruleanCity_Mart" && plan->lines.size() >= 2
            && plan->lines[0].item == "poke-ball" && plan->lines[1].item == "potion";
    });
    if (mart == steps.size()){
        return "05.01: expected a Cerulean Mart step buying Poke Balls, then Potions.";
    }
    const std::optional<StoryPose> clerk = mart > 0 ? story_pose_arg(steps[mart - 1], "target") : std::nullopt;
    if (!clerk || clerk->map != "CeruleanCity_Mart"){
        return "05.01: expected the Mart step right after a route to the clerk.";
    }
    const size_t capture = find_step(steps, mart, [](const StoryStep& step){
        return step.name == "catch_rattata";
    });
    const size_t heal = find_step(steps, mart, [](const StoryStep& step){
        return step.action == "heal";
    });
    if (capture == steps.size() || heal == steps.size() || capture > heal){
        return "05.01: expected catch_rattata after the Mart, then a Center heal.";
    }
    return true;
}

//  05.02c, the rival's battle as its own checkpoint: it starts from a save,
//  05.02b's on (23, 7) below his trigger, so a verify run or a retry reloads
//  right before him; his is its only battle, then back to Cerulean's Center.
UnitTestResult test_cerulean_rival_row(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t index = row_index("05.02c");
    const StoryCheckpoint& rival = checkpoints[index];
    const std::optional<StoryPose> start = rival.start ? story_pose(*rival.start) : std::nullopt;
    if (!start || rival.live_start || start->map != "CeruleanCity" || !(start->tile == TilePos{23, 7})
        || !save_card_place(start->map)
    ){
        return "05.02c: expected it to start from a save on (23, 7) in Cerulean City.";
    }
    std::string battles;
    for (const StoryStep& step : rival.steps){
        battles += step.action == "battle" ? " " + step.name : "";
    }
    if (battles != " rival_cerulean" || rival.steps.size() != 4 || rival.steps[0].name != "rival_trigger"
        || rival.steps[3].action != "heal"
    ){
        return "05.02c: expected rival_trigger, rival_cerulean, nurse route and heal; its battles:" + battles;
    }
    const StoryCheckpoint& before = checkpoints[index - 1];
    const size_t n = before.steps.size();
    const std::optional<StoryPose> end = n > 0 ? story_pose_arg(before.steps[n - 1], "target") : std::nullopt;
    if (before.id != "05.02b" || !end || end->map != start->map || !(end->tile == start->tile)){
        return "05.02b: expected it to end with the walk to (23, 7) where 05.02c starts.";
    }
    return true;
}

//  The battles in `row`, each name after a space.
std::string battle_names(const StoryCheckpoint& row){
    std::string battles;
    for (const StoryStep& step : row.steps){
        battles += step.action == "battle" ? " " + step.name : "";
    }
    return battles;
}

//  05.01, 05.02 and 05.02b between 04.06 and 05.02c; 05.02b walks from
//  Cerulean's Center into the Gym to (9, 6) beside Misty, talks and
//  battles her, heals at the Center, teaches TM03 Water Pulse over
//  Tackle, and walks to (23, 7) below the rival.
UnitTestResult test_misty_rows(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    if (checkpoints[row_index("05.01") - 1].id != "04.06" || checkpoints[row_index("05.02b") + 1].id != "05.02c"){
        return "Expected 05.01, 05.02 and 05.02b between 04.06 and 05.02c.";
    }
    const StoryCheckpoint& misty = checkpoints[row_index("05.02b")];
    const std::optional<StoryPose> start = misty.start ? story_pose(*misty.start) : std::nullopt;
    if (!start || misty.live_start || start->map != "CeruleanCity_PokemonCenter_1F" || !(start->tile == TilePos{7, 4})){
        return "05.02b: expected it to start in Cerulean's Center on (7, 4).";
    }
    const size_t talk = find_step(misty.steps, 0, [](const StoryStep& step){
        return step.action == "talk";
    });
    if (battle_names(misty) != " misty" || talk == 0 || talk + 1 >= misty.steps.size()
        || misty.steps[talk + 1].name != "misty"
    ){
        return "05.02b: expected a walk beside Misty, the talk and her battle, the only one; its battles:" + battle_names(misty);
    }
    const std::optional<StoryPose> beside = story_pose_arg(misty.steps[talk - 1], "target");
    if (!beside || !(beside->tile == TilePos{9, 6}) || beside->facing != Direction::WEST){
        return "05.02b: expected the talk from (9, 6), facing Misty (8, 6).";
    }
    const size_t teach = find_step(misty.steps, talk + 2, [](const StoryStep& step){
        const std::optional<TeachPlan> plan = teach_of(step);
        return plan && plan->item == "tm03" && plan->move == "water-pulse" && plan->member == 0 && forgets_tackle(*plan);
    });
    if (teach == misty.steps.size()){
        return "05.02b: expected TM03 Water Pulse taught to the lead in place of Tackle.";
    }
    return true;
}

//  05.02 in order: a GRIND of the lead to Lv19 on Route 4, from a route
//  to its patch, with Cerulean's nurse as its target, then the nurse.
UnitTestResult test_misty_grind_order(){
    const std::vector<StoryStep>& steps = story_checkpoints()[row_index("05.02")].steps;
    const size_t grind = find_step(steps, 0, [](const StoryStep& step){
        const std::optional<GrindPlan> grind_plan = grind_of(step);
        return grind_plan && grind_plan->member == 0 && grind_plan->level >= 19;
    });
    const auto trip = grind_trip(steps, grind);
    if (!trip){
        return "05.02: expected a GRIND of the lead to Lv19, right after a route to its patch, the nurse its target.";
    }
    const std::optional<StoryPose> patch = story_pose(trip->first), nurse = story_pose(trip->second);
    if (!patch || patch->map != "Route4" || !nurse || nurse->map != "CeruleanCity_PokemonCenter_1F"
        || !plans(trip->first, trip->second) || !plans(trip->second, trip->first)
    ){
        return "05.02: expected the GRIND on Route 4, its trips to Cerulean's nurse and back planning.";
    }
    const size_t heal = find_step(steps, grind + 1, [](const StoryStep& step){
        return step.action == "heal";
    });
    if (heal == steps.size()){
        return "05.02: expected the nurse heal after the GRIND.";
    }
    return true;
}

} // namespace


const AutoStoryCase AUTO_STORY_CASES[] = {
    {
        .input = {"01.02, 04.03 and 09.04: back in after SID Helper and Gift RNG", test_after_sid_helper_row},
        .expectation = {},
    },
    {
        .input = {"01.05 and 01.06: one checkpoint with Starter RNG off", test_starter_rows_joined},
        .expectation = {},
    },
    {
        .input = {"05.01: Poke Balls and Potions at Cerulean's Mart, then catch Rattata", test_cerulean_mart_and_capture},
        .expectation = {},
    },
    {
        .input = {"05.02c: the rival's battle alone, from a save before him", test_cerulean_rival_row},
        .expectation = {},
    },
    {
        .input = {"05.01, 05.02 and 05.02b: Cerulean Gym and Misty, then Water Pulse", test_misty_rows},
        .expectation = {},
    },
    {
        .input = {"05.02: GRIND to Lv19 on Route 4, then Cerulean's nurse", test_misty_grind_order},
        .expectation = {},
    },
};

void add_tests_AutoStoryRunPlan(UnitTestDatabase& database){
    add_tests_RunPlan(database);
    for (const auto& item : AUTO_STORY_CASES){
        database.add<Test_AutoStory>(item);
    }
}

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
