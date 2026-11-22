/*  AutoStory Step Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <memory>
#include <utility>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryTrainerMessage.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::InterruptKind;
using Pokemon::InterruptPose;
using namespace std::chrono_literals;

constexpr Milliseconds WARP_SCENE_TIMEOUT = 60000ms;


bool presses_through_text(std::string_view action){
    if (action == "scene" || action == "answer" || action == "battle" || action == "new_game"){
        return true;
    }
    return !is_core_action(action);
}

namespace{

bool presses_a_first(const StoryStep& step){
    return step.action == CUT_TREE_VARIANT
        || step.action == PUSH_BOULDER_VARIANT
        || step.action == POSTER_SWITCH_VARIANT
        || step.action == SURF_VARIANT;
}

} // namespace

std::string clear_box_before_a(
    const StoryStep& step,
    const Pokemon::ScreenLookFunction& look, const std::function<void()>& clear_box
){
    if (!presses_a_first(step)){
        return "";
    }
    const Pokemon::ScreenSettle before = Pokemon::judge_settle(look(true));
    if (!before.showing.has(ScreenEvent::DIALOG_BOX)){
        return "";
    }
    clear_box();
    const Pokemon::ScreenSettle after = wait_for_quiet_overworld(
        look, MovementMechanics::AFTER_BATTLE_QUIET, BATTLE_EXIT_TIMEOUT
    );
    return std::format("Story: box open before {}: cleared with B, then {}.", step.name, Pokemon::describe(after));
}

FrlgStepAction clearing_box_first(
    FrlgStepAction act, const Pokemon::StoryInterrupts& interrupts, Logger& logger
){
    return [act = std::move(act), &interrupts, &logger](const StoryStep& step, FrlgRunState& state){
        const Pokemon::InterruptHandler* dialog = interrupts.handlers.find(InterruptKind::DIALOG);
        const std::string cleared = clear_box_before_a(step, interrupts.look, [&]{
            if (dialog != nullptr && dialog->handle){
                Pokemon::InterruptVerdict verdict;
                verdict.kind = InterruptKind::DIALOG;
                dialog->handle(verdict);
            }
        });
        if (!cleared.empty()){
            logger.log(cleared, COLOR_ORANGE);
        }
        return act(step, state);
    };
}

StepInterruptActions console_interrupt_actions(
    ConsoleHandle& console, ProControllerContext& context,
    std::function<OverworldInterruptSet()> walker
){
    const auto walker_clears = [&console, &context, walker](bool wild){
        return [&console, &context, walker, wild](const Pokemon::InterruptVerdict&) -> std::optional<Pokemon::StoryFailure>{
            const OverworldInterruptSet set = walker();
            (wild ? set.wild : set.other).front()->clear(console, context);
            return std::nullopt;
        };
    };
    return StepInterruptActions{
        .wild = walker_clears(true),
        .trainer = walker_clears(false),
        .dialog = walker_clears(false),
        .warp = [&console, &context](const Pokemon::InterruptVerdict&) -> std::optional<Pokemon::StoryFailure>{
            mash_until_overworld(console, context, WARP_SCENE_TIMEOUT);
            return std::nullopt;
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
    for (const std::string& action : frlg_action_names()){
        if (presses_through_text(action)){
            out.handlers.starts_on(action, InterruptKind::DIALOG);
        }
        if (action == "battle" || action == "new_game" || !is_core_action(action)){
            out.handlers.starts_on(action, InterruptKind::TRAINER_SIGHT);
            out.handlers.starts_on(action, InterruptKind::WILD_ENCOUNTER);
        }
        if (action == "new_game" || !is_core_action(action)){
            out.handlers.starts_on(action, InterruptKind::WARP);
        }
    }
    return out;
}


ConsoleScreenLook::ConsoleScreenLook(ConsoleHandle& console, ProControllerContext& context)
    : m_stream(console)
    , m_context(context)
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


bool quiet_overworld(const Pokemon::ScreenSettle& settle, Milliseconds quiet){
    return settle.seen && settle.showing == ScreenEvents{} && settle.quiet >= quiet;
}

Pokemon::ScreenSettle wait_for_quiet_overworld(
    const Pokemon::ScreenLookFunction& look, Milliseconds quiet, Milliseconds timeout
){
    Pokemon::ScreenLook seen = look(true);
    Pokemon::ScreenSettle settle = Pokemon::judge_settle(seen);
    const size_t max_looks = static_cast<size_t>(timeout / BETWEEN_STEPS_LOOK) + 1;
    for (size_t looks = 1; looks < max_looks && !quiet_overworld(settle, quiet); looks++){
        if (seen.window.end - seen.window.start >= timeout){
            break;
        }
        seen = look(false);
        settle = Pokemon::judge_settle(seen);
    }
    return settle;
}


void InterruptLogGuard::on_failure(const StoryStep& step, const OperationFailedException& error){
    save_frame(step.name, error);
}

void InterruptLogGuard::save_frame(std::string_view name, const OperationFailedException& error){
    const OperationFailedExceptionWithScreenshot* shot = dynamic_cast<const OperationFailedExceptionWithScreenshot*>(&error);
    const std::shared_ptr<const ImageRGB32> frame = shot == nullptr ? nullptr : shot->screenshot();
    if (!frame || !*frame){
        m_logger.log(std::format("Story: {} failed with no frame to save.", name), COLOR_ORANGE);
        return;
    }
    Filesystem::create_directories(Filesystem::Path(m_failures_folder));
    const std::string path = std::format("{}{}-{}.png", m_failures_folder, now_to_filestring(), name);
    if (frame->save(path)){
        m_logger.log(std::format("Failure screenshot: {}", path), COLOR_RED);
    }else{
        m_logger.log(std::format("Story: the failure's frame didn't save to {}", path), COLOR_RED);
    }
}

void InterruptLogGuard::on_interrupt(const StoryStep& step, const Pokemon::InterruptCheck& check){
    const bool cleared = !check.failure;
    m_logger.log(std::format(
        "Story: {}: before it, {}: {}", step.name,
        Pokemon::interrupt_kind_name(check.verdict.kind),
        cleared ? std::string("cleared") : std::format("{}", *check.failure)
    ), cleared ? COLOR_ORANGE : COLOR_RED);
}

void InterruptLogGuard::on_screen(const StoryStep& step, const Pokemon::ScreenSettle& screen){
    m_logger.log(std::format("[Screen] before {}: {}", step.name, Pokemon::describe(screen)));
}

void InterruptLogGuard::on_check_skipped(const StoryStep&, const std::string& line){
    m_logger.log(std::format("Story: {}", line));
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
