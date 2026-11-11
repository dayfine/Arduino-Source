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
#include <string_view>
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

struct AutoStoryRngLoopInput{
    std::string name;
    UnitTestResult (*body)();
};

struct AutoStoryRngLoopExpectation{
    const char* description;
};

struct AutoStoryRngLoopTestCase{
    AutoStoryRngLoopInput input;
    AutoStoryRngLoopExpectation expectation;
};

class Test_AutoStoryRngLoop : public UnitTest{
public:
    explicit Test_AutoStoryRngLoop(AutoStoryRngLoopTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStoryRngLoop - " + scenario.input.name)
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
    AutoStoryRngLoopTestCase m_scenario;
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


size_t row_index(std::string_view id){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    for (size_t c = 0; c < checkpoints.size(); c++){
        if (checkpoints[c].id == id){
            return c;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint " + std::string(id));
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

//  After a shiny, 01.06 picks up at the rival's trigger and runs on, to
//  Gift RNG's stop when it is on.
UnitTestResult test_rows_after_shiny(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t ball_row = row_index("01.05");
    struct Case{
        std::string last;
        StoryHandoffs handoffs = {};
        std::string ids;
        StoryHandoff stop = StoryHandoff::NONE;
    };
    const std::vector<Case> cases = {
        {.last = "01.05", .ids = "01.06"},
        {.last = "01.06", .ids = "01.06"},
        {.last = "02.01", .ids = "01.06,02.01"},
        {.last = "04.03", .ids = "01.06,02.01,02.02,02.03,03.01,03.02,03.03,04.01,04.02"},
        {
            .last = "04.03",
            .handoffs = {.starter_rng = true, .gift_rng = true},
            .ids = "01.06,02.01,02.02,02.03,03.01,03.02,03.03,04.01,04.02",
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


//  What the run says when Gift RNG's loop ends: a shiny goes on to 04.03;
//  otherwise the save beside the salesman is where to go on from.
UnitTestResult test_gift_messages(){
    struct Case{
        StarterRngLoopResult result;
        std::string expected;
    };
    const std::string save = "The save is still beside the salesman, facing him: start AutoStory at 04.03 with Run Gift "
        "RNG in AutoStory on to try again, or run Gift RNG (Magikarp) from it by hand with an attempt from the log, then "
        "start AutoStory at 04.03 with that option off.";
    const std::vector<Case> cases = {
        {
            .result = {.end = StarterRngLoopEnd::SHINY, .attempts_run = 1, .remaining_sids = {300}, .sid = uint16_t(300)},
            .expected = "Gift RNG: shiny Magikarp, on the planned target. 04.03 saves it.",
        },
        {
            .result = {.end = StarterRngLoopEnd::SHINY_OFF_TARGET, .attempts_run = 1, .remaining_sids = {300}, .sid = std::nullopt},
            .expected = "Gift RNG: shiny Magikarp, but not the planned target. 04.03 saves it.",
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

//  04.02 saves facing the salesman for Gift RNG; after a shiny Magikarp,
//  04.03 leaves its summary and saves there, then the run goes on.
UnitTestResult test_rows_after_magikarp(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t salesman_row = row_index("04.02");
    const StoryCheckpoint& salesman = checkpoints[salesman_row];
    const StoryCheckpoint& after = checkpoints[salesman_row + 1];
    const std::optional<StoryPose> start = after.start ? story_pose(*after.start) : std::nullopt;
    if (salesman.handoff != StoryHandoff::GIFT_RNG || !after.live_start || !start
        || start->map != "Route4_PokemonCenter_1F" || !(start->tile == TilePos{2, 3}) || start->facing != Direction::WEST
    ){
        return "Expected 04.02 to be Gift RNG's handoff and 04.03 to start from the live game at (2, 3) facing west.";
    }
    for (std::string_view last : {"04.02", "04.03"}){
        const StoryRunPlan plan = rows_after_gift_rng(checkpoints, salesman_row, row_index(last), StoryHandoffs{.gift_rng = true});
        if (joined_ids(plan) != "04.03" || first_step(plan) != LEAVE_GIFT_SUMMARY || plan.stop != StoryHandoff::NONE){
            return std::format(
                "To {}: expected 04.03 from {}; got {} from {}", last, LEAVE_GIFT_SUMMARY, joined_ids(plan), first_step(plan)
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


std::string calibration_text(const RngCalibrations& calibration){
    return std::format("{}/{}/{}", calibration.seed_offset, calibration.csf_offset, calibration.ingame_offset);
}

//  Starter RNG starts from the RNG plan's calibration: the console's seed
//  and Continue Screen Frames offsets, and Starter RNG's in-game offset.
UnitTestResult test_starter_calibration_from_options(){
    StarterRngPlanOptions options;
    RngStopCalibrationOptions& calibration = options.CALIBRATION;
    const RngCalibrations zero{.seed_offset = 0, .csf_offset = 0, .ingame_offset = 0};
    if (calibration.calibration(calibration.STARTER_INGAME) != zero){
        return "Expected no calibration by default; got " + calibration_text(calibration.calibration(calibration.STARTER_INGAME));
    }
    calibration.SEED.set(17);
    calibration.CSF.set(0.5);
    calibration.STARTER_INGAME.set(-1.2);
    const RngCalibrations expected{.seed_offset = 17 / FRLG_FRAME_DURATION, .csf_offset = 0.5, .ingame_offset = -1.2};
    const RngCalibrations got = calibration.calibration(calibration.STARTER_INGAME);
    if (got != expected){
        return std::format("Expected calibration {}; got {}", calibration_text(expected), calibration_text(got));
    }
    return true;
}

//  Steps whose one try resets once for each calibration in `resets`.
class ResettingSteps : public StarterRngLoopSteps{
public:
    explicit ResettingSteps(std::vector<RngCalibrations> resets) : m_resets(std::move(resets)) {}
    virtual WallClock now() override{ return WallClock{}; }
    virtual StarterRngResult run_attempt(const RngAttempt&, const StarterRngAttemptCall& call) override{
        for (size_t c = 0; c < m_resets.size(); c++){
            call.on_reset(StarterRngProgress{.resets = c + 1, .calibrations = m_resets[c]});
        }
        return StarterRngResult{.outcome = StarterRngOutcome::out_of_resets, .pokemon = std::nullopt};
    }

private:
    std::vector<RngCalibrations> m_resets;
};

//  Each reset's calibration is kept in the RNG plan before the loop hears
//  of the reset, so a run stopped after it keeps it.
UnitTestResult test_starter_calibration_saved(){
    StarterRngPlanOptions options;
    RngStopCalibrationOptions& calibration = options.CALIBRATION;
    ResettingSteps resetting({
        {.seed_offset = 0.5, .csf_offset = 0.25, .ingame_offset = -0.5},
        {.seed_offset = 1.0, .csf_offset = 0.5, .ingame_offset = -1.2},
    });
    CalibrationSavingSteps steps(resetting, calibration, calibration.STARTER_INGAME);
    std::vector<double> csf_at_reset;
    steps.run_attempt(RngAttempt{}, StarterRngAttemptCall{
        .index = 0, .deadline = WallClock{}, .max_resets = 2,
        .on_reset = [&](const StarterRngProgress&){ csf_at_reset.emplace_back(calibration.CSF); },
    });
    if (csf_at_reset != std::vector<double>{0.25, 0.5}){
        std::string got;
        for (double csf : csf_at_reset){
            got += std::format(" {}", csf);
        }
        return "Expected the CSF offset saved at each reset as 0.25 0.5; got" + got;
    }
    //  The seed offset is kept in ms: 1.0 frame is 16.7 ms.
    if (int64_t(calibration.SEED) != 17 || double(calibration.STARTER_INGAME) != -1.2){
        return std::format(
            "Expected seed 17 ms and Starter RNG in-game -1.2 saved; got {} ms and {}",
            int64_t(calibration.SEED), double(calibration.STARTER_INGAME)
        );
    }
    return true;
}

//  Gift RNG starts from the console's seed and CSF offsets, which Starter
//  RNG may have saved, and its own in-game offset: 0 until it saves one.
//  Saving Gift RNG's leaves Starter RNG's in-game offset as it was.
UnitTestResult test_gift_calibration(){
    StarterRngPlanOptions options;
    RngStopCalibrationOptions& calibration = options.CALIBRATION;
    calibration.save({.seed_offset = 1.0, .csf_offset = 0.5, .ingame_offset = -1.2}, calibration.STARTER_INGAME);
    const RngCalibrations gift = calibration.calibration(calibration.GIFT_INGAME);
    const RngCalibrations expected{.seed_offset = 17 / FRLG_FRAME_DURATION, .csf_offset = 0.5, .ingame_offset = 0};
    if (gift != expected){
        return std::format("Expected Gift RNG to start from {}; got {}", calibration_text(expected), calibration_text(gift));
    }
    calibration.save({.seed_offset = 1.0, .csf_offset = 0.5, .ingame_offset = 9.0}, calibration.GIFT_INGAME);
    if (double(calibration.GIFT_INGAME) != 9.0 || double(calibration.STARTER_INGAME) != -1.2){
        return std::format(
            "Expected in-game offsets Gift 9 and Starter -1.2; got {} and {}",
            double(calibration.GIFT_INGAME), double(calibration.STARTER_INGAME)
        );
    }
    return true;
}


const AutoStoryRngLoopTestCase AUTO_STORY_RNG_LOOP_CASES[] = {
    {
        .input = {.name = "shiny messages", .body = test_shiny_messages},
        .expectation = {.description = "formats notifications for shiny starter encounters"},
    },
    {
        .input = {.name = "rows after a shiny", .body = test_rows_after_shiny},
        .expectation = {.description = "plans continuation rows following shiny discovery"},
    },
    {
        .input = {.name = "Gift RNG messages", .body = test_gift_messages},
        .expectation = {.description = "formats notifications for gift rng encounters"},
    },
    {
        .input = {.name = "04.02 and 04.03: rows after a shiny Magikarp", .body = test_rows_after_magikarp},
        .expectation = {.description = "plans continuation rows following shiny magikarp"},
    },
    {
        .input = {.name = "Gift RNG outcomes in the RNG plan", .body = test_gift_outcomes},
        .expectation = {.description = "handles gift rng outcomes in plan"},
    },
    {
        .input = {.name = "Starter RNG starts from the RNG plan's calibration", .body = test_starter_calibration_from_options},
        .expectation = {.description = "initializes starter calibrations from program options"},
    },
    {
        .input = {.name = "Starter RNG saves its calibration after each reset", .body = test_starter_calibration_saved},
        .expectation = {.description = "persists calibration adjustments across reset cycles"},
    },
    {
        .input = {.name = "Gift RNG keeps its own in-game calibration", .body = test_gift_calibration},
        .expectation = {.description = "persists gift rng in-game calibrations separately"},
    },
};

} // namespace


void add_tests_AutoStoryRngLoop(UnitTestDatabase& database){
    for (const auto& scenario : AUTO_STORY_RNG_LOOP_CASES){
        database.add<Test_AutoStoryRngLoop>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
