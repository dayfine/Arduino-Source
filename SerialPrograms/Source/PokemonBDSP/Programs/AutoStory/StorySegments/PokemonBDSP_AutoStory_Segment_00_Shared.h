/*  BDSP AutoStory - Segment 00 Shared
 *  Helpers used by more than one Segment 00 checkpoint.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Segment00_Shared_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Segment00_Shared_H

#include <string>

#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // Watch before movement starts, release on the fade, and hold the
            // following recorded wait until the destination is visible again.
            void recorded_move_until_transition(
                VideoStream& stream, ProControllerContext& context,
                double x, double y, Milliseconds duration, const std::string& label
            );

        }
    }
}
#endif
