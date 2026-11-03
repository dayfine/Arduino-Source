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
            return std::format("Starter RNG: shiny Squirtle. The SID is {}: saved as the RNG plan's SID, so AutoStory skips SID Helper's steps next time.", *result.sid);
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


std::string gift_rng_loop_message(const StarterRngLoopResult& result){
    const std::string save = "The save is still beside the salesman, facing him: start AutoStory at 00.12b with Run Gift "
        "RNG in AutoStory on to try again, or run Gift RNG (Magikarp) from it by hand with an attempt from the log, then "
        "start AutoStory at 00.12b with that option off.";
    switch (result.end){
    case StarterRngLoopEnd::SHINY:
        return "Gift RNG: shiny Magikarp, on the planned target. 00.12b saves it.";
    case StarterRngLoopEnd::SHINY_OFF_TARGET:
        return "Gift RNG: shiny Magikarp, but not the planned target. 00.12b saves it.";
    case StarterRngLoopEnd::TARGET:
        return "Gift RNG hit the target (shiny not required). Nothing was saved after it: start AutoStory at 00.12b "
            "to save the Magikarp where it stands.";
    case StarterRngLoopEnd::OUT_OF_ATTEMPTS:
        return std::format(
            "Gift RNG: every planned attempt hit its target and none was shiny, so the RNG plan's SID doesn't fit. "
            "SIDs left: {}. {}", join_sids(result.remaining_sids), save
        );
    case StarterRngLoopEnd::MISSED_EVERY_ATTEMPT_LEFT:
        return "Gift RNG: every attempt left missed its target more often than the retries allow, or Gift RNG refused "
            "its timings (see the log). Check the RNG plan's sound setting. " + save;
    case StarterRngLoopEnd::OUT_OF_TIME:
        return std::format("Gift RNG: out of time after {} attempts. {}", result.attempts_run, save);
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown Gift RNG loop end.");
}


namespace{

//  `plan` with the rows from `first` to `last` as plan_story_run() takes
//  them after it, and the handoff they stop at.
StoryRunPlan then_rows(
    StoryRunPlan plan, const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last, const StoryHandoffs& handoffs
){
    if (last >= first){
        StoryRunPlan rest = plan_story_run(checkpoints, first, last, handoffs);
        plan.rows.insert(plan.rows.end(), rest.rows.begin(), rest.rows.end());
        plan.stop = rest.stop;
    }
    return plan;
}

}

StoryRunPlan rows_after_starter_rng(
    const std::vector<StoryCheckpoint>& checkpoints, size_t ball_row, size_t last, const StoryHandoffs& handoffs
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

    StoryRunPlan plan;
    plan.rows.emplace_back(std::move(row));
    return then_rows(std::move(plan), checkpoints, ball_row + 2, last, handoffs);
}

StoryRunPlan rows_after_gift_rng(
    const std::vector<StoryCheckpoint>& checkpoints, size_t salesman_row, size_t last, const StoryHandoffs& handoffs
){
    if (salesman_row + 1 >= checkpoints.size()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint after the Magikarp salesman.");
    }
    const StoryCheckpoint& row = checkpoints[salesman_row + 1];
    if (row.steps.empty() || row.steps.front().variant != LEAVE_GIFT_SUMMARY){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, row.id + " doesn't start by leaving the gift's summary.");
    }
    StoryRunPlan plan;
    plan.rows.emplace_back(row);
    return then_rows(std::move(plan), checkpoints, salesman_row + 2, last, handoffs);
}



}
}
}
