/*  Interrupt Handlers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Split out of Pokemon_StoryInterrupts.h, unchanged; it includes this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_InterruptHandlers_H
#define PokemonAutomation_Pokemon_InterruptHandlers_H

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



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
