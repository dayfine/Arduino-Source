/*  Story Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What stops the story between two of its steps: someone walks up and
 *  talks, a trainer sees us as a step ends, a wild battle starts on the
 *  last tile of a walk, a door takes us somewhere. A walk's own walker
 *  clears what happens part-way through it (Pokemon_LegWalker.h, BDSP's
 *  walks); this is what is left once a step has settled.
 *
 *  Before each step, and after a step fails, the TableExecutor looks at
 *  the screen through the game's eyes (ScreenLookFunction), judges the
 *  look with the shared classifier (classify_interrupt()) and clears what
 *  it found with the game's handler for that kind of interrupt. The
 *  handlers are a table, like the story's actions: data a game binds to
 *  its console or its simulation, not a type per game.
 *
 *  A step that starts on what the look found (a MASH on its scene's box,
 *  a BATTLE on its trainer's lines) is not interrupted by it: the table
 *  says which (InterruptHandlerTable::starts_on()). A step that starts on
 *  a battle also starts on an UNSETTLED screen, a fade with no battle yet:
 *  that is its battle's opening, still playing (BDSP's Starly battle right
 *  after the starter is picked), and the step waits for it itself.
 *
 *  Before another step, an unsettled screen that may be a battle's opening
 *  (a white flash, a fade still down, a fade after a dialog box) is looked
 *  at until its battle shows, up to max_opening_looks looks: the walk
 *  before the step ended in grass or in a trainer's sight. The battle is
 *  then cleared as any other. A fade that came back up with no box before
 *  it (a door's) is looked at max_looks times only.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryInterrupts_H
#define PokemonAutomation_Pokemon_StoryInterrupts_H

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include "Pokemon_Interrupts.h"
#include "Pokemon_ScreenEvents.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_StoryStepExecutor.h"
#include "Pokemon_InterruptHandlers.h"

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
    //  Failed when the look stayed unsettled, the kind had no handler, or
    //  the handler failed; GAME_NOT_RUNNING, with no verdict, when the
    //  look ended on the Switch's HOME menu.
    Outcome outcome = {};
};

//  Looks at the screen before `step` (or after it failed) and clears what
//  interrupted the player, if anything, writing the handler's effect on
//  the pose into `state`. On the HOME menu it fails as GAME_NOT_RUNNING
//  and clears nothing, whatever the step starts on.
InterruptCheck clear_story_interrupt(const StoryInterrupts& interrupts, const StoryStep& step, RunState& state);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
