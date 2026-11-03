/*  AutoStory RNG Loop Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the run says when an RNG loop (Starter RNG's or Gift RNG's) ends,
 *  and the rows it takes after a shiny.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngConsoleSteps.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryRngLoop.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_AutoStoryRngLoop : public UnitTest{
public:
    Test_AutoStoryRngLoop(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::AutoStoryRngLoop - " + std::move(name))
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



//  What the run says after a shiny, by what it tells of the SID.
UnitTestResult test_shiny_messages(){
    struct Case{
        StarterRngLoopResult result;
        std::string expected;
    };
    const std::vector<Case> cases = {
        {
            .result = {.end = StarterRngLoopEnd::SHINY, .attempts_run = 2, .remaining_sids = {300}, .sid = uint16_t(300)},
            .expected = "Starter RNG: shiny Squirtle. The SID is 300: saved as the RNG plan's SID, so AutoStory skips SID Helper's steps next time.",
        },
        {
            .result = {.end = StarterRngLoopEnd::SHINY, .attempts_run = 1, .remaining_sids = {100, 200}, .sid = std::nullopt},
            .expected = "Starter RNG: shiny Squirtle. The SID is one of 100,200.",
        },
        {
            .result = {.end = StarterRngLoopEnd::SHINY, .attempts_run = 1, .remaining_sids = {}, .sid = std::nullopt},
            .expected = "Starter RNG: shiny Squirtle, for none of the SID candidates left: the SID isn't one of SID Helper's candidates.",
        },
        {
            .result = {.end = StarterRngLoopEnd::SHINY_OFF_TARGET, .attempts_run = 1, .remaining_sids = {100, 200}, .sid = std::nullopt},
            .expected = "Starter RNG: shiny Squirtle, but not the planned target, so it doesn't tell the SID. "
                "SIDs left: 100,200.",
        },
    };
    for (const Case& item : cases){
        const std::string got = starter_rng_loop_message(item.result);
        if (got != item.expected){
            return "Expected \"" + item.expected + "\", got \"" + got + "\".";
        }
    }
    return true;
}


size_t row_index(const std::string& id){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    for (size_t c = 0; c < checkpoints.size(); c++){
        if (checkpoints[c].id == id){
            return c;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint " + id);
}

std::string joined_ids(const StoryRunPlan& plan){
    std::string ids;
    for (const StoryCheckpoint& row : plan.rows){
        ids += (ids.empty() ? "" : ",") + row.id;
    }
    return ids;
}

std::string first_step(const StoryRunPlan& plan){
    return plan.rows.empty() || plan.rows.front().steps.empty() ? "" : plan.rows.front().steps.front().name;
}

//  After a shiny, 00.04b picks up at the rival's trigger and runs on, to
//  Gift RNG's stop when it is on.
UnitTestResult test_rows_after_shiny(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t ball_row = row_index("00.04");
    struct Case{
        std::string last;
        StoryHandoffs handoffs = {};
        std::string ids;
        StoryHandoff stop = StoryHandoff::NONE;
    };
    const std::vector<Case> cases = {
        {.last = "00.04", .ids = "00.04b"},
        {.last = "00.04b", .ids = "00.04b"},
        {.last = "00.05", .ids = "00.04b,00.05"},
        {.last = "00.12b", .ids = "00.04b,00.05,00.06,00.07,00.08,00.09,00.10,00.11,00.12"},
        {
            .last = "00.12b",
            .handoffs = {.starter_rng = true, .gift_rng = true},
            .ids = "00.04b,00.05,00.06,00.07,00.08,00.09,00.10,00.11,00.12",
            .stop = StoryHandoff::GIFT_RNG,
        },
    };
    for (const Case& item : cases){
        const StoryRunPlan plan = rows_after_starter_rng(checkpoints, ball_row, row_index(item.last), item.handoffs);
        if (joined_ids(plan) != item.ids || first_step(plan) != "above_rival_trigger" || plan.stop != item.stop){
            return std::format(
                "To {}: expected rows {} starting at above_rival_trigger, stop {}; got {} starting at {}, stop {}",
                item.last, item.ids, int(item.stop), joined_ids(plan), first_step(plan), int(plan.stop)
            );
        }
    }
    return true;
}


//  What the run says when Gift RNG's loop ends: a shiny goes on to 00.12b;
//  otherwise the save beside the salesman is where to go on from.
UnitTestResult test_gift_messages(){
    struct Case{
        StarterRngLoopResult result;
        std::string expected;
    };
    const std::string save = "The save is still beside the salesman, facing him: run Gift RNG (Magikarp) from it with "
        "an attempt from the log, then start AutoStory at 00.12b.";
    const std::vector<Case> cases = {
        {
            .result = {.end = StarterRngLoopEnd::SHINY, .attempts_run = 1, .remaining_sids = {300}, .sid = uint16_t(300)},
            .expected = "Gift RNG: shiny Magikarp, on the planned target. 00.12b saves it.",
        },
        {
            .result = {.end = StarterRngLoopEnd::SHINY_OFF_TARGET, .attempts_run = 1, .remaining_sids = {300}, .sid = std::nullopt},
            .expected = "Gift RNG: shiny Magikarp, but not the planned target. 00.12b saves it.",
        },
        {
            .result = {.end = StarterRngLoopEnd::OUT_OF_ATTEMPTS, .attempts_run = 3, .remaining_sids = {}, .sid = std::nullopt},
            .expected = "Gift RNG: every planned attempt hit its target and none was shiny, so the RNG plan's SID doesn't fit. "
                "SIDs left: -. " + save,
        },
        {
            .result = {.end = StarterRngLoopEnd::OUT_OF_TIME, .attempts_run = 4, .remaining_sids = {300}, .sid = std::nullopt},
            .expected = "Gift RNG: out of time after 4 attempts. " + save,
        },
    };
    for (const Case& item : cases){
        const std::string got = gift_rng_loop_message(item.result);
        if (got != item.expected){
            return "Expected \"" + item.expected + "\", got \"" + got + "\".";
        }
    }
    return true;
}

//  00.12 saves facing the salesman for Gift RNG; after a shiny Magikarp,
//  00.12b leaves its summary and saves there, then the run goes on.
UnitTestResult test_rows_after_magikarp(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t salesman_row = row_index("00.12");
    const StoryCheckpoint& salesman = checkpoints[salesman_row];
    const StoryCheckpoint& after = checkpoints[salesman_row + 1];
    const std::optional<StoryPose> start = after.start ? story_pose(*after.start) : std::nullopt;
    if (salesman.handoff != StoryHandoff::GIFT_RNG || !after.live_start || !start
        || start->map != "Route4_PokemonCenter_1F" || !(start->tile == TilePos{2, 3}) || start->facing != Direction::WEST
    ){
        return "Expected 00.12 to be Gift RNG's handoff and 00.12b to start from the live game at (2, 3) facing west.";
    }
    for (const std::string& last : {"00.12", "00.12b"}){
        const StoryRunPlan plan = rows_after_gift_rng(checkpoints, salesman_row, row_index(last), StoryHandoffs{.gift_rng = true});
        if (joined_ids(plan) != "00.12b" || first_step(plan) != LEAVE_GIFT_SUMMARY || plan.stop != StoryHandoff::NONE){
            return std::format(
                "To {}: expected 00.12b from {}; got {} from {}", last, LEAVE_GIFT_SUMMARY, joined_ids(plan), first_step(plan)
            );
        }
    }
    return true;
}

//  Gift RNG's outcomes count in the RNG plan as Starter RNG's do: only a
//  shiny on the target confirms anything.
UnitTestResult test_gift_outcomes(){
    struct Case{
        GiftRngOutcome outcome;
        RngAttemptOutcome expected;
    };
    const std::vector<Case> cases = {
        {.outcome = GiftRngOutcome::shiny_on_target, .expected = RngAttemptOutcome::SHINY},
        {.outcome = GiftRngOutcome::shiny, .expected = RngAttemptOutcome::SHINY_OFF_TARGET},
        {.outcome = GiftRngOutcome::shiny_off_target, .expected = RngAttemptOutcome::SHINY_OFF_TARGET},
        {.outcome = GiftRngOutcome::target_hit, .expected = RngAttemptOutcome::HIT_NOT_SHINY},
        {.outcome = GiftRngOutcome::out_of_resets, .expected = RngAttemptOutcome::MISSED},
        {.outcome = GiftRngOutcome::no_matches, .expected = RngAttemptOutcome::MISSED},
    };
    for (const Case& item : cases){
        const RngAttemptOutcome got = plan_outcome(loop_outcome(item.outcome));
        if (got != item.expected){
            return std::format(
                "Gift RNG outcome {}: expected plan outcome {}, got {}.", int(item.outcome), int(item.expected), int(got)
            );
        }
    }
    //  The plan's targets are Starter RNG's (StarterRngPlanOptions::gift_plan()).
    if (fixed_advances_offset(PokemonFRLG_RngTarget::magikarp) != fixed_advances_offset(PokemonFRLG_RngTarget::starters)){
        return "Expected the Magikarp's advances offset to be the starters'.";
    }
    return true;
}

}


void add_tests_AutoStoryRngLoop(UnitTestDatabase& database){
    database.add<Test_AutoStoryRngLoop>("shiny messages", test_shiny_messages);
    database.add<Test_AutoStoryRngLoop>("rows after a shiny", test_rows_after_shiny);
    database.add<Test_AutoStoryRngLoop>("Gift RNG messages", test_gift_messages);
    database.add<Test_AutoStoryRngLoop>("00.12 and 00.12b: rows after a shiny Magikarp", test_rows_after_magikarp);
    database.add<Test_AutoStoryRngLoop>("Gift RNG outcomes in the RNG plan", test_gift_outcomes);
}



}
}
}
