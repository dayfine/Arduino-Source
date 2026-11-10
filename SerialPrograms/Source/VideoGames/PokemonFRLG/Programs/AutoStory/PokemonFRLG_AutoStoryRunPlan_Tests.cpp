/*  AutoStory Run Plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which rows of the story table a run takes (plan_story_run()) with the
 *  RNG programs' handoffs on and off, and the rows the handoffs split or
 *  join.
 *
 */

#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryTable.h"

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

const std::string TODAY = "00.01 00.02 00.03 00.04 00.05 00.06 00.07 00.08 00.09 00.10 00.11 00.12 00.12c 00.13";

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
        .input = {
            .name = "SID Helper: 00.01 stops on the keyboard",
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
        .expectation = {.expected_ids = "00.11 00.12 00.12c 00.13"},
    },
    {
        .input = {.name = "Gift RNG off: 00.12b runs when started at", .first = "00.12b"},
        .expectation = {.expected_ids = "00.12b 00.12c 00.13"},
    },
    {
        .input = {
            .name = "Gift RNG: started at 00.12b, from the save beside the salesman",
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
        .expectation = {.expected_ids = "00.12b 00.12c 00.13"},
    },
    {
        .input = {
            .name = "Starter RNG first, with both on",
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
    const std::optional<StoryPose> start = story_pose(Pokemon::pose_of(*row.start));
    if (!start || start->map != "PalletTown_ProfessorOaksLab" || !(start->tile == TilePos{6, 4}) || row.menu != StartMenu::BEFORE_POKEDEX){
        return "Expected the joined 00.04 to start at the table (6, 4) and save with the starter's START menu.";
    }
    //  Split, 00.04 saves under the ball facing it, before the starter.
    const StoryCheckpoint& to_ball = checkpoints[row_index("00.04")];
    const std::optional<StoryPose> ball = story_pose(Pokemon::pose_of(*checkpoints[row_index("00.04b")].start));
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
