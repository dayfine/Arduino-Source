/*  BDSP AutoStory - Game Settings
 *  Checks the game's options at the start of a new game, and sets the
 *  Switch's clock.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_GameSettings_H
#define PokemonAutomation_PokemonBDSP_AutoStory_GameSettings_H

#include "NintendoSwitch/Programs/DateManip/NintendoSwitch_DateManip.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            void verify_tutorial_settings(VideoStream& stream, ProControllerContext& context);

            void set_time_fast(
                SingleSwitchProgramEnvironment& env,
                ProControllerContext& context,
                const DateTime& target,
                bool preserve_date = false    // Keep current year/month/day; set only target time.
            );

        }
    }
}
#endif
