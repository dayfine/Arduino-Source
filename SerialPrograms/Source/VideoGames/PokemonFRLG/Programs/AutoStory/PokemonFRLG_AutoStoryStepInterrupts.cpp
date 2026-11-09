/*  AutoStory Step Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Color.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::InterruptKind;
using Pokemon::InterruptPose;
using Pokemon::StoryStepKind;
using namespace std::chrono_literals;

//  A warp's scene is a fade and the map's name: well within this.
constexpr Milliseconds WARP_SCENE_TIMEOUT = 60000ms;


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

StepInterruptActions console_interrupt_actions(
    ConsoleHandle& console, ProControllerContext& context,
    std::function<OverworldInterruptSet()> walker
){
    const auto walker_clears = [&console, &context, walker](bool wild){
        return [&console, &context, walker, wild](Pokemon::RunState&, const Pokemon::InterruptVerdict&){
            const OverworldInterruptSet set = walker();
            (wild ? set.wild : set.other).front()->clear(console, context);
            return Pokemon::outcome_ok();
        };
    };
    return StepInterruptActions{
        .wild = walker_clears(true),
        .trainer = walker_clears(false),
        .dialog = walker_clears(false),
        .warp = [&console, &context](Pokemon::RunState&, const Pokemon::InterruptVerdict&){
            mash_until_overworld(console, context, WARP_SCENE_TIMEOUT);
            return Pokemon::outcome_ok();
        },
    };
}

Pokemon::StoryInterrupts step_interrupts(Pokemon::ScreenLookFunction look, const StepInterruptActions& actions){
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
    out.handlers.starts_on(StoryStepKind::BATTLE, InterruptKind::WILD_ENCOUNTER);
    for (StoryStepKind step : {StoryStepKind::GAME, StoryStepKind::NEW_GAME}){
        for (InterruptKind kind : {InterruptKind::WILD_ENCOUNTER, InterruptKind::TRAINER_SIGHT, InterruptKind::WARP}){
            out.handlers.starts_on(step, kind);
        }
    }
    return out;
}


ConsoleScreenLook::ConsoleScreenLook(VideoStream& stream, ProControllerContext& context)
    : m_stream(stream)
    , m_context(context)
    , m_watcher(m_observer)
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


void InterruptLogGuard::on_interrupt(const Pokemon::StoryStep& step, const Pokemon::InterruptCheck& check){
    const bool cleared = check.outcome.succeeded();
    m_logger.log(std::format(
        "Story: {}: before it, {}: {}", step.name,
        Pokemon::interrupt_kind_name(check.verdict.kind), cleared ? "cleared" : Pokemon::to_string(check.outcome)
    ), cleared ? COLOR_ORANGE : COLOR_RED);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
