/*  Story Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What stops the story between two of its steps: someone walks up and
 *  talks, a trainer sees us as a step ends, a wild battle starts on the
 *  last tile of a walk, a door takes us somewhere.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryInterrupts_H
#define PokemonAutomation_Pokemon_StoryInterrupts_H

#include <optional>
#include <string>
#include <string_view>
#include "Pokemon_InterruptHandlers.h"
#include "Pokemon_Interrupts.h"
#include "Pokemon_StoryFailure.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How the screen looked at the end of a look: the line a run log needs to
//  answer "had the scene ended before the step's first input?".
struct ScreenSettle{
    //  Whether the look saw a frame at all.
    bool seen = false;
    //  The events its last frame showed.
    ScreenEvents showing = {};
    //  How long the screen had shown them unchanged when the look ended.
    Milliseconds quiet = Milliseconds(0);
};
//  The end of `look`: its last frame's events, and how long they held.
ScreenSettle judge_settle(const ScreenLook& look);
//  Whether it ended on the HOME menu: the game is not running.
bool on_home_menu(const ScreenSettle& settle);
//  Whether it ended on the overworld with no box, no fade and no battle.
bool is_settled(const ScreenSettle& settle);
//  "settled overworld (quiet 0.3 s)", "black fade showing (quiet 0.1 s)",
//  "Switch HOME menu (quiet 0.3 s)", "no frame seen".
std::string describe(const ScreenSettle& settle);


//  What a look between steps found, and what clearing it came to.
struct InterruptCheck{
    //  How the screen looked when the look ended, judged or not.
    ScreenSettle screen = {};
    //  NONE when nothing interrupted, or when the step starts on it.
    InterruptVerdict verdict = {};
    //  Set when the look stayed unsettled, the kind had no handler, or
    //  the handler failed; GAME_NOT_RUNNING, with no verdict, when the
    //  look ended on the Switch's HOME menu.
    std::optional<StoryFailure> failure = std::nullopt;
    //  True when clearing the interrupt invalidated the player's pose (a warp).
    bool pose_lost = false;
};

//  Looks at the screen before step `step_name` (`action`) and clears what
//  interrupted the player, if anything. On the HOME menu it fails as
//  GAME_NOT_RUNNING and clears nothing, whatever the step starts on.
InterruptCheck clear_story_interrupt(
    const StoryInterrupts& interrupts, std::string_view step_name, std::string_view action
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
