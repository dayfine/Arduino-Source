/*  AutoStory RNG Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  AutoStory's Starter RNG stop, run in the program with the Starter RNG
 *  loop (RngManipulation/PokemonFRLG_StarterRngLoop.h) from the save under
 *  Squirtle's ball: the plan it runs, what the run says when the loop
 *  ends, and the rows it takes after a shiny.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRngLoop_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRngLoop_H

#include <cstdint>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRngOptions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The Starter RNG plan, for the TID it was made for.
struct StarterRngPlanned{
    uint16_t tid = 0;
    std::vector<RngAttempt> attempts;
};

//  What the run says when the loop ends with `result`.
std::string starter_rng_loop_message(const StarterRngLoopResult& result);


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
//  `ball_row` (00.04b) from its step after "rival_takes_bulbasaur", then
//  the rows after it to `last` as plan_story_run() takes them. Throws
//  InternalProgramError for a table without that row or step.
std::vector<StoryCheckpoint> rows_after_starter_rng(
    const std::vector<StoryCheckpoint>& checkpoints, size_t ball_row, size_t last
);


//  Registers AutoStory RNG loop unit tests.
void add_tests_AutoStoryRngLoop(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
