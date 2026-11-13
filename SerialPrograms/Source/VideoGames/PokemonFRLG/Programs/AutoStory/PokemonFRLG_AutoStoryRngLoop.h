/*  AutoStory RNG Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  AutoStory's RNG stops, run in the program with the RNG plan loop
 *  (RngManipulation/PokemonFRLG_StarterRngLoop.h): Starter RNG from the
 *  save under Squirtle's ball, and Gift RNG from the save beside Route 4's
 *  Magikarp salesman. The plan each runs, what the run says when the loop
 *  ends, and the rows it takes after a shiny.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRngLoop_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRngLoop_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_StoryRngHandoff.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_BlindNavigation.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRngOptions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  An RNG plan (Starter RNG's or Gift RNG's), for the TID it was made for.
struct StarterRngPlanned{
    uint16_t tid = 0;
    std::vector<RngAttempt> attempts;
};

//  What the run says when the loop ends with `result`.
std::string starter_rng_loop_message(const StarterRngLoopResult& result);
//  As starter_rng_loop_message(), for Gift RNG's `gift`, which the
//  live-start row `row` (04.03, 09.04) saves after a shiny.
std::string gift_rng_loop_message(const StarterRngLoopResult& result, PokemonFRLG_RngTarget gift, std::string_view row);

//  The loop's end as the shared RNG handoff takes it (Pokemon_StoryRngHandoff.h).
//  A shiny, on the target or not, is a success: the game is on its summary and the
//  run goes on, with the rows rows_after_*_rng() give, which the caller runs. Every other
//  end is the user's turn, with the message, not a failure: nothing here is an error.
Pokemon::RngHandoffResult starter_rng_handoff_result(const StarterRngLoopResult& result);
Pokemon::RngHandoffResult gift_rng_handoff_result(
    const StarterRngLoopResult& result, PokemonFRLG_RngTarget gift, std::string_view row
);


//  `steps`, with the calibration of each reset also kept in the RNG plan's
//  calibration, with `ingame` as the stop's in-game advances offset, so the
//  next run starts from it.
class CalibrationSavingSteps : public StarterRngLoopSteps{
public:
    CalibrationSavingSteps(StarterRngLoopSteps& steps, RngStopCalibrationOptions& calibration, FloatingPointOption& ingame);

    virtual WallClock now() override;
    virtual StarterRngResult run_attempt(const RngAttempt& attempt, const StarterRngAttemptCall& call) override;

private:
    StarterRngLoopSteps& m_steps;
    RngStopCalibrationOptions& m_calibration;
    FloatingPointOption& m_ingame;
};


//  The rows a run takes after a shiny from the loop, which leaves the player
//  under Squirtle's ball with the rival's ball taken: the row after
//  `ball_row` (01.06) from its step after "rival_takes_bulbasaur", as a
//  live-start row (no start check: no save was loaded), then
//  the rows after it to `last` as plan_story_run() takes them with
//  `handoffs`, with the handoff they stop at. Throws InternalProgramError
//  for a table without that row or step.
StoryRunPlan rows_after_starter_rng(
    const std::vector<StoryCheckpoint>& checkpoints, size_t ball_row, size_t last, const StoryHandoffs& handoffs
);

//  The rows a run takes after a shiny Magikarp from the loop, which leaves
//  its summary open beside the salesman: the row after `salesman_row`
//  (04.03), which leaves the summary and saves, then the rows after it to
//  `last` as plan_story_run() takes them with `handoffs`. Throws
//  InternalProgramError for a table whose next row doesn't leave the
//  summary.
StoryRunPlan rows_after_gift_rng(
    const std::vector<StoryCheckpoint>& checkpoints, size_t salesman_row, size_t last, const StoryHandoffs& handoffs
);


//  Registers AutoStory RNG loop unit tests.
void add_tests_AutoStoryRngLoop(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
