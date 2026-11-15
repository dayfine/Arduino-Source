/*  Warp Judge
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The warp judge: whether a fade was a warp, from a game's timing. Its
 *  definitions are in Pokemon_Interrupts.cpp.
 *
 */

#ifndef PokemonAutomation_Pokemon_WarpJudge_H
#define PokemonAutomation_Pokemon_WarpJudge_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "Pokemon_Transitions.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A game's timing, as data.
struct InterruptKnobs{
    //  A fade must hold this long to be a warp's. One uniform frame is enough
    //  by default: a 50 ms fade can show on one frame only.
    Milliseconds min_fade_hold = Milliseconds(0);
    //  A battle that shows this soon after a fade ends is that fade's
    //  encounter, and a fade is no warp before this has passed with no
    //  battle. BDSP's encounter flash shows its battle within 8 s
    //  (WHITE_FLASH_BATTLE_WAIT in its walk).
    Milliseconds battle_after_fade = Milliseconds(8000);
    //  A dialog box that rose this long or less before a battle's fade is a
    //  trainer's lines. A trainer's lines can pause more than 4 s before the
    //  battle (BDSP r210d), and a trainer may say several boxes.
    Milliseconds sight_lines_lead = Milliseconds(20000);
};

//  The span of the log to judge: changes at `start` or later, and no later
//  than `end`.
struct InterruptWindow{
    WallClock start = {};
    WallClock end = {};
};


//  How a fade looked, for the warp judge.
struct FadeSpan{
    WarpFade fade = WarpFade::NONE;
    WallClock down = {};
    //  When the screen came back; none while it still shows the fade.
    std::optional<WallClock> up = {};
};

//  What the warp judge made of a fade.
enum class WarpJudgement{
    NO_FADE,        //  No fade went down in the window.
    FADE_SHOWING,   //  It went down and has not come back up.
    TOO_SHORT,      //  It came back up before min_fade_hold.
    SAME_SCENE,     //  It came back up on the scene it left.
    WARPED,         //  Down, held, back up; not on the same scene.
    FLASH,          //  An encounter's flash (a tracked transition only).
    WHITE_UNSURE,   //  A white that may be a flash or a cave exit (a
                    //  tracked transition only): wait for a battle.
};
//  "fade too short", for the log.
std::string warp_judgement_name(WarpJudgement judgement);

//  The warp judge's verdict and the fade it judged.
struct WarpVerdict{
    WarpJudgement judgement = WarpJudgement::NO_FADE;
    //  The last fade that went down in the window, if any.
    std::optional<FadeSpan> fade = {};
};

//  A warp is a fade going down, holding uniform, and coming back up on
//  another scene. The last fade that went down in `window` is judged; a
//  LOCATION_BANNER does not count here (WarpSeen does). `change` is what
//  the caller saw of the scene before and after: UNKNOWN does not rule the
//  warp out. This judges the fade only: a battle after it is
//  classify_interrupt()'s question.
WarpVerdict judge_warp(
    const ScreenEventLog& log, InterruptWindow window,
    const InterruptKnobs& knobs, SceneChange change
);
//  The same judge over a TransitionTracker's transition, which has its own
//  scene comparison. A transition whose hold went unseen onto another scene
//  is a warp whatever min_fade_hold says: the scene change is the evidence.
WarpVerdict judge_warp(const Transition& transition, const InterruptKnobs& knobs);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
