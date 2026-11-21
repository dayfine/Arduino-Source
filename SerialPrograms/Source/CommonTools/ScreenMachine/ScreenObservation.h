/*  Screen Observation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What one look at the game showed, as facts rather than pixels: the
 *  events on screen (a fade, a dialog box, a battle) and named readings. A
 *  console fills it from a frame with the game's detectors; a simulation
 *  fills it from its model and has no frame. A ScreenState that matches on
 *  an observation (ScreenState::matches_observation) therefore runs in
 *  both.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenObservation_H
#define PokemonAutomation_CommonTools_ScreenObservation_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"

namespace PokemonAutomation{


//  One look at the game.
struct ScreenObservation{
    WallClock when = WallClock::min();
    ScreenEvents events = {};
    //  Named numbers a reader found: "bag.RARE_CANDY", "facing",
    //  "heading" (degrees clockwise from north, [0, 360)).
    std::map<std::string, int, std::less<>> readings = {};
    //  The frame looked at; none in a simulation. A state that needs it
    //  can only run on a console.
    std::optional<VideoSnapshot> frame = std::nullopt;
};

//  Whether `a` and `b` both have a frame of the same size within the
//  still-screen RMSD threshold (2.0).
bool same_screen(const VideoSnapshot& a, const VideoSnapshot& b);



} // namespace PokemonAutomation
#endif
