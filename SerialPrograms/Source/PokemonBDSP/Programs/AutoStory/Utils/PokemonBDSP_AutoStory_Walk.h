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
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "Controllers/Joystick.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Runs from a wild battle that has interrupted a walk, and returns once the
//  player stands on the overworld again. The walker only sees that a battle
//  started; how to leave it is the program's battle code, passed in.
using WildBattleEscape = std::function<void(VideoStream& stream, ProControllerContext& context)>;

enum class WalkStop{
    ARRIVED,        //  `arrived` matched.
    DIALOG,         //  A dialogue box opened: an NPC or a trainer stopped us.
    TRANSITION,     //  The screen went black and came back without a battle: a door or a new area.
    BLOCKED,        //  The screen stopped changing: we are walking into a wall.
    LIMIT,          //  Walked `limit` without any of the above.
};
const char* walk_stop_name(WalkStop stop);

//  Whether the walk has arrived, from the screen after each chunk.
using WalkArrived = std::function<bool(const ImageViewRGB32& screen)>;

struct WalkResult{
    WalkStop stop;
    Milliseconds walked;
    size_t wild_battles;
    //  Chunks whose screen change passed the blocked threshold: on a tap
    //  walk, the taps that moved a tile. A trainer's freeze and a bump do
    //  not count (r248a: "4 taps before the battle" were 2 steps and 2
    //  taps into Taylor's freeze).
    size_t moved_chunks = 0;
};

//  Holds the left stick at `stick` until one of the WalkStop events. Wild battles on
//  the way are run from, and walking resumes. Does not throw for any WalkStop; the
//  caller decides which ones are expected.
WalkResult walk_until(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label,
    const WalkArrived& arrived = nullptr
);

//  How walk_until_dpad() presses: RUN holds B too, twice as fast and as
//  tile-true.
enum class DpadPace{
    WALK,
    RUN,
};
struct DpadWalkOptions{
    WalkArrived arrived = nullptr;
    DpadPace pace = DpadPace::WALK;
};

//  walk_until() on the D-pad, for a straight walk along a row or a column
//  (`stick` is one of the four directions). The D-pad moves whole tiles and
//  keeps to the row: the stick drifts off it, catches tree corners and slides
//  a row on stairs (Route 203, r226a and r227b; by hand the D-pad went
//  through the same places).
WalkResult walk_until_dpad(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label,
    const DpadWalkOptions& options = {}
);

//  A set number of whole tiles on the D-pad, a tap a tile (a tap moves
//  exactly one tile; a stick hold timed for n tiles is a tile or two off),
//  with the same watch for wild battles and dialogue as walk_until(). Ends
//  LIMIT after `tiles` taps, or BLOCKED when two taps in a row move nothing.
WalkResult walk_tiles_dpad(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, size_t tiles, const std::string& label
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
bool run_from_late_encounter(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const std::string& label);

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

//  How much the screen changed between two frames, 0 for identical. Walking scrolls
//  the whole view: 16 to 85 per 400ms on Route 202, against about 6 for running in
//  place against trees.
double walk_screen_change(const ImageViewRGB32& before, const ImageViewRGB32& after);
constexpr double WALK_BLOCKED_CHANGE = 10.0;
//  For whole-tile D-pad taps: a tap that moves changes the screen by 21 to 50
//  (r244a: 40 per tap along Route 204's row 27); a tap into a wall or an NPC
//  bumps for 10 to 13.4, and an NPC walking past does the same, so 10 counted
//  bumps as steps (r244a, 2026-10-31).
constexpr double WALK_TAP_BLOCKED_CHANGE = 16.0;
//  More than walking changes in 400ms: a new scene, such as a battle opening.
constexpr double WALK_SCENE_CHANGE = 120.0;


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
WalkProbe probe_walk(
    VideoStream& stream, ProControllerContext& context,
    JoystickPosition stick, Milliseconds limit,
    double moving_change, const std::string& label
);


}
}
}
#endif
