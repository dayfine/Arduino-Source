/*  BDSP AutoStory - Story interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "CommonTools/Async/InferenceRoutines.h"
#include "Actions/PokemonBDSP_AutoStoryEvents.h"
#include "PokemonBDSP_AutoStoryInterrupts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::InterruptKind;
using Pokemon::InterruptPose;


bool presses_through_text(std::string_view action){
    return action == "scene" || action == "answer" || action == "battle" ||
        action == "new_game" || story_events().contains(std::string(action));
}

Pokemon::StoryInterrupts story_interrupts(Pokemon::ScreenLookFunction look, const StoryInterruptActions& actions){
    Pokemon::StoryInterrupts out;
    out.look = std::move(look);
    out.handlers.add(InterruptKind::WILD_ENCOUNTER, {.handle = actions.wild, .pose = InterruptPose::UNCHANGED});
    out.handlers.add(InterruptKind::TRAINER_SIGHT, {.handle = actions.trainer, .pose = InterruptPose::UNCHANGED});
    out.handlers.add(InterruptKind::DIALOG, {.handle = actions.dialog, .pose = InterruptPose::UNCHANGED});
    out.handlers.add(InterruptKind::WARP, {.handle = actions.warp, .pose = InterruptPose::LOST});
    for (std::string_view action : {"scene", "answer", "battle", "new_game"}){
        out.handlers.starts_on(std::string(action), InterruptKind::DIALOG);
    }
    out.handlers.starts_on("battle", InterruptKind::TRAINER_SIGHT);
    //  The story's wild battles (the Starly at the lake) are the step's.
    out.handlers.starts_on("battle", InterruptKind::WILD_ENCOUNTER);
    out.handlers.starts_on("train", InterruptKind::WILD_ENCOUNTER);
    for (InterruptKind kind : {InterruptKind::WILD_ENCOUNTER, InterruptKind::TRAINER_SIGHT, InterruptKind::WARP}){
        out.handlers.starts_on("new_game", kind);
    }
    for (const auto& [name, unused] : story_events()){
        for (InterruptKind kind : {InterruptKind::DIALOG, InterruptKind::WILD_ENCOUNTER, InterruptKind::TRAINER_SIGHT, InterruptKind::WARP}){
            out.handlers.starts_on(name, kind);
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
