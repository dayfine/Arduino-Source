/*  AutoStory Run Plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which rows of the story table a run takes (plan_story_run()) with the
 *  RNG programs' handoffs on and off, the rows the handoffs split or
 *  join, Misty in Cerulean Gym, and what 00.15 runs before the rival.
 *
 */

#include <algorithm>
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
#include "PokemonFRLG_AutoStoryTeach.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

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

const std::string TODAY = "00.01 00.02 00.03 00.04 00.05 00.06 00.07 00.08 00.09 00.10 00.11 00.12 00.12c 00.13 00.14 00.14b 00.14c 00.15 00.15b 00.16 00.17 00.18 00.19 00.20 00.21 00.22 00.23 00.24 00.25 00.26 00.27 00.28 00.29 00.30";

const RunPlanScenario RUN_PLAN_CASES[] = {
    {
        .input = {.name = "handoffs off: the story as before", .first = "00.01"},
        .expectation = {.expected_ids = TODAY},
    },
    {
        .input = {.name = "handoffs off: a range", .first = "00.02", .last = "00.04"},
        .expectation = {.expected_ids = "00.02 00.03 00.04"},
    },
    {
        .input = {.name = "handoffs off: Mt. Moon to Nugget Bridge", .first = "00.13", .last = "00.16"},
        .expectation = {.expected_ids = "00.13 00.14 00.14b 00.14c 00.15 00.15b 00.16"},
    },
    {
        .input = {            .name = "SID Helper: 00.01 stops on the keyboard",
            .first = "00.01",
            .handoffs = {.sid_helper = true},
        },
        .expectation = {
            .expected_ids = "",
            .expected_stop = StoryHandoff::SID_HELPER,
        },
    },
    {
        .input = {
            .name = "SID Helper: back in at 00.01b",
            .first = "00.01b",
            .last = "00.03",
            .handoffs = {.sid_helper = true},
        },
        .expectation = {
            .expected_ids = "00.01b 00.02 00.03",
        },
    },
    {
        .input = {.name = "00.01b runs when started at, option off", .first = "00.01b", .last = "00.02"},
        .expectation = {.expected_ids = "00.01b 00.02"},
    },
    {
        .input = {.name = "SID Helper: a later start passes over it", .first = "00.02", .last = "00.03", .handoffs = {.sid_helper = true}},
        .expectation = {.expected_ids = "00.02 00.03"},
    },
    {
        .input = {.name = "handoffs off: an end at 00.04 still takes the rival's battle", .first = "00.03", .last = "00.04"},
        .expectation = {.expected_ids = "00.03 00.04"},
    },
    {
        .input = {
            .name = "Starter RNG: stops after 00.04's save",
            .first = "00.02",
            .handoffs = {.starter_rng = true},
        },
        .expectation = {
            .expected_ids = "00.02 00.03 00.04",
            .expected_stop = StoryHandoff::STARTER_RNG,
        },
    },
    {
        .input = {.name = "00.04b runs from its own save", .first = "00.04b", .last = "00.05"},
        .expectation = {.expected_ids = "00.04b 00.05"},
    },
    {
        .input = {.name = "Starter RNG: a later start doesn't stop", .first = "00.05", .last = "00.06", .handoffs = {.starter_rng = true}},
        .expectation = {.expected_ids = "00.05 00.06"},
    },
    {
        .input = {
            .name = "both handoffs, back in after SID Helper",
            .first = "00.01b",
            .handoffs = {.sid_helper = true, .starter_rng = true},
        },
        .expectation = {
            .expected_ids = "00.01b 00.02 00.03 00.04",
            .expected_stop = StoryHandoff::STARTER_RNG,
        },
    },
    {
        .input = {
            .name = "Gift RNG: stops after 00.12's save",
            .first = "00.11",
            .handoffs = {.gift_rng = true},
        },
        .expectation = {
            .expected_ids = "00.11 00.12",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
    },
    {
        .input = {.name = "Gift RNG off: 00.12b is passed over", .first = "00.11"},
        .expectation = {.expected_ids = "00.11 00.12 00.12c 00.13 00.14 00.14b 00.14c 00.15 00.15b 00.16 00.17 00.18 00.19 00.20 00.21 00.22 00.23 00.24 00.25 00.26 00.27 00.28 00.29 00.30"},
    },
    {
        .input = {.name = "Gift RNG off: 00.12b runs when started at", .first = "00.12b"},
        .expectation = {.expected_ids = "00.12b 00.12c 00.13 00.14 00.14b 00.14c 00.15 00.15b 00.16 00.17 00.18 00.19 00.20 00.21 00.22 00.23 00.24 00.25 00.26 00.27 00.28 00.29 00.30"},
    },
    {
        .input = {            .name = "Gift RNG: started at 00.12b, from the save beside the salesman",
            .first = "00.12b",
            .handoffs = {.gift_rng = true},
        },
        .expectation = {
            .expected_ids = "",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
    },
    {
        .input = {
            .name = "Gift RNG: started at 00.12b with Starter RNG on too",
            .first = "00.12b",
            .handoffs = {.sid_helper = true, .starter_rng = true, .gift_rng = true},
        },
        .expectation = {
            .expected_ids = "",
            .expected_stop = StoryHandoff::GIFT_RNG,
        },
    },
    {
        .input = {.name = "Starter RNG alone: 00.12b runs when started at", .first = "00.12b", .handoffs = {.starter_rng = true}},
        .expectation = {.expected_ids = "00.12b 00.12c 00.13 00.14 00.14b 00.14c 00.15 00.15b 00.16 00.17 00.18 00.19 00.20 00.21 00.22 00.23 00.24 00.25 00.26 00.27 00.28 00.29 00.30"},
    },
    {
        .input = {            .name = "Starter RNG first, with both on",
            .first = "00.03",
            .handoffs = {.starter_rng = true, .gift_rng = true},
        },
        .expectation = {
            .expected_ids = "00.03 00.04",
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

//  00.15, before the rival, from the Center: first TM03 Water Pulse over
//  Tail Whip if the lead doesn't know it, so the sim plays it in the
//  GRIND; Potions from Cerulean's Mart (a loaded save's bag may hold too
//  few for the wild battles a GRIND fights), and at its money floor 2 or
//  more Awakenings for the rival's Sleep Powder; then a GRIND on Route 4's
//  grass east of Mt. Moon until the sim beats the rival 00.15b fights 50%
//  or more, healing at Cerulean's Center; its trip there plans both ways.
UnitTestResult test_cerulean_mart_and_grind(){
    const StoryCheckpoint& row = story_checkpoints()[row_index("00.15")];
    const std::vector<StoryStep>& steps = row.steps;
    const std::optional<StoryPose> start = row.start ? story_pose(*row.start) : std::nullopt;
    if (!start || start->map != "CeruleanCity_PokemonCenter_1F"){
        return "00.15: expected it to start in Cerulean's Center.";
    }
    const std::optional<TeachPlan> teach = steps.empty() ? std::nullopt : parse_teach_variant(steps[0].variant);
    if (!teach || teach->item != "tm03" || teach->move != "water-pulse" || teach->member != 0 || teach->forget_move != "tail-whip"){
        return "00.15: expected it to start with TM03 Water Pulse over the lead's Tail Whip.";
    }
    const size_t mart = find_step(steps, 0, [](const StoryStep& step){
        const std::optional<MartPlan> plan = parse_mart_variant(step.variant);
        return plan && plan->map == "CeruleanCity_Mart" && !plan->lines.empty() && plan->lines[0].item == "potion";
    });
    const std::optional<std::string_view> rival = story_trainer("CeruleanCity", "rival_cerulean");
    const size_t grind = find_step(steps, mart, [&](const StoryStep& step){
        const std::optional<GrindPlan> plan = parse_grind_variant(step.variant);
        return plan && plan->member == 0 && rival && plan->trainer == *rival && plan->min_chance == MIN_WIN_CHANCE
            && plan->level > 25;
    });
    if (mart == steps.size() || grind == steps.size()){
        return "00.15: expected a Cerulean Mart step buying Potions, then a GRIND of the lead until the rival is 50%, capped above Lv25.";
    }
    const MartPlan plan = *parse_mart_variant(steps[mart].variant);
    const std::optional<std::vector<MartBuy>> buys = plan.money_floor
        ? plan_mart_buys(mart_stock(plan.map), plan, *plan.money_floor)
        : std::nullopt;
    const bool awakenings = buys && std::ranges::any_of(*buys, [](const MartBuy& buy){
        return buy.item == "awakening" && buy.quantity >= 2;
    });
    if (!awakenings){
        return "00.15: expected the Mart step to buy 2 or more Awakenings at its money floor.";
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
//  right before him; and his is its only battle. First Water Pulse over
//  Tail Whip if the lead doesn't know it: console B's save (2026-11-03) is
//  past the TEACH that failed there, which does nothing once it is known.
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
    if (battles != " rival_cerulean" || rival.steps.size() != 3 || rival.steps[1].name != "rival_trigger"){
        return "00.15b: expected a TEACH, the step onto his trigger and his battle; its battles:" + battles;
    }
    const std::optional<TeachPlan> teach = parse_teach_variant(rival.steps[0].variant);
    if (rival.steps[0].kind != StoryStepKind::GAME || !teach || teach->item != "tm03" || teach->move != "water-pulse"
        || teach->member != 0 || teach->forget_move != "tail-whip"
    ){
        return "00.15b: expected it to start with TM03 Water Pulse over the lead's Tail Whip.";
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

//  The battles in `row`, each name after a space.
std::string battle_names(const StoryCheckpoint& row){
    std::string battles;
    for (const StoryStep& step : row.steps){
        battles += step.kind == StoryStepKind::BATTLE ? " " + step.name : "";
    }
    return battles;
}

//  00.14b, before Misty: Potions at Cerulean's Mart, then into the Gym to
//  a save on (9, 7), below her platform; no battle. 00.14c, Misty's battle
//  as its own checkpoint: it starts from that save, talks to her from
//  beside her, (9, 6), and hers is its only battle; then TM03 Water Pulse
//  in place of Tail Whip.
UnitTestResult test_misty_rows(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const StoryCheckpoint& gym = checkpoints[row_index("00.14b")];
    const StoryCheckpoint& misty = checkpoints[row_index("00.14c")];
    if (checkpoints[row_index("00.14b") - 1].id != "00.14" || checkpoints[row_index("00.14c") + 1].id != "00.15"){
        return "Expected 00.14b and 00.14c between 00.14 and 00.15.";
    }
    const std::vector<StoryStep>& steps = gym.steps;
    const size_t mart = find_step(steps, 0, [](const StoryStep& step){
        const std::optional<MartPlan> plan = parse_mart_variant(step.variant);
        return plan && plan->map == "CeruleanCity_Mart" && !plan->lines.empty() && plan->lines[0].item == "potion";
    });
    if (mart == steps.size() || !battle_names(gym).empty()){
        return "00.14b: expected a Cerulean Mart step buying Potions and no battle; its battles:" + battle_names(gym);
    }
    const std::optional<StoryPose> start = misty.start ? story_pose(*misty.start) : std::nullopt;
    if (!start || misty.live_start || start->map != "CeruleanCity_Gym" || !(start->tile == TilePos{9, 7})
        || save_card_place(start->map).empty()
    ){
        return "00.14c: expected it to start from a save on (9, 7) in Cerulean Gym.";
    }
    if (battle_names(misty) != " misty" || misty.steps.size() < 3 || misty.steps[1].kind != StoryStepKind::TALK
        || misty.steps[2].name != "misty"
    ){
        return "00.14c: expected a walk beside Misty, the talk and her battle, the only one; its battles:" + battle_names(misty);
    }
    const std::optional<StoryPose> beside = misty.steps[0].target ? story_pose(*misty.steps[0].target) : std::nullopt;
    if (!beside || !(beside->tile == TilePos{9, 6}) || beside->facing != Direction::WEST){
        return "00.14c: expected the talk from (9, 6), facing Misty (8, 6).";
    }
    const size_t teach = find_step(misty.steps, 0, [](const StoryStep& step){
        const std::optional<TeachPlan> plan = parse_teach_variant(step.variant);
        return plan && plan->item == "tm03" && plan->move == "water-pulse" && plan->member == 0 && plan->forget_move == "tail-whip";
    });
    if (teach == misty.steps.size()){
        return "00.14c: expected TM03 Water Pulse taught to the lead in place of Tail Whip.";
    }
    return true;
}

//  00.14b in order (Misty lost to at Lv20, console B, run m51): the Mart
//  first, Potions then Super Potions, the Potions enough for the GRIND's
//  wild battles at the table's money floor and at least one Super Potion
//  after them; a GRIND of the lead to Lv23 on Route 4, from a route to its
//  patch, with Cerulean's nurse as its target; the nurse; then the walk
//  into the Gym, ending on (9, 7).
UnitTestResult test_misty_grind_order(){
    const std::vector<StoryStep>& steps = story_checkpoints()[row_index("00.14b")].steps;
    std::optional<MartPlan> plan;
    const size_t mart = find_step(steps, 0, [&](const StoryStep& step){
        plan = parse_mart_variant(step.variant);
        return plan && plan->map == "CeruleanCity_Mart";
    });
    if (mart == steps.size() || plan->lines.size() < 2 || plan->lines[0].item != "potion" || plan->lines[1].item != "super-potion"
        || !plan->money_floor
    ){
        return "00.14b: expected a Cerulean Mart step buying Potions, then Super Potions, with a money floor.";
    }
    const std::optional<std::vector<MartBuy>> buys = plan_mart_buys(mart_stock(plan->map), *plan, *plan->money_floor);
    if (!buys || buys->size() < 2 || (*buys)[0].quantity < 2 || (*buys)[1].item != "super-potion"){
        return "00.14b: expected the money floor to buy 2 or more Potions and a Super Potion.";
    }
    const size_t grind = find_step(steps, mart, [](const StoryStep& step){
        const std::optional<GrindPlan> grind_plan = parse_grind_variant(step.variant);
        return grind_plan && grind_plan->member == 0 && grind_plan->level >= 23;
    });
    if (grind == steps.size() || !steps[grind - 1].target || !steps[grind].target){
        return "00.14b: expected a GRIND of the lead to Lv23 after the Mart, right after a route to its patch, the nurse its target.";
    }
    const std::optional<StoryPose> patch = story_pose(*steps[grind - 1].target);
    const std::optional<StoryPose> nurse = story_pose(*steps[grind].target);
    if (!patch || patch->map != "Route4" || !nurse || nurse->map != "CeruleanCity_PokemonCenter_1F"
        || !plans(*steps[grind - 1].target, *steps[grind].target) || !plans(*steps[grind].target, *steps[grind - 1].target)
    ){
        return "00.14b: expected the GRIND on Route 4, its trips to Cerulean's nurse and back planning.";
    }
    const size_t heal = find_step(steps, grind + 1, [](const StoryStep& step){
        return step.name == "heal_cerulean";
    });
    const size_t gym = find_step(steps, grind + 1, [](const StoryStep& step){
        const std::optional<StoryPose> pose = step.target ? story_pose(*step.target) : std::nullopt;
        return pose && pose->map == "CeruleanCity_Gym";
    });
    if (heal == steps.size() || gym == steps.size() || heal > gym){
        return "00.14b: expected the nurse after the GRIND, then the walk into the Gym.";
    }
    StoryPose end;
    for (const StoryStep& step : steps){
        const std::optional<StoryPose> pose = step.target ? story_pose(*step.target) : std::nullopt;
        end = pose ? *pose : end;
    }
    if (end.map != "CeruleanCity_Gym"){
        return "00.14b: expected the row to end in Cerulean Gym.";
    }
    return true;
}

} // namespace


const AutoStoryCase AUTO_STORY_CASES[] = {
    {
        .input = {"00.01b and 00.12b: back in after SID Helper and Gift RNG", test_after_sid_helper_row},
        .expectation = {},
    },
    {
        .input = {"00.04 and 00.04b: one checkpoint with Starter RNG off", test_starter_rows_joined},
        .expectation = {},
    },
    {
        .input = {"00.15: Potions at Cerulean's Mart and a GRIND before the rival", test_cerulean_mart_and_grind},
        .expectation = {},
    },
    {
        .input = {"00.15b: the rival's battle alone, from a save before him", test_cerulean_rival_row},
        .expectation = {},
    },
    {
        .input = {"00.14b and 00.14c: Cerulean Gym, then Misty alone from a save before her", test_misty_rows},
        .expectation = {},
    },
    {
        .input = {"00.14b: Potions and Super Potions, a GRIND to Lv23, the nurse, then the Gym", test_misty_grind_order},
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
