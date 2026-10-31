/*  Hard Reset
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_HardReset_H
#define PokemonAutomation_PokemonFRLG_HardReset_H


#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonFRLG_BlindNavigation.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

static const Milliseconds INITIAL_LAUNCH_DELAY = 950ms;

//  Off by default, so the reset starts the game blind with a fixed delay.
class HandleOnlineLicenseCheckOption : public BooleanCheckBoxOption{
public:
    HandleOnlineLicenseCheckOption();
};

//  How the reset waits between starting the game and going back Home.
enum class LaunchWait{
    FixedDelay,             //  wait out the launch delay without looking at the screen
    WatchForLicenseCheck,   //  same delay, but stop if the online license check comes up
    UntilGameStarts,        //  the license check was seen before: wait for the game to start
};
LaunchWait choose_launch_wait(bool handle_online_license, bool online_license_seen);

void reset_and_perform_blind_sequence(
    ConsoleHandle& console, 
    ProControllerContext& context, 
    const Language& language,
    PokemonFRLG_RngTarget target,
    const SeedButton& seed_button,
    const BlackoutButton& extra_button,
    const RngTimings& timings,
    Milliseconds& launch_delay,
    bool safari_zone,
    uint8_t profile,
    bool handle_online_license
);

#if 0
void reset_and_detect_copyright_text(
    ConsoleHandle& console, 
    ProControllerContext& context, 
    uint8_t PROFILE = 0
);
#endif

}
}
}
#endif



