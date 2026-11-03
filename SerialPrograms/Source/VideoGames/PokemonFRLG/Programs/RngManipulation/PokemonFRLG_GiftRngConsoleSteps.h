/*  Gift RNG Console Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The RNG plan loop's steps (PokemonFRLG_StarterRngLoop.h) on the console
 *  for Gift RNG's Magikarp: each try runs Gift RNG's attempts
 *  (PokemonFRLG_GiftRngAttempts.h) from the save beside Route 4's salesman.
 *  The loop counts tries in Starter RNG's outcomes; Gift RNG's map onto
 *  them one to one.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_GiftRngConsoleSteps_H
#define PokemonAutomation_PokemonFRLG_GiftRngConsoleSteps_H

#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngAttempts.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngDisplays.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SeedsDatabase.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngAttempts.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Gift RNG's outcome as the loop counts it.
StarterRngOutcome loop_outcome(GiftRngOutcome outcome);


struct GiftRngConsoleSettings{
    Language language = Language::English;
    SoundSetting sound = SoundSetting::Mono;
};

//  The loop's steps on the console, with Gift RNG's displays, stats and
//  calibration kept here (AutoStory doesn't show them). Magikarp, FireRed,
//  the Switch's default profile, no Teachy TV and no Rare Candies.
class ConsoleGiftRngSteps : public StarterRngLoopSteps{
public:
    ConsoleGiftRngSteps(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const GiftRngConsoleSettings& settings, const GiftRngNotifications& notifications
    );

    virtual WallClock now() override;
    virtual StarterRngResult run_attempt(const RngAttempt& attempt, const StarterRngAttemptCall& call) override;

private:
    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
    GiftRngConsoleSettings m_settings;
    GiftRngNotifications m_notifications;
    SeedsDatabase m_seeds;
    RngTargetDisplay m_target_display;
    RngFilterDisplay m_filter_display;
    RngCalibrationDisplay m_calibration_display;
};



}
}
}
#endif
