/*  Starter RNG Console Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Starter RNG loop's steps (PokemonFRLG_StarterRngLoop.h) on the
 *  console, and the log it writes to.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_StarterRngConsoleSteps_H
#define PokemonAutomation_PokemonFRLG_StarterRngConsoleSteps_H

#include <string>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/Notifications/EventNotificationOption.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngDisplays.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SeedsDatabase.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngAttempts.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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
    //  The calibration the first try starts from.
    RngCalibrations calibration = {};
};

//  The loop's steps on the console, with Starter RNG's displays, stats and
//  calibration kept here (AutoStory doesn't show them). Each try after the
//  first starts from the calibration the last one ended with. Squirtle, FireRed, the
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



}
}
}
#endif
