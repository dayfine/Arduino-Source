/*  AutoStory RNG Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  AutoStory's Starter RNG stop, run in the program: the RNG plan's
 *  attempts (PokemonFRLG_AutoStoryRngPlan.h), each run with Starter RNG's
 *  attempt loop (StarterRngAttempts) from the save under Squirtle's ball,
 *  until one comes out shiny or the plan, its retries or the time run out.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRngLoop_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRngLoop_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/Notifications/EventNotificationOption.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngDisplays.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SeedsDatabase.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngAttempts.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRngPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The Starter RNG plan, for the TID it was made for.
struct StarterRngPlanned{
    uint16_t tid = 0;
    std::vector<RngAttempt> attempts;
};

//  Starter RNG's outcome as the RNG plan counts it. Out of resets, no
//  matches and a wild shiny are misses, which the plan runs again.
RngAttemptOutcome plan_outcome(StarterRngOutcome outcome);


struct StarterRngLoopLimits{
    //  An attempt makes no more resets after this long, and counts as missed.
    Milliseconds attempt_time = Milliseconds(0);
    //  No attempt starts after this long.
    Milliseconds total_time = Milliseconds(0);
};

//  The game steps of the loop, injected so that its decisions run in tests
//  without a Switch.
class StarterRngLoopSteps{
public:
    virtual ~StarterRngLoopSteps() = default;
    virtual WallClock now() = 0;
    //  Runs Starter RNG's attempts at attempt `index` (from 0) from the save
    //  under the ball, making no reset after `deadline`.
    virtual StarterRngOutcome run_attempt(size_t index, const RngAttempt& attempt, WallClock deadline) = 0;
};

enum class StarterRngLoopEnd{
    //  The starter is shiny; the game is on its summary, not saved.
    SHINY,
    //  Shiny not required, and the target was hit. The game is where
    //  Starter RNG's calibration left it, not saved.
    TARGET,
    //  Every attempt was hit and none was shiny.
    OUT_OF_ATTEMPTS,
    //  One attempt missed more often than the retries allow.
    OUT_OF_RETRIES,
    //  The total time ran out with attempts left.
    OUT_OF_TIME,
};

struct StarterRngLoopResult{
    StarterRngLoopEnd end = StarterRngLoopEnd::OUT_OF_TIME;
    //  Attempts run, retries included.
    size_t attempts_run = 0;
    //  The SID candidates not ruled out.
    std::vector<uint16_t> remaining_sids;
    //  The SID, once a shiny leaves one candidate.
    std::optional<uint16_t> sid;
};

//  Runs `tracker`'s attempts through `steps` until the plan ends or
//  `limits.total_time` has passed. Logs an "RngPlan: result" line per
//  attempt and, when a shiny leaves one SID, an "RngPlan: confirmed" line
//  placing it among SID Helper's candidates for `tid`.
StarterRngLoopResult run_starter_rng_plan(
    Logger& logger, RngPlanTracker& tracker, StarterRngLoopSteps& steps,
    const StarterRngLoopLimits& limits, uint16_t tid, const std::vector<SidCandidate>& sid_helper_candidates
);

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


struct StarterRngConsoleSettings{
    Language language = Language::English;
    SoundSetting sound = SoundSetting::Mono;
    uint64_t max_resets = 0;
};

//  The loop's steps on the console, with Starter RNG's displays, stats and
//  calibration kept here (AutoStory doesn't show them). Squirtle, FireRed,
//  the Switch's default profile.
class ConsoleStarterRngSteps : public StarterRngLoopSteps{
public:
    ConsoleStarterRngSteps(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const StarterRngConsoleSettings& settings, const StarterRngNotifications& notifications
    );

    virtual WallClock now() override;
    virtual StarterRngOutcome run_attempt(size_t index, const RngAttempt& attempt, WallClock deadline) override;

private:
    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
    StarterRngConsoleSettings m_settings;
    StarterRngNotifications m_notifications;
    SeedsDatabase m_seeds;
    RngTargetDisplay m_target_display;
    RngFilterDisplay m_filter_display;
    RngCalibrationDisplay m_calibration_display;
};


void add_tests_AutoStoryRngLoop(UnitTestDatabase& database);



}
}
}
#endif
