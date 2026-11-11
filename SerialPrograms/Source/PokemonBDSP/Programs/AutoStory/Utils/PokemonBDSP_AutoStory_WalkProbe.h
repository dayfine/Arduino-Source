/*  BDSP AutoStory - Walk probe
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A straight walk timed finely, to check a map against the game.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WalkProbe_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WalkProbe_H

#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Controllers/Joystick.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  One straight walk timed finely, to check a map against the game: holds the
//  stick at `stick` and samples the screen every 100ms until the view has been
//  still for 800ms or `limit` runs out. Times count from the start of the hold.
//  A sample moved if it changed by at least `moving_change`: still is 4 to 7,
//  walking 16 to 85. Handles no battles or dialogs: use it on open ground,
//  away from tall grass.
struct WalkProbe{
    Milliseconds first_move{-1};    //  the first sample that moved; -1 if none did
    Milliseconds last_move{-1};     //  the last sample that moved; -1 if none did
    struct Sample{
        Milliseconds at;
        double change;              //  walk_screen_change() since the previous sample
    };
    std::vector<Sample> samples;
};
//  Probes walking movement and records timestamped screen change samples.
WalkProbe probe_walk(
    VideoStream& stream, ProControllerContext& context,
    JoystickPosition stick, Milliseconds limit,
    double moving_change, const std::string& label
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
