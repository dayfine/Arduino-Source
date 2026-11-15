/*  Screen Observation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What one look at the game showed, as facts rather than pixels: the
 *  events on screen (a fade, a dialog box, a battle) and named readings. A
 *  console fills it from a frame with the game's detectors; a simulation
 *  fills it from its model and has no frame. Code that decides on an
 *  observation alone therefore runs in both.
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
    //  When the look was taken. Only a default; no reader treats min() as
    //  "not set".
    WallClock when = WallClock::min();
    ScreenEvents events = {};
    //  Named numbers the game's readers found, by names the game chooses;
    //  "heading" is degrees clockwise from north, [0, 360).
    std::map<std::string, int, std::less<>> readings = {};
    //  The frame looked at; none in a simulation. A state that needs it
    //  can only run on a console.
    std::optional<VideoSnapshot> frame = std::nullopt;
};



} // namespace PokemonAutomation
#endif
