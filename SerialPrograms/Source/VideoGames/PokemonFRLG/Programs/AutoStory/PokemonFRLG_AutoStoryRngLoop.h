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


//  The rows a run takes after a shiny from the loop, which leaves the player
//  under Squirtle's ball with the rival's ball taken: the row after
//  `ball_row` (00.04b) from its step after "rival_takes_bulbasaur", then
//  the rows after it to `last` as plan_story_run() takes them. Throws
//  InternalProgramError for a table without that row or step.
std::vector<StoryCheckpoint> rows_after_starter_rng(
    const std::vector<StoryCheckpoint>& checkpoints, size_t ball_row, size_t last
);


void add_tests_AutoStoryRngLoop(UnitTestDatabase& database);



}
}
}
#endif
