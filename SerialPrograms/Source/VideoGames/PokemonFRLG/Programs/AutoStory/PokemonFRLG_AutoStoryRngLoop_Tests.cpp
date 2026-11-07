/*  AutoStory RNG Loop Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the run says after a shiny, and the rows it takes then.
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


//  After a shiny, 00.04b picks up at the rival's trigger and runs on.
UnitTestResult test_rows_after_shiny(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    size_t ball_row = 0;
    size_t parcel_row = 0;
    for (size_t c = 0; c < checkpoints.size(); c++){
        if (checkpoints[c].id == "00.04"){
            ball_row = c;
        }
        if (checkpoints[c].id == "00.05"){
            parcel_row = c;
        }
    }
    struct Case{
        size_t last;
        std::string ids;
    };
    const std::vector<Case> cases = {
        {.last = ball_row, .ids = "00.04b"},
        {.last = ball_row + 1, .ids = "00.04b"},
        {.last = parcel_row, .ids = "00.04b,00.05"},
    };
    for (const Case& item : cases){
        const std::vector<StoryCheckpoint> rows = rows_after_starter_rng(checkpoints, ball_row, item.last);
        std::string ids;
        for (const StoryCheckpoint& row : rows){
            ids += (ids.empty() ? "" : ",") + row.id;
        }
        const std::string first_step = rows.front().steps.empty() ? "" : rows.front().steps.front().name;
        if (ids != item.ids || first_step != "above_rival_trigger"){
            return std::format(
                "Expected rows {} starting at above_rival_trigger; got {} starting at {}", item.ids, ids, first_step
            );
        }
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
        .input = {.name = "Starter RNG starts from the RNG plan's calibration", .body = test_starter_calibration_from_options},
        .expectation = {.description = "initializes starter calibrations from program options"},
    },
    {
        .input = {.name = "Starter RNG saves its calibration after each reset", .body = test_starter_calibration_saved},
        .expectation = {.description = "persists calibration adjustments across reset cycles"},
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
