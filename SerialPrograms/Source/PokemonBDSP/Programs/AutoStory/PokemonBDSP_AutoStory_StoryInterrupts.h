/*  BDSP AutoStory - Story interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's interrupts between story steps (Pokemon_StoryInterrupts.h): a
 *  wild battle is run from, a trainer who saw us is fought, a dialog is
 *  pressed through, and a warp is waited out, the pose then lost. The
 *  console (ConsoleScreenLook and the executor's console actions) and the
 *  simulation bind the same table.
 *
 *  The steps that press through text (MASH, ANSWER, BATTLE, GAME,
 *  NEW_GAME) start on a dialog: one showing before them is theirs. A
 *  BATTLE and a GAME step start on a trainer's lines too, TRAIN on a wild
 *  battle, and a GAME step (a measured script) and NEW_GAME (the title
 *  screen) on anything.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryInterrupts_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryInterrupts_H

#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_ScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  What clears each kind of interrupt, as a binding does it.
struct StoryInterruptActions{
    Pokemon::InterruptAction wild = {};
    Pokemon::InterruptAction trainer = {};
    Pokemon::InterruptAction dialog = {};
    Pokemon::InterruptAction warp = {};
};

//  BDSP's interrupts: `look` judged with the shared knobs, `actions` for
//  each kind, and the steps that start on one.
Pokemon::StoryInterrupts story_interrupts(Pokemon::ScreenLookFunction look, const StoryInterruptActions& actions);

//  Whether a step of `kind` presses through any text showing when it
//  starts; a step of another kind is stopped by it (a walk, a talk).
bool presses_through_text(Pokemon::StoryStepKind kind);


//  How long the console looks at the screen before a step: a few frames,
//  for what came up after the step before had settled.
constexpr Milliseconds BETWEEN_STEPS_LOOK = Milliseconds(300);

//  The console's look between steps: BDSP's screen events for
//  BETWEEN_STEPS_LOOK, everything showing in it judged.
class ConsoleScreenLook{
public:
    ConsoleScreenLook(VideoStream& stream, ProControllerContext& context);

    Pokemon::ScreenLook look(bool fresh);

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    BdspScreenObserver m_observer;
    Pokemon::ScreenEventWatcher m_watcher;
    WallClock m_start;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
