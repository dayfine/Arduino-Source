/*  BDSP AutoStory - Walk screen
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How much the screen changes as we walk, and following a warp's fade frame by frame.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WalkScreen_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WalkScreen_H

#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "Pokemon/AutoStory/Pokemon_SceneJudges.h"
#include "Pokemon/AutoStory/Pokemon_Transitions.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  How much the screen changed between two frames, 0 for identical. Walking scrolls
//  the whole view: 16 to 85 per 400ms on Route 202, against about 6 for running in
//  place against trees.
double walk_screen_change(const ImageViewRGB32& before, const ImageViewRGB32& after);
//  Screen change metric threshold indicative of blocked movement against an obstacle.
constexpr double WALK_BLOCKED_CHANGE = 16.0;
//  For whole-tile D-pad taps: a tap that moves changes the screen by 21 to 50
//  (r244a: 40 per tap along Route 204's row 27); a tap into a wall or an NPC
//  bumps for 10 to 13.4, and an NPC walking past does the same, so 10 counted
//  bumps as steps (r244a, 2026-10-31).
constexpr double WALK_TAP_BLOCKED_CHANGE = 16.0;
//  More than walking changes in 400ms: a new scene, such as a battle opening.
constexpr double WALK_SCENE_CHANGE = 120.0;

namespace detail{
//  What a change of the whole screen walking cannot make turns out to be.
enum class WholeScreenChange{
    BATTLE,         //  the battle's menu or text box showed
    WARP,           //  positive evidence of a map change: a fade or a banner first read
    UNCLASSIFIED,   //  neither yet: an encounter's swirl before its menu looks like
                    //  this (lab-B 2026-11-12 21:58, a Geodude in 01.03's mine); wait
                    //  for the battle detector, never end the walk on it
};
//  What the watchers read after the change.
struct WholeScreenEvidence{
    bool battle_showed = false;
    bool fade_or_banner_read = false;
};
//  Classifies a whole-screen change from what was read after it.
inline WholeScreenChange classify_whole_screen_change(const WholeScreenEvidence& evidence){
    if (evidence.battle_showed){
        return WholeScreenChange::BATTLE;
    }
    return evidence.fade_or_banner_read ? WholeScreenChange::WARP : WholeScreenChange::UNCLASSIFIED;
}
} // namespace detail
using detail::WholeScreenChange;
using detail::WholeScreenEvidence;
using detail::classify_whole_screen_change;

//  Whether two frames show different scenes: a change walking cannot make
//  (WALK_SCENE_CHANGE). UNKNOWN without both frames.
Pokemon::SceneChange walk_scenes_differ(const Pokemon::TransitionSample& before, const Pokemon::TransitionSample& after);

//  What a warp's fade must show before a step takes it as one. All of it
//  reads Pokemon::BDSP_FADE_PROFILE. Dark is the profile's uniform tier, or
//  with `partial_dark` its partial tier's texture (a doorway's dim fade
//  frames, which run_into_door() has stopped on since #268) up to the same
//  dark sum. Dark must hold `dark_for` and white (always uniform) `white_for`.
struct WarpFadeRule{
    bool partial_dark = false;
    Milliseconds dark_for = Milliseconds(0);
    Milliseconds white_for = Milliseconds(0);
};

//  A warp's fade followed frame by frame (a Pokemon::TransitionTracker on
//  BDSP_FADE_PROFILE) rather than judged on one frame. A fade shows when its
//  dark or white has held as the rule says; or, when its hold fell between
//  two frames (a 50 ms door fade), at the first frame of another scene
//  after the fall, as `compare` tells scenes apart. That one unseen hold
//  used to leave a door run to its 12 s timeout. Not thread-safe.
class WarpFadeWatch{
public:
    explicit WarpFadeWatch(WarpFadeRule rule, Pokemon::TransitionTracker::CompareScenes compare = walk_scenes_differ);

    //  Adds a frame; times must not go back. Returns the fade this frame
    //  shows, NONE if none (yet).
    WarpFade push(const Pokemon::TransitionSample& sample);
    //  When the screen last went dark as the rule counts dark; none if it
    //  has not.
    std::optional<WallClock> went_dark() const{
        return m_went_dark;
    }
    //  How the last fade showed, for the log; "" before one.
    const std::string& how() const{
        return m_how;
    }

private:
    WarpFadeRule m_rule;
    Pokemon::TransitionTracker m_tracker;
    std::optional<WallClock> m_dark_since;
    std::optional<WallClock> m_white_since;
    std::optional<WallClock> m_went_dark;
    std::string m_how;
};


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
