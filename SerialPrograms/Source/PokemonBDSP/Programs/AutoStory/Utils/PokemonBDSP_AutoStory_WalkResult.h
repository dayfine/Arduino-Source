/*  BDSP AutoStory - Walk result
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a walk ended, and what it saw on the way.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WalkResult_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WalkResult_H

#include <functional>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "Pokemon/AutoStory/Pokemon_SceneJudges.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Runs from a wild battle that has interrupted a walk, and returns once the
//  player stands on the overworld again. The walker only sees that a battle
//  started; how to leave it is the program's battle code, passed in.
using WildBattleEscape = std::function<void(VideoStream& stream, ProControllerContext& context)>;

//  Possible termination conditions for an automated walking sequence.
enum class WalkStop{
    ARRIVED,        //  `arrived` matched.
    DIALOG,         //  A dialogue box opened: an NPC or a trainer stopped us.
    TRANSITION,     //  A fade (WalkResult::warp) with no battle after it: a door or a new area.
    BLOCKED,        //  The screen stopped changing: we are walking into a wall.
    LIMIT,          //  Walked `limit` without any of the above.
};
//  Returns display name string corresponding to given WalkStop condition.
std::string walk_stop_name(WalkStop stop);

//  Whether the walk has arrived, from the screen after each chunk.
using WalkArrived = std::function<bool(const ImageViewRGB32& screen)>;

//  An encounter already active at the first input, or found only after a
//  leg, may belong to earlier movement: it began on the tile that movement
//  ended on, so the leg's tiles from its start, none moved to all, still
//  hold where we are.
enum class WalkOrigin{ CURRENT_LEG, MAY_PRECEDE_LEG };

//  The fade and warp judgements are shared (Pokemon_ScreenEvents.h,
//  Pokemon_SceneJudges.h); BDSP's walks and steps fill them.
//  How a screen's fade looks.
//  "black fade", for the log.
//  A fade's most texture (the walk's black-screen watcher uses it too).
//  A dark fade's brightest rgb sum (likewise).
//  The fade a screen's middle shows, from its stats.
//  The fade a screen shows.
//  What a walk or a step saw of a warp: a fade or a banner.
using Pokemon::WarpSeen;

//  Result metrics and terminal state returned by automated walking execution.
struct WalkResult{
    WalkStop stop;
    Milliseconds walked{};
    size_t wild_battles = 0;
    //  Count of completed steps or taps that clearly moved. Blocked taps do
    //  not count (r248a: "4 taps before the battle" were 2 steps and 2
    //  taps into Taylor's freeze).
    size_t moved_chunks = 0;
    WalkOrigin origin = WalkOrigin::CURRENT_LEG;
    //  The fade that ended a TRANSITION, and any location banner read on
    //  the way (#268 slice 2: walkers report warps).
    WarpSeen warp = {};
    //  What the walk's watchers saw, timed, from its first input through
    //  the watch after it: an encounter's flash, a battle, a box, a fade.
    //  The shared judges read it (Pokemon::classify_interrupt()) to tell
    //  what stopped the walk and when its step was taken. Empty where
    //  nothing recorded it.
    ScreenEventLog events{};
    //  When the walk's first input went in; `events` are timed against it.
    WallClock started = {};
    //  Chunks that moved after the last wild battle was run from: the taps
    //  the walk went on with.
    size_t moved_after_battle = 0;
};


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
