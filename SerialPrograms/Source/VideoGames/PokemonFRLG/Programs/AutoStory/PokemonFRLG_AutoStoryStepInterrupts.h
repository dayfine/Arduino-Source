/*  AutoStory Step Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's interrupts between story steps (Pokemon_StoryInterrupts.h): a
 *  wild battle is run from, a trainer who saw the player is fought, a
 *  message is read to its end (and a trainer's battle after it fought),
 *  and a warp's fade is pressed through, the pose then lost. The console
 *  (ConsoleScreenLook and console_interrupt_actions()), the simulation
 *  and the replay bind the same table.
 *
 *  The steps that press through text (MASH, ANSWER, BATTLE, GAME,
 *  NEW_GAME) start on a message: one showing before them is theirs. A
 *  BATTLE starts on its trainer's lines, and on its battle already
 *  showing, which reads as a wild one when the lines were missed. A GAME
 *  step (a measured script: a capture, the PC, a gift's summary) and
 *  NEW_GAME (the title screen) start on anything. FRLG's story has no
 *  TRAIN steps.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStepInterrupts_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStepInterrupts_H

#include <functional>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeScreenObserver.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What clears each kind of interrupt, as a binding does it.
struct StepInterruptActions{
    Pokemon::InterruptAction wild = {};
    Pokemon::InterruptAction trainer = {};
    Pokemon::InterruptAction dialog = {};
    Pokemon::InterruptAction warp = {};
};

//  FRLG's interrupts between steps: `look` judged with the shared knobs,
//  `actions` for each kind, and the steps that start on one. The knobs
//  are the shared defaults, BDSP's timings: FRLG's flash-to-battle and
//  lines-to-battle times are not measured on the console yet.
Pokemon::StoryInterrupts step_interrupts(Pokemon::ScreenLookFunction look, const StepInterruptActions& actions);

//  The console's actions, as the walker clears what stops a leg
//  part-way (`walker()`'s interrupts, story_interrupts() in
//  PokemonFRLG_AutoStoryBattle.h, built when one is cleared): a wild
//  battle is run from, a message read to its end and a trainer's battle
//  after it fought. A warp's scene is pressed through.
StepInterruptActions console_interrupt_actions(
    ConsoleHandle& console, ProControllerContext& context,
    std::function<OverworldInterruptSet()> walker
);

//  Whether a step of `kind` presses through any text showing when it
//  starts; a step of another kind is stopped by it (a walk, a talk).
bool presses_through_text(Pokemon::StoryStepKind kind);


//  How long the console looks at the screen before a step: the walker's
//  wait for a message box (its StoryMessage interrupt's detection delay).
//  Not measured as a look between steps yet.
constexpr Milliseconds BETWEEN_STEPS_LOOK = Milliseconds(400);

//  The console's look between steps: FrlgScreenObserver's events for
//  BETWEEN_STEPS_LOOK, everything showing in it judged.
//  The Switch's HOME menu comes first (SwitchHomeObserver), so a
//  step that starts on it fails instead of pressing buttons on HOME.
class ConsoleScreenLook{
public:
    ConsoleScreenLook(ConsoleHandle& console, ProControllerContext& context);

    Pokemon::ScreenLook look(bool fresh);

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    FrlgScreenObserver m_observer;
    SwitchHomeObserver m_eyes;
    ScreenEventWatcher m_watcher;
    WallClock m_start;
};

//  The console's guard: a failed step fails it, as before; each interrupt
//  found between steps is logged, cleared or not, and how the screen looked
//  before every step.
class InterruptLogGuard : public Pokemon::StepGuard{
public:
    explicit InterruptLogGuard(Logger& logger)
        : m_logger(logger)
    {}

    virtual void on_interrupt(const Pokemon::StoryStep& step, const Pokemon::InterruptCheck& check) override;
    virtual void on_screen(const Pokemon::StoryStep& step, const Pokemon::ScreenSettle& screen) override;

private:
    Logger& m_logger;
};


//  The binding's table against a scripted screen.
void add_tests_AutoStoryStepInterrupts(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
