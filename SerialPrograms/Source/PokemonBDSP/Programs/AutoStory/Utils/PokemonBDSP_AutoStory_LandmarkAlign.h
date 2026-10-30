/*  BDSP AutoStory - Landmark Alignment
 *  Lines the player up with an overworld landmark seen on screen.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_LandmarkAlign_H
#define PokemonAutomation_PokemonBDSP_AutoStory_LandmarkAlign_H

#include <string>

#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

#include "PokemonBDSP_OverworldMatchers.h"

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            void align_to_overworld_landmarks(VideoStream& stream, ProControllerContext& context,
                OverworldMatchResult (*matcher)(const ImageRGB32&), const std::string& label, bool allow_story_start = false, bool horizontal_first = false,
                bool search_down_first = false,
                OverworldMatchResult (*entry_matcher)(const ImageRGB32&) = nullptr,
                bool* entry_detected = nullptr,
                double arrival_tolerance_x = 0, double arrival_tolerance_y = 0,
                double moderate_entry_confidence = 0,
                bool* rock_acquisition_timed_out = nullptr);

        }
    }
}
#endif
