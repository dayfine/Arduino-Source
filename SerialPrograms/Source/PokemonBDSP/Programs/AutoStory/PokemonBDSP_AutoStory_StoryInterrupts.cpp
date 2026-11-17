/*  BDSP AutoStory - Story interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "CommonTools/Async/InferenceRoutines.h"
#include "PokemonBDSP_AutoStory_StoryInterrupts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::InterruptKind;
using Pokemon::InterruptPose;
using Pokemon::StoryStepKind;


bool presses_through_text(StoryStepKind kind){
    switch (kind){
    case StoryStepKind::MASH:
    case StoryStepKind::ANSWER:
    case StoryStepKind::BATTLE:
    case StoryStepKind::GAME:
    case StoryStepKind::NEW_GAME:
        return true;
    default:
        return false;
    }
}

Pokemon::StoryInterrupts story_interrupts(Pokemon::ScreenLookFunction look, const StoryInterruptActions& actions){
    Pokemon::StoryInterrupts out;
    out.look = std::move(look);
    out.handlers.add(InterruptKind::WILD_ENCOUNTER, {.handle = actions.wild, .pose = InterruptPose::UNCHANGED});
    out.handlers.add(InterruptKind::TRAINER_SIGHT, {.handle = actions.trainer, .pose = InterruptPose::UNCHANGED});
    out.handlers.add(InterruptKind::DIALOG, {.handle = actions.dialog, .pose = InterruptPose::UNCHANGED});
    out.handlers.add(InterruptKind::WARP, {.handle = actions.warp, .pose = InterruptPose::LOST});
    for (StoryStepKind step : {StoryStepKind::MASH, StoryStepKind::ANSWER, StoryStepKind::BATTLE, StoryStepKind::GAME, StoryStepKind::NEW_GAME}){
        out.handlers.starts_on(step, InterruptKind::DIALOG);
    }
    out.handlers.starts_on(StoryStepKind::BATTLE, InterruptKind::TRAINER_SIGHT);
    //  The story's wild battles (the Starly at the lake) are the step's.
    out.handlers.starts_on(StoryStepKind::BATTLE, InterruptKind::WILD_ENCOUNTER);
    out.handlers.starts_on(StoryStepKind::TRAIN, InterruptKind::WILD_ENCOUNTER);
    for (StoryStepKind step : {StoryStepKind::GAME, StoryStepKind::NEW_GAME}){
        for (InterruptKind kind : {InterruptKind::WILD_ENCOUNTER, InterruptKind::TRAINER_SIGHT, InterruptKind::WARP}){
            out.handlers.starts_on(step, kind);
        }
    }
    return out;
}


ConsoleScreenLook::ConsoleScreenLook(ConsoleHandle& console, ProControllerContext& context)
    : m_stream(console)
    , m_context(context)
    , m_observer(BattleCue::WILD_MENU, "")
    , m_eyes(console, m_observer)
    , m_watcher(m_eyes)
    , m_start(current_time())
{}

Pokemon::ScreenLook ConsoleScreenLook::look(bool fresh){
    if (fresh){
        m_watcher.reset();
        m_start = current_time();
    }
    wait_until(m_stream, m_context, BETWEEN_STEPS_LOOK, {m_watcher});
    return {.log = m_watcher.log(), .window = {.start = m_start, .end = current_time()}};
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
