/*  Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What stopped the player, judged from the order of screen events
 *  (ScreenEventLog), the same way for every game: a wild encounter, a
 *  trainer's sight, a dialog, a warp. One frame cannot tell them apart; the
 *  order can. A white flash then a battle is an encounter, a white fade with
 *  no battle after it a cave exit (BDSP's walk fled one as a battle, #268);
 *  a box then a flash then a battle is a trainer's lines; a flash then a box
 *  then a battle is a wild battle's opening text.
 *
 *  Games differ in timing only, so they pass their numbers (InterruptKnobs)
 *  rather than their own classifier.
 *
 */

#ifndef PokemonAutomation_Pokemon_Interrupts_H
#define PokemonAutomation_Pokemon_Interrupts_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon_ScreenEvents.h"
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
const char* warp_judgement_name(WarpJudgement judgement);

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


//  What stopped the player.
enum class InterruptKind{
    NONE,
    WILD_ENCOUNTER,     //  A battle with no trainer's lines before it.
    TRAINER_SIGHT,      //  A dialog box, then a battle.
    DIALOG,             //  A dialog box and no battle.
    WARP,               //  A fade back up with no battle after it, or a banner.
    UNSETTLED,          //  A fade, too recent to know if a battle follows.
};
//  "trainer sight", for the log.
const char* interrupt_kind_name(InterruptKind kind);

//  What stopped the player, and the evidence for it.
struct InterruptVerdict{
    InterruptKind kind = InterruptKind::NONE;
    //  The changes it was judged on, in order.
    std::vector<ScreenEventChange> evidence = {};
    //  The log dropped changes the window reaches back to: the verdict may
    //  miss what came first.
    bool log_truncated = false;
};
//  "wild encounter: +0 ms white fade rose, +3400 ms battle started rose",
//  times from `origin`.
std::string describe(const InterruptVerdict& verdict, WallClock origin);

//  What interrupted the player in `window` of `log`. Order of precedence: a
//  battle, then a warp, then a dialog. Only events that rose in the window
//  count: a box already up when it starts is not this interrupt's.
InterruptVerdict classify_interrupt(
    const ScreenEventLog& log, InterruptWindow window, const InterruptKnobs& knobs
);


//  How long after the step that set an interrupt off one of its events
//  shows, as a game measured it on its console.
struct EventLag{
    ScreenEvent event = ScreenEvent::BATTLE_STARTED;
    Milliseconds lag = Milliseconds(0);
};

//  When the step that set `verdict`'s interrupt off was taken: the first
//  rise in its evidence that `lags` times, less that lag. The first, as it
//  shows nearest the step. Nothing if `lags` times none of them, as for a
//  game that hasn't measured its events, or for no verdict. Lags are one
//  kind of interrupt's (a wild battle's box, not a trainer's): the caller
//  checks the verdict's kind first.
//
//  This places an interrupt seen only after a walk's hold has ended, by
//  what the log saw of it during the hold, instead of by when the watcher
//  that stopped for it fired.
std::optional<WallClock> interrupt_step_time(const InterruptVerdict& verdict, const std::vector<EventLag>& lags);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
