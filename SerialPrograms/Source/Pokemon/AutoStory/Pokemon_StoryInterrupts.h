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
#include "Pokemon_StorySteps.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a game saw of the screen between two steps: its timed events, and
//  the span of them the classifier judges.
struct ScreenLook{
    ScreenEventLog log{};
    InterruptWindow window = {};
};

//  Looks at the screen. `fresh` starts a new look; otherwise the look goes
//  on from the last one, to see how an unsettled screen ends.
using ScreenLookFunction = std::function<ScreenLook(bool fresh)>;

//  Clears an interrupt a look found: flees or fights the battle, closes
//  the dialog, waits out the warp.
using InterruptAction = std::function<Outcome(RunState& state, const InterruptVerdict& verdict)>;

//  A row of an InterruptHandlerTable.
struct InterruptHandler{
    InterruptAction handle = {};
    //  What clearing it did to where the player is.
    InterruptPose pose = InterruptPose::UNCHANGED;
};


//  A game's handlers by kind of interrupt, and the steps that start on one.
class InterruptHandlerTable{
public:
    //  The handler for `kind`: one of WILD_ENCOUNTER, TRAINER_SIGHT,
    //  DIALOG or WARP. Another kind, or a second handler for one, throws
    //  InternalProgramError.
    void add(InterruptKind kind, InterruptHandler handler);
    //  The handler for `kind`; nullptr if there is none.
    const InterruptHandler* find(InterruptKind kind) const;

    //  Steps of `step` start on `kind` showing: before one, it is the
    //  step's to clear, not an interrupt. A step that starts on
    //  WILD_ENCOUNTER or TRAINER_SIGHT starts on UNSETTLED too.
    void starts_on(StoryStepKind step, InterruptKind kind);
    bool step_starts_on(StoryStepKind step, InterruptKind kind) const;

private:
    std::map<InterruptKind, InterruptHandler> m_handlers;
    std::set<std::pair<StoryStepKind, InterruptKind>> m_starts_on;
};


//  A game's interrupts between steps: how it looks, how it judges what it
//  saw, and what it does about each kind.
struct StoryInterrupts{
    ScreenLookFunction look = {};
    InterruptKnobs knobs = {};
    InterruptHandlerTable handlers = {};
    //  Looks at a screen that stays UNSETTLED (a fade with no battle yet)
    //  before it fails the step as "interrupt_unsettled". A step that
    //  starts on it gets it on the first look.
    size_t max_looks = 3;
    //  The same, for a screen that may be a battle's opening: enough looks
    //  for its battle to show. BDSP's 300 ms looks cover 12 s, past its
    //  battle_after_fade (8 s), after which a fade with no battle settles.
    size_t max_opening_looks = 40;
    //  Interrupts cleared for one step before it fails as
    //  "too_many_interrupts".
    size_t max_per_step = 3;
};


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
InterruptCheck clear_story_interrupt(const StoryInterrupts& interrupts, RunState& state, const StoryStep& step);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
