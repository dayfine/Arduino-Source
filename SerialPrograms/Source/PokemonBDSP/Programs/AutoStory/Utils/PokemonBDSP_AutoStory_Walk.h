/*  BDSP AutoStory - Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walking that reads the screen instead of trusting a duration: hold a
 *  direction and stop on whatever the screen says happened.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Walk_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Walk_H

#include <functional>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


enum class WalkStop{
    ARRIVED,        //  `arrived` matched.
    DIALOG,         //  A dialogue box opened: an NPC or a trainer stopped us.
    TRANSITION,     //  The screen went black and came back without a battle: a door or a new area.
    BLOCKED,        //  The screen stopped changing: we are walking into a wall.
    LIMIT,          //  Walked `limit` without any of the above.
};
const char* walk_stop_name(WalkStop stop);

struct WalkResult{
    WalkStop stop;
    Milliseconds walked;
    size_t wild_battles;
};

//  Holds the left stick at (x, y) until one of the WalkStop events. Wild battles on
//  the way are run from, and walking resumes. Does not throw for any WalkStop; the
//  caller decides which ones are expected.
WalkResult walk_until(
    VideoStream& stream, ProControllerContext& context,
    double x, double y, Milliseconds limit, const std::string& label,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived = nullptr
);

//  walk_until() that requires `expected`. Fails with the screenshot otherwise.
WalkResult walk_expecting(
    VideoStream& stream, ProControllerContext& context,
    double x, double y, Milliseconds limit, const std::string& label,
    WalkStop expected,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived = nullptr
);

//  Walks toward someone to talk to: after each short stretch, presses A and stops
//  once a dialogue opens. Use it where walking into the person does not read as
//  BLOCKED, e.g. in tall grass, whose sway keeps the screen changing.
WalkResult walk_to_talk(
    VideoStream& stream, ProControllerContext& context,
    double x, double y, Milliseconds limit, const std::string& label
);

//  How much the screen changed between two frames, 0 for identical. Walking scrolls
//  the whole view: 16 to 85 per 400ms on Route 202, against about 6 for running in
//  place against trees.
double walk_screen_change(const ImageViewRGB32& before, const ImageViewRGB32& after);
constexpr double WALK_BLOCKED_CHANGE = 10.0;


}
}
}
#endif
