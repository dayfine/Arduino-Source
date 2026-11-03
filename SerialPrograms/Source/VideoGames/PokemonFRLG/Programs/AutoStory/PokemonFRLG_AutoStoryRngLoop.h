/*  AutoStory RNG Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  AutoStory's Starter RNG stop, run in the program: the RNG plan's
 *  attempts (RngManipulation/PokemonFRLG_RngPlan.h), each run with Starter RNG's
 *  attempt loop (StarterRngAttempts) from the save under Squirtle's ball,
 *  until one comes out shiny or the plan, its retries or the time run out.
 *  Its "RngPlan: state" and "RngPlan: result" lines are listed with the
 *  plan's in RngManipulation/PokemonFRLG_RngPlanLog.h.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRngLoop_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRngLoop_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/Notifications/EventNotificationOption.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngCalibration.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngDisplays.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTracker.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SeedsDatabase.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngAttempts.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The Starter RNG plan, for the TID it was made for.
struct StarterRngPlanned{
    uint16_t tid = 0;
    std::vector<RngAttempt> attempts;
};

//  Starter RNG's outcome as the RNG plan counts it. Only a shiny on the
//  target is SHINY; one off it, or not known to be on it, is
//  SHINY_OFF_TARGET. Out of resets, no matches and a wild shiny are misses,
//  which the plan runs again.
RngAttemptOutcome plan_outcome(StarterRngOutcome outcome);
//  As the result line names it: "shiny_on_target", "target_hit",
//  "out_of_resets"... (The loop says "attempt_deadline" instead of
//  "out_of_resets" once the try's deadline has passed.)
std::string starter_rng_outcome_name(StarterRngOutcome outcome);


//  Where the loop is, for an "RngPlan: state" line.
struct RngLoopState{
    //  From 1, of `attempts`.
    size_t attempt = 1;
    size_t attempts = 0;
    //  From 1, of `tries`.
    size_t try_number = 1;
    size_t tries = 0;
    uint64_t resets = 0;
    uint64_t max_resets = 0;
    uint16_t seed = 0;
    uint64_t advances = 0;
    //  The SIDs the attempt is shiny for, and those ruled out so far.
    std::vector<uint16_t> sids;
    std::vector<uint16_t> ruled_out;
    Milliseconds attempt_time = Milliseconds(0);
    Milliseconds attempt_cap = Milliseconds(0);
    Milliseconds total_time = Milliseconds(0);
    Milliseconds total_cap = Milliseconds(0);
    //  The calibration of the last reset, in frames; none before the first.
    std::optional<RngCalibrations> calibration;
};
std::string rng_log_state(const RngLoopState& state);


struct StarterRngLoopLimits{
    //  An attempt makes no more resets after this long, and counts as missed.
    Milliseconds attempt_time = Milliseconds(0);
    //  No attempt starts after this long.
    Milliseconds total_time = Milliseconds(0);
    //  Resets per try of an attempt.
    uint64_t max_resets = 0;
    //  The tracker's retries of a missed attempt, for the state lines.
    size_t max_retries = 0;
};

//  Starter RNG refused an attempt's timings (check_timings()). The loop
//  records the attempt as unusable and moves on; any other error, a
//  UserSetupError included, stops the loop.
class StarterRngTimingsRefused : public Exception{
public:
    explicit StarterRngTimingsRefused(std::string message) : m_message(std::move(message)) {}
    virtual const char* name() const override{ return "StarterRngTimingsRefused"; }
    virtual std::string message() const override{ return m_message; }

private:
    std::string m_message;
};

//  One try of an attempt, as the loop asks the steps for it.
struct StarterRngAttemptCall{
    //  From 0.
    size_t index = 0;
    //  No reset after this.
    WallClock deadline;
    uint64_t max_resets = 0;
    //  To call after each reset.
    std::function<void(const StarterRngProgress&)> on_reset;
};

//  The game steps of the loop, injected so that its decisions run in tests
//  without a Switch.
class StarterRngLoopSteps{
public:
    virtual ~StarterRngLoopSteps() = default;
    virtual WallClock now() = 0;
    //  Runs Starter RNG's attempts at `attempt`'s target from the save under
    //  the ball, as `call` says. Throws StarterRngTimingsRefused when
    //  Starter RNG refuses the attempt's timings; the loop logs it and moves on.
    virtual StarterRngResult run_attempt(const RngAttempt& attempt, const StarterRngAttemptCall& call) = 0;
};

enum class StarterRngLoopEnd{
    //  The starter is shiny and the target; the game is on its summary, not saved.
    SHINY,
    //  The starter is shiny but not the target (or not known to be), so it
    //  doesn't tell the SID; the game is on its summary, not saved.
    SHINY_OFF_TARGET,
    //  Shiny not required, and the target was hit. The game is where
    //  Starter RNG's calibration left it, not saved.
    TARGET,
    //  Every attempt was hit and none was shiny.
    OUT_OF_ATTEMPTS,
    //  The last attempt still shiny for a SID left missed more often than
    //  the retries allow, or couldn't run.
    MISSED_EVERY_ATTEMPT_LEFT,
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
//  `limits.total_time` has passed. Logs an "RngPlan: state" line at the
//  start of each try and after each reset, an "RngPlan: result" line after
//  it and, when a shiny leaves one SID, an "RngPlan: confirmed" line placing
//  it among SID Helper's candidates for `tid`.
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


//  The console's log, with the loop's state and result lines also on the
//  stream's overlay log.
class RngPlanLogger : public Logger{
public:
    explicit RngPlanLogger(ConsoleHandle& console);
    virtual void log(const std::string& msg, Color color = Color()) override;

private:
    ConsoleHandle& m_console;
};


struct StarterRngConsoleSettings{
    Language language = Language::English;
    SoundSetting sound = SoundSetting::Mono;
};

//  The loop's steps on the console, with Starter RNG's displays, stats and
//  calibration kept here (AutoStory doesn't show them). Each try starts
//  from the calibration the last one ended with. Squirtle, FireRed, the
//  Switch's default profile.
class ConsoleStarterRngSteps : public StarterRngLoopSteps{
public:
    ConsoleStarterRngSteps(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const StarterRngConsoleSettings& settings, const StarterRngNotifications& notifications
    );

    virtual WallClock now() override;
    virtual StarterRngResult run_attempt(const RngAttempt& attempt, const StarterRngAttemptCall& call) override;

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
