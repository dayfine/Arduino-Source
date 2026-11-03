/*  AutoStory RNG Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_AutoStoryRngLoop.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  "-" for none.
std::string join_sids(const std::vector<uint16_t>& sids){
    std::string text;
    for (uint16_t sid : sids){
        text += (text.empty() ? "" : ",") + std::to_string(sid);
    }
    return text.empty() ? "-" : text;
}

}

std::string starter_rng_loop_message(const StarterRngLoopResult& result){
    const std::string sids = join_sids(result.remaining_sids);
    switch (result.end){
    case StarterRngLoopEnd::SHINY:
        if (result.sid){
            return std::format("Starter RNG: shiny Squirtle. The SID is {}: enter it as the RNG plan's SID.", *result.sid);
        }
        if (result.remaining_sids.empty()){
            return "Starter RNG: shiny Squirtle, for none of the SID candidates left: the SID isn't one of SID Helper's candidates.";
        }
        return std::format("Starter RNG: shiny Squirtle. The SID is one of {}.", sids);
    case StarterRngLoopEnd::SHINY_OFF_TARGET:
        return std::format(
            "Starter RNG: shiny Squirtle, but not the planned target, so it doesn't tell the SID. SIDs left: {}.", sids
        );
    case StarterRngLoopEnd::TARGET:
        return "Starter RNG hit the target (shiny not required). Nothing was saved after it: the game is where "
            "Starter RNG's calibration left it. Play to the rival's battle and save by hand, then start AutoStory at 00.05.";
    case StarterRngLoopEnd::OUT_OF_ATTEMPTS:
        return std::format(
            "Starter RNG: every planned attempt hit its target and none was shiny. SIDs left: {}. "
            "The save is still south of the table: raise the RNG plan's max attempts and start AutoStory at 00.04 again.", sids
        );
    case StarterRngLoopEnd::MISSED_EVERY_ATTEMPT_LEFT:
        return "Starter RNG: every attempt left missed its target more often than the retries allow, or Starter RNG "
            "refused its timings (see the log). The save is still south of the table: check the RNG plan's sound setting "
            "and start AutoStory at 00.04 again.";
    case StarterRngLoopEnd::OUT_OF_TIME:
        return std::format(
            "Starter RNG: out of time after {} attempts. The save is still south of the table: start AutoStory at 00.04 again.",
            result.attempts_run
        );
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown Starter RNG loop end.");
}


std::vector<StoryCheckpoint> rows_after_starter_rng(
    const std::vector<StoryCheckpoint>& checkpoints, size_t ball_row, size_t last
){
    if (ball_row + 1 >= checkpoints.size()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint after the starter's ball.");
    }
    StoryCheckpoint row = checkpoints[ball_row + 1];
    auto taken = std::find_if(row.steps.begin(), row.steps.end(), [](const StoryStep& step){
        return step.name == "rival_takes_bulbasaur";
    });
    if (taken == row.steps.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, row.id + " has no rival_takes_bulbasaur step.");
    }
    row.steps.erase(row.steps.begin(), taken + 1);
    row.title = "after Starter RNG in AutoStory: the rival's battle";

    std::vector<StoryCheckpoint> rows{std::move(row)};
    if (last > ball_row + 1){
        StoryRunPlan rest = plan_story_run(checkpoints, ball_row + 2, last, StoryHandoffs{});
        rows.insert(rows.end(), rest.rows.begin(), rest.rows.end());
    }
    return rows;
}



}
}
}
