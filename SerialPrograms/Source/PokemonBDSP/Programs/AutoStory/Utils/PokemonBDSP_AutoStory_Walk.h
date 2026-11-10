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
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "Controllers/Joystick.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Holds the left stick at `stick` until one of the WalkStop events. Wild battles on
//  the way are run from, and walking resumes. Does not throw for any WalkStop; the
//  caller decides which ones are expected.
WalkResult walk_until(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label,
    const WalkArrived& arrived = nullptr
);

//  walk_until() where there are no wild Pokemon (a gym, a building): any
//  battle that starts is a trainer's, so the walk stops with DIALOG, left in
//  the battle for the caller to fight, instead of trying to run from it
//  (r206d tried to run from the second Oreburgh Gym trainer).
WalkResult walk_until_no_wild(
    VideoStream& stream, ProControllerContext& context,
    JoystickPosition stick, Milliseconds limit, const std::string& label
);

//  Holds the left stick at `stick` for `duration` in one go, for a walk of a
//  set length: walk_until() checks the screen between short holds, and each
//  break can cost a step. Stops early for a dialog or a transition. A wild
//  battle is run from and ends the walk with `wild_battles` 1, at a place the
//  caller cannot know.
WalkResult walk_for(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds duration, const std::string& label
);

//  The stop walk_expecting() requires, and what counts as arrived.
struct WalkExpectation{
    WalkStop stop = WalkStop::ARRIVED;
    WalkArrived arrived = nullptr;
};
//  walk_until() that requires `expected.stop`. Fails with the screenshot otherwise.
WalkResult walk_expecting(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label,
    const WalkExpectation& expected
);

//  After a move that may have walked onto an encounter tile (a tap in a cave
//  or in tall grass): waits briefly for a wild battle and runs from it.
//  Returns whether there was one; we may then be off the place we expected.
//  `events`, if given, records what was seen, timed.
bool run_from_late_encounter(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const std::string& label,
    Pokemon::ScreenEventLog* events = nullptr
);

//  walk_until() toward a wall the route was measured to end at. Seeing the
//  stop only ends the hold early: holding on for the whole `limit` also
//  counts as there, as people or animations nearby can keep the screen
//  changing at a wall (s03a1: a hiker and a child by the Mart fence; s03c1:
//  Roark's pickaxe; r162a: Jubilife's TV). A wrong place is caught by the
//  next gate. Fails on a dialog or a transition: we are not where we think.
WalkResult walk_to_wall(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label
);

//  Walks toward someone to talk to: after each short stretch, presses A and stops
//  once a dialogue opens. Use it where walking into the person does not read as
//  BLOCKED, e.g. in tall grass, whose sway keeps the screen changing.
WalkResult walk_to_talk(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
