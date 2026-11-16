/*  AutoStory Step Lint Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Lints over FRLG's story table, without a sim: each checkpoint starts
 *  where the one before declares its end, no ANSWER follows a walk, and no
 *  new `legs` event without a reason the planner can't walk it.
 *
 */

#include <format>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryStepLint_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryUnauthoredStarts_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Without a sim: each checkpoint starts where the one before it declares
//  its end (checkpoint_end_pose()), so the stage tests, each from its own
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
        const std::optional<Pokemon::GridPose> end = Pokemon::checkpoint_end_pose(before);
        if (!end){
            if (before.id.substr(0, 2) != data.id.substr(0, 2)){
                return std::format("{} ends its stage, but declares no end for {} to start from.", before.id, data.id);
            }
            continue;
        }
        const std::optional<StoryPose> ends = story_pose(Pokemon::pose_of(*end));
        const std::optional<StoryPose> start = data.start ? story_pose(Pokemon::pose_of(*data.start)) : std::nullopt;
        const bool same = ends && start && start->map == ends->map && start->tile == ends->tile
            && (!start->facing || !ends->facing || start->facing == ends->facing);
        if (!same){
            return std::format("{} starts at {}, but {} declares its end at {}.", data.id,
                data.start ? Pokemon::to_string(Pokemon::pose_of(*data.start)) : std::string("a new game"), before.id, Pokemon::to_string(Pokemon::pose_of(*end)));
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
            if (answer.kind != StoryStepKind::ANSWER){
                continue;
            }
            if (walk.kind == StoryStepKind::ROUTE || walk.kind == StoryStepKind::LEGS){
                return std::format("{}/{}: follows the walk {}, which answers a YES/NO at its end with B.", row.id, answer.name, walk.name);
            }
        }
    }
    return true;
}

//  The `legs` events in the story before 2026-11-15, row/step. The user
//  (2026-11-15, 17.03's hand-written legs: "not ok"): every walk is a
//  planned `route` to a target tile, the gym's spinners planner moves too;
//  these are listed, not rewritten, and leave the list as they become
//  routes.
const std::set<std::string, std::less<>>& legs_before_2026_11_15(){
    static const std::set<std::string, std::less<>> LEGS{
        "03.01/forest_east", "03.01/forest_exit", "04.01/below_colton_line", "04.01/past_ben",
        "04.01/above_james_line", "04.01/save_spot", "04.05/mt_moon_1f", "04.05/b2f_south",
        "04.05/below_miguel_trigger", "04.05/below_helix_fossil", "04.06/above_city_ledge",
        "05.01/gym_west_walkway", "05.01/below_misty_platform", "05.02/gym_way_out", "05.05/below_ali_line",
        "05.05/below_timmy_line", "05.05/below_reli_line", "05.05/below_ethan_line", "05.06/west_of_dan",
        "05.06/round_dan", "05.06/west_of_nob_line", "05.06/below_haley_line", "05.07/beside_bill",
        "05.07/around_bill", "05.10/round_keigo_and_elijah", "06.01/past_the_cabins", "06.01/round_the_sailor",
        "06.02/west_of_dwayne_line", "06.02/east_of_baily_line", "06.02/below_tucker_line",
        "06.03/round_isabelle_line", "06.03/down_to_jeff_row", "07.01/round_isabelle_and_elijah",
        "07.02/round_brice", "07.02/above_caitlin_line", "07.02/below_center_door", "08.01/above_ashton_line",
        "08.01/winston_line_end", "08.01/below_martha_line", "08.01/below_allen_line", "08.01/west_of_b1f_ladder",
        "08.01/above_lenny_line", "08.01/west_of_lucas_line", "08.01/east_of_sofia_line",
        "08.01/east_of_dudley_line", "08.01/past_cooper", "08.01/above_dana_line", "08.01/above_south_mouth",
        "08.02/route10_east_side", "08.02/below_center_door", "08.03/lavender_west_edge",
        "08.03/east_of_rich_line", "08.03/west_of_glenn_column", "08.03/route8_row7_west_wall",
        "08.03/route8_row14_west_wall", "08.03/east_of_underground_door", "09.01/b2f_to_stairs",
        "09.01/b3f_arrow_maze", "09.01/beside_lift_key", "09.01/b4f_east_to_wall", "09.01/b4f_to_stairs",
        "09.01/b3f_down_to_row_20", "09.01/b3f_row_20_to_wall", "09.01/b3f_back_to_candy_row",
        "09.02/b3f_to_column_12", "09.02/b3f_to_top_corridor", "09.02/b2f_arrows_to_moon_stone",
        "09.02/beside_moon_stone", "09.02/b2f_arrows_to_lift_room", "09.02/lift_room_to_row_17",
        "09.02/row_17_to_wall", "09.02/below_lift", "09.02/below_left_guard", "09.02/below_right_guard",
        "09.02/west_of_right_guard", "09.02/up_column_17", "09.02/beside_giovanni", "10.04/tower_3f_to_stairs",
        "10.04/tower_4f_column_16", "10.04/tower_5f_row6_east", "10.05/tower_6f_below_jennifer_line",
        "10.05/above_tower_rare_candy", "10.05/above_marowak_trigger", "10.06/below_fuji_house",
        "11.02/top_of_cycling_road", "11.02/route18_west_wall", "11.03/before_east_grass",
        "11.03/to_full_restore", "11.03/to_north_area", "11.03/to_west_area", "11.03/below_gold_teeth",
        "11.03/below_secret_house_attendant", "12.01/center_northwest_corner", "12.01/below_kayden_line",
        "12.01/east_column_to_kirk", "12.01/east_column_to_wall", "12.01/top_row_to_wall", "12.01/down_to_koga",
        "12.02b/beside_warden", "13.01/statue_row", "13.01/statue_wall", "13.01/statue_tile",
        "14.01/center_northwest_corner", "14.01/below_quiz_1", "14.01/below_door_1", "14.01/to_quiz_2_machine",
        "14.01/below_quiz_2", "14.01/above_door_2", "14.01/below_quiz_3", "14.01/above_door_3",
        "14.01/below_quiz_4", "14.01/east_of_door_4", "14.01/below_quiz_5", "14.01/below_door_5",
        "14.01/below_quiz_6", "14.01/below_door_6", "14.01/below_blaine", "14.02b/above_door_6",
        "14.02b/above_door_5", "14.02b/west_of_door_4", "14.02b/below_door_3", "14.02b/below_door_2",
        "14.02b/above_door_1", "14.03/below_gym_gap", "14.03/column_1_west", "14.03/column_1_to_wall",
        "14.03/beside_inner_tree", "14.03/beside_erika", "14.04b/below_lori", "14.04b/row_5_to_wall",
        "14.04b/column_1_down_to_wall", "14.04b/onto_column_2", "14.04b/column_2_to_wall",
        "16.02/to_card_key_ball",
    };
    return LEGS;
}

//  What a new `legs` event's why says, then why the planner can't walk it
//  (taps beside a wall where a run overshoots, as 12.02b's beside_warden).
constexpr std::string_view NOT_PLANNED = "not planned: ";

//  No new `legs` event without a reason the planner can't walk it, and
//  every listed one still in the story.
UnitTestResult test_legs_need_a_reason(){
    std::set<std::string, std::less<>> listed_seen;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.kind != StoryStepKind::LEGS){
                continue;
            }
            const std::string key = row.id + "/" + step.name;
            if (legs_before_2026_11_15().contains(key)){
                listed_seen.insert(key);
                continue;
            }
            if (step.why.find(NOT_PLANNED) == std::string::npos){
                return std::format("{}: a legs event; make it a planned route to its target tile, or start its why's reason with \"{}\".", key, NOT_PLANNED);
            }
        }
    }
    for (const std::string& key : legs_before_2026_11_15()){
        if (!listed_seen.contains(key)){
            return std::format("{} is no legs event now: take it off legs_before_2026_11_15().", key);
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
        .name = "no new legs event without a reason the planner can't walk it",
        .input = {.body = test_legs_need_a_reason},
        .expectation = {.description = "every walk a planned route, but the listed legs and those whose why says \"not planned: \""},
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
