/*  AutoStory Sim Budget
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  For the FRLG story sims' tests: a wall-clock budget a run stops at, so a
 *  sim that goes round in circles fails its test with a message instead of
 *  holding CI's per-push run (#599's whole-story sims ran past 5 minutes
 *  each), and the switch that turns the slow tests on.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySimBudget_H
#define PokemonAutomation_PokemonFRLG_AutoStorySimBudget_H

#include <chrono>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  One test case's sims, at most this long: a stage of the story.
inline constexpr Seconds STAGE_BUDGET{120};

//  A run's wall-clock budget.
struct TimeBudget{
    Seconds seconds;
    std::chrono::steady_clock::time_point deadline;
};

inline TimeBudget budget_from_now(Seconds seconds){
    return TimeBudget{seconds, std::chrono::steady_clock::now() + seconds};
}

//  Throws, naming `where` (the step about to run), once `budget` is spent.
inline void check_budget(const TimeBudget& budget, std::string_view where){
    if (std::chrono::steady_clock::now() > budget.deadline){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format(
            "Time budget of {} s spent before {}: the run is stuck or too long for one test; split it.",
            budget.seconds.count(), where
        ));
    }
}

//  The environment variable that turns on the slow tests (the whole story
//  end to end). Unset, as in Fork Draft CI's per-push run, they are
//  SKIPPED.
inline constexpr std::string_view SLOW_TESTS_VARIABLE = "PA_SLOW_TESTS";

inline bool slow_tests_enabled(){
    // code-health: allow char_pointer -- std::getenv returns a const char pointer
    const char* value = std::getenv(std::string(SLOW_TESTS_VARIABLE).c_str());
    return value != nullptr && value[0] != '\0' && std::string_view(value) != "0";
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
