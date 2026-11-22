/*  BDSP AutoStory - Story interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's interrupts between story steps (Pokemon_StoryInterrupts.h): a
 *  wild battle is run from, a trainer who saw us is fought, a dialog is
 *  pressed through, and a warp is waited out, the pose then lost.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStoryInterrupts_H
#define PokemonAutomation_PokemonBDSP_AutoStoryInterrupts_H

#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeScreenObserver.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEventLog.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStoryScreenEvents.h"

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
//  each kind, and the step actions that start on one.
Pokemon::StoryInterrupts story_interrupts(Pokemon::ScreenLookFunction look, const StoryInterruptActions& actions);

//  Whether a step with `action` presses through any text showing when it
//  starts; another action is stopped by it (a walk, a talk).
bool presses_through_text(std::string_view action);


//  How long the console looks at the screen before a step: a few frames,
//  for what came up after the step before had settled.
constexpr Milliseconds BETWEEN_STEPS_LOOK = Milliseconds(300);

//  The console's look between steps: BDSP's screen events for
//  BETWEEN_STEPS_LOOK, everything showing in it judged.
class ConsoleScreenLook{
public:
    ConsoleScreenLook(ConsoleHandle& console, ProControllerContext& context);

    Pokemon::ScreenLook look(bool fresh);

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    BdspScreenObserver m_observer;
    SwitchHomeObserver m_eyes;
    ScreenEventWatcher m_watcher;
    WallClock m_start;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
