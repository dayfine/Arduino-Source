/*  AutoStory Step Lint Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Lints over FRLG's story table, without a sim: each checkpoint starts
 *  where the one before declares its end, no ANSWER follows a walk, and no
 *  new `legs` event without a reason the planner can't walk it.
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "PokemonFRLG_AutoStoryBattleRisk.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryStepLint_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryUnauthoredStarts_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Where `checkpoint` declares its end: the last step's target/pose_after
//  if any (matching legacy checkpoint_end_pose()).
std::optional<Pokemon::GridPose> declared_end_pose(const StoryCheckpoint& checkpoint){
    for (auto it = checkpoint.steps.rbegin(); it != checkpoint.steps.rend(); ++it){
        if (it->pose_after){
            return *it->pose_after;
        }
        if (it->action == "route" && it->args.contains("target")){
            return frlg_arg<Pokemon::GridPose>(it->args, "target");
        }
        if (it->action == "legs" || it->action == "step"){
            return std::nullopt;
        }
    }
    return checkpoint.start;
}

//  Without a sim: each checkpoint starts where the one before it declares
//  its end (declared_end_pose()), so the stage tests, each from its own
//  first row's start, chain into the whole story. A row whose end is left
//  to its legs' taps (04.01, 05.01) is checked by its stage's run; a
//  stage's last row must declare its end.
UnitTestResult test_declared_ends_chain(){
    const std::vector<StoryCheckpoint> rows = plan_story_run(story_checkpoints(), /*first=*/0, story_checkpoints().size() - 1, StoryHandoffs{}).rows;
    for (size_t c = 1; c < rows.size(); c++){
        const StoryCheckpoint& before = rows[c - 1];
        const StoryCheckpoint& data = rows[c];
        if (starts_after_unauthored_rows(data.id)){
            continue;
        }
        const std::optional<Pokemon::GridPose> end = declared_end_pose(before);
        if (!end){
            if (before.id.substr(0, 2) != data.id.substr(0, 2)){
                return std::format("{} ends its stage, but declares no end for {} to start from.", before.id, data.id);
            }
            continue;
        }
        const std::optional<StoryPose> ends = story_pose(*end);
        const std::optional<StoryPose> start = data.start ? story_pose(*data.start) : std::nullopt;
        const bool same = ends && start && start->map == ends->map && start->tile == ends->tile
            && (!start->facing || !ends->facing || start->facing == ends->facing);
        if (!same){
            return std::format("{} starts at {}, but {} declares its end at {}.", data.id,
                data.start ? format_grid_pose(*data.start) : std::string("a new game"), before.id, format_grid_pose(*end));
        }
    }
    return true;
}

//  No ANSWER step follows a ROUTE or LEGS step. A walk closes a message
//  that shows at its end with B (MessageInterrupt), which answers a YES/NO
//  NO by itself, and the ANSWER after it then finds none. Console B,
//  2026-11-14, 14.02b: Bill's scene ran as below_gym_door's warp out of
//  Cinnabar Gym landed, the walk pressed through his question, and
//  decline_bill failed with "answer_yes_no(): no YES/NO". A question that
//  shows on arrival follows a STEP onto the warp, which leaves it up.
UnitTestResult test_no_answer_after_walk(){
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (size_t c = 1; c < row.steps.size(); c++){
            const StoryStep& walk = row.steps[c - 1];
            const StoryStep& answer = row.steps[c];
            if (answer.action != "answer"){
                continue;
            }
            if (walk.action == "route" || walk.action == "legs"){
                return std::format("{}/{}: follows the walk {}, which answers a YES/NO at its end with B.", row.id, answer.name, walk.name);
            }
        }
    }
    return true;
}

//  What a `legs` event's why says, then why the planner can't walk it
//  (taps beside a wall where a run overshoots, as 12.02b's beside_warden).
constexpr std::string_view NOT_USING_PLANNER_ROUTE = "not_using_planner_route: ";

//  No `legs` event without a reason the planner can't walk it.
UnitTestResult test_legs_need_a_reason(){
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.action != "legs"){
                continue;
            }
            if (step.why.find(NOT_USING_PLANNER_ROUTE) == std::string::npos){
                return std::format(
                    "{}/{}: a legs event; make it a planned route to its target tile, or start its why's reason with \"{}\".",
                    row.id, step.name, NOT_USING_PLANNER_ROUTE
                );
            }
        }
    }
    return true;
}

//  The boss battles that, on 2026-11-16, still share their checkpoint
//  with the walk or errands before them, row/step. The user (2026-11-16,
//  19.01's Route 22 rival: "This fight also isn't its own checkpoint"):
//  a boss battle is its own checkpoint, saved right before it, so a retry
//  replays no walk. These are listed, not split yet, and leave the list as
//  they are split.
const std::set<std::string, std::less<>>& bosses_sharing_before_2026_11_16(){
    static const std::set<std::string, std::less<>> BOSSES{
        "01.06/rival_battle", "03.02/brock", "05.02b/misty", "06.01/rival_ss_anne", "09.02/giovanni",
        "16.03/rival", "16.05/giovanni", "20.01/lorelei", "20.02/bruno", "20.03/agatha", "20.04/lance",
    };
    return BOSSES;
}

//  A step that leaves the player where they stand: a GAME action with no
//  declared pose after it (a level check, a bag count, a teach), not a
//  Fly, a walk, a scene or another battle.
bool stays_in_place(const StoryStep& step){
    return !is_core_action(step.action) && !step.pose_after;
}

//  The first step of `row` before its battle `battle` that is not in
//  place, the battle's trigger (a STEP or TALK right before it) aside;
//  nothing if the battle starts its checkpoint.
std::optional<std::string_view> walk_before_battle(const StoryCheckpoint& row, size_t battle){
    for (size_t c = 0; c < battle; c++){
        const StoryStep& step = row.steps[c];
        const bool trigger = c + 1 == battle && (step.action == "step" || step.action == "talk");
        if (!trigger && !stays_in_place(step)){
            return step.name;
        }
    }
    return std::nullopt;
}

//  A boss battle (the rival, gym leaders, Giovanni, the Elite Four and the
//  Champion: the HIGH risk trainers, default_battle_risk()) is the first
//  step of its checkpoint but in-place checks and its trigger, so the
//  checkpoint's save is right before it; every listed one still shares.
UnitTestResult test_boss_battle_is_its_own_checkpoint(){
    std::set<std::string, std::less<>> listed_seen;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (size_t c = 0; c < row.steps.size(); c++){
            const StoryStep& step = row.steps[c];
            if (step.action != "battle"){
                continue;
            }
            if (default_battle_risk(frlg_arg<std::string>(step.args, "trainer")) != BattleRisk::HIGH){
                continue;
            }
            const std::optional<std::string_view> walk = walk_before_battle(row, c);
            const std::string key = std::format("{}/{}", row.id, step.name);
            if (bosses_sharing_before_2026_11_16().contains(key)){
                if (!walk){
                    return std::format("{} starts its checkpoint now: take it off bosses_sharing_before_2026_11_16().", key);
                }
                listed_seen.insert(key);
                continue;
            }
            if (walk){
                return std::format("{}: a boss battle after {}; end the checkpoint before it, so its save is right before the battle.", key, *walk);
            }
        }
    }
    for (const std::string& key : bosses_sharing_before_2026_11_16()){
        if (!listed_seen.contains(key)){
            return std::format("{} is no boss battle now: take it off bosses_sharing_before_2026_11_16().", key);
        }
    }
    return true;
}

//  The Mart steps' plans, each with its row.
struct RowMart{
    std::string_view row;
    MartPlan plan;
};
std::vector<RowMart> story_marts(){
    std::vector<RowMart> marts;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.action != MART_ACTION){
                continue;
            }
            if (const std::optional<MartPlan> plan = mart_plan_of(step.args)){
                marts.push_back({row.id, *plan});
            }
        }
    }
    return marts;
}

//  Whether the Mart on `map` sells every line of `lines`.
bool stocks_all(std::string_view map, const std::vector<MartLine>& lines){
    const std::vector<MartStockItem>& stock = mart_stock(map);
    for (const MartLine& line : lines){
        const bool sold = std::any_of(stock.begin(), stock.end(), [&](const MartStockItem& item){ return item.item == line.item; });
        if (!sold){
            return false;
        }
    }
    return true;
}

//  An errand (one checkpoint) never shops at two Marts when one Mart the
//  story visits stocks its whole list: 19.01a flew from Lavender's Mart
//  back to Viridian's for an Antidote and 2 Parlyz Heals, which Lavender
//  sells too (user, 2026-11-16).
UnitTestResult test_errand_shops_at_one_mart(){
    const std::vector<RowMart> marts = story_marts();
    std::set<std::string, std::less<>> maps;
    for (const RowMart& mart : marts){
        maps.insert(mart.plan.map);
    }
    std::map<std::string_view, std::vector<const MartPlan*>> by_row;
    for (const RowMart& mart : marts){
        by_row[mart.row].push_back(&mart.plan);
    }
    for (const auto& [row, plans] : by_row){
        std::set<std::string_view> visited;
        std::vector<MartLine> lines;
        for (const MartPlan* plan : plans){
            visited.insert(plan->map);
            lines.insert(lines.end(), plan->lines.begin(), plan->lines.end());
        }
        if (visited.size() < 2){
            continue;
        }
        for (const std::string& map : maps){
            if (stocks_all(map, lines)){
                return std::format("{}: shops at {} Marts, but {} stocks the whole list; buy it there in one step.", row, visited.size(), map);
            }
        }
    }
    return true;
}

struct StepLintInput{
    UnitTestResult (*body)();
};

struct StepLintExpectation{
    std::string_view description;
};

struct StepLintCase{
    std::string name;
    StepLintInput input;
    StepLintExpectation expectation;
};

class Test_AutoStoryStepLint : public UnitTest{
public:
    explicit Test_AutoStoryStepLint(StepLintCase scenario)
        : UnitTest("PokemonFRLG::AutoStory - Step lint: " + scenario.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.body();
    }

private:
    StepLintCase m_scenario;
};

const StepLintCase STEP_LINT_CASES[] = {
    {
        .name = "each checkpoint starts where the one before declares its end",
        .input = {.body = test_declared_ends_chain},
        .expectation = {.description = "the stage sims chain into the whole story"},
    },
    {
        .name = "an ANSWER never follows a walk",
        .input = {.body = test_no_answer_after_walk},
        .expectation = {.description = "a YES/NO after a warp follows a STEP onto it, which leaves the question up"},
    },
    {
        .name = "no legs event without a reason the planner can't walk it",
        .input = {.body = test_legs_need_a_reason},
        .expectation = {.description = "every walk a planned route, or a legs event whose why says \"not_using_planner_route: \""},
    },
    {
        .name = "a boss battle is its own checkpoint",
        .input = {.body = test_boss_battle_is_its_own_checkpoint},
        .expectation = {.description = "only in-place checks and its trigger come before it in its checkpoint, but the listed ones"},
    },
    {
        .name = "an errand shops at one Mart when one stocks its whole list",
        .input = {.body = test_errand_shops_at_one_mart},
        .expectation = {.description = "no checkpoint visits two Marts for a list one Mart sells"},
    },
};

} // namespace


void add_tests_AutoStoryStepLint(UnitTestDatabase& database){
    for (const StepLintCase& item : STEP_LINT_CASES){
        database.add<Test_AutoStoryStepLint>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
