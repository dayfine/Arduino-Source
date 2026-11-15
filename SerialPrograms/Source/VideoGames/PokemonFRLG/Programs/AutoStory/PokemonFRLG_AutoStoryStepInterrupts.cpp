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

namespace{

//  Whether `step`'s first input is A on what the player faces.
bool presses_a_first(const Pokemon::StoryStep& step){
    if (step.kind != StoryStepKind::GAME){
        return false;
    }
    return step.action == CUT_TREE_VARIANT || step.action == PUSH_BOULDER_VARIANT
        || step.action == POSTER_SWITCH_VARIANT || parse_surf_step(step).has_value();
}

} // namespace

std::string clear_box_before_a(
    const Pokemon::StoryStep& step,
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
    //  The overworld after the box, quiet as after a battle: its next input
    //  is the step's A.
    const Pokemon::SettleEnd after = wait_for_quiet_overworld(look, step.name, BATTLE_EXIT_TIMEOUT);
    return std::format("Story: box open before {}: cleared with B, then {}.", step.name, describe(after));
}

Pokemon::StoryActionFunction clearing_box_first(
    Pokemon::StoryActionFunction act, const Pokemon::StoryInterrupts& interrupts, Logger& logger
){
    return [act = std::move(act), &interrupts, &logger](const Pokemon::StoryStep& step, Pokemon::RunState& state){
        const Pokemon::InterruptHandler* dialog = interrupts.handlers.find(InterruptKind::DIALOG);
        const std::string cleared = clear_box_before_a(step, interrupts.look, [&]{
            if (dialog != nullptr && dialog->handle){
                Pokemon::InterruptVerdict verdict;
                verdict.kind = InterruptKind::DIALOG;
                dialog->handle(verdict, state);
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
        return [&console, &context, walker, wild](const Pokemon::InterruptVerdict&, Pokemon::RunState&){
            const OverworldInterruptSet set = walker();
            (wild ? set.wild : set.other).front()->clear(console, context);
            return Pokemon::outcome_ok();
        };
    };
    return StepInterruptActions{
        .wild = walker_clears(true),
        .trainer = walker_clears(false),
        .dialog = walker_clears(false),
        .warp = [&console, &context](const Pokemon::InterruptVerdict&, Pokemon::RunState&){
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
    return settle.seen && settle.showing == QUIET_OVERWORLD && settle.quiet >= quiet;
}

Pokemon::SettleRule quiet_overworld_rule(Milliseconds cap){
    Pokemon::SettleRule rule = Pokemon::settle_rule(Pokemon::SettleKind::DOOR);
    rule.look_every = BETWEEN_STEPS_LOOK;
    rule.cap = cap;
    return rule;
}

namespace{

//  run_settle()'s looks on FRLG: each watches BETWEEN_STEPS_LOOK, the
//  first fresh, and ends on what its last frame showed.
class WatchingSettleIO : public Pokemon::SettleIO{
public:
    explicit WatchingSettleIO(const Pokemon::ScreenLookFunction& look)
        : m_look(look)
    {}

    virtual Pokemon::SettleLook look() override{
        const Pokemon::ScreenLook seen = m_look(std::exchange(m_fresh, false));
        return {.events = seen.log.showing(), .when = seen.window.end};
    }
    //  No button: the next look's watch is the wait.
    virtual void press(Pokemon::SceneMove, Milliseconds) override{}
    //  The battle's own loop ended it: what still shows is waited out.
    virtual bool flee_battle() override{
        return true;
    }
    //  The caller logs how it ended.
    virtual void log(std::string_view) override{}

private:
    const Pokemon::ScreenLookFunction& m_look;
    bool m_fresh = true;
};

} // namespace

Pokemon::SettleEnd wait_for_quiet_overworld(
    const Pokemon::ScreenLookFunction& look, std::string_view label, Milliseconds timeout
){
    WatchingSettleIO io(look);
    return Pokemon::run_settle(quiet_overworld_rule(timeout), label, io);
}

std::string describe(const Pokemon::SettleEnd& end){
    switch (end.result){
    case Pokemon::SettleEnd::Result::QUIET:
        return "quiet overworld";
    case Pokemon::SettleEnd::Result::BATTLE:
        return "a battle";
    case Pokemon::SettleEnd::Result::TIMED_OUT:
        break;
    }
    return end.text;
}


void InterruptLogGuard::on_failure(const Pokemon::StoryStep& step, const OperationFailedException& error){
    const OperationFailedExceptionWithScreenshot* shot = dynamic_cast<const OperationFailedExceptionWithScreenshot*>(&error);
    const std::shared_ptr<const ImageRGB32> frame = shot == nullptr ? nullptr : shot->screenshot();
    if (!frame || !*frame){
        m_logger.log(std::format("Story: {} failed with no frame to save.", step.name), COLOR_ORANGE);
        return;
    }
    Filesystem::create_directories(Filesystem::Path(m_failures_folder));
    const std::string path = std::format("{}{}-{}.png", m_failures_folder, now_to_filestring(), step.name);
    if (frame->save(path)){
        m_logger.log(std::format("Failure screenshot: {}", path), COLOR_RED);
    }else{
        m_logger.log(std::format("Story: the failure's frame didn't save to {}", path), COLOR_RED);
    }
}

void InterruptLogGuard::on_interrupt(const Pokemon::StoryStep& step, const Pokemon::InterruptCheck& check){
    const bool cleared = check.outcome.succeeded();
    m_logger.log(std::format(
        "Story: {}: before it, {}: {}", step.name,
        Pokemon::interrupt_kind_name(check.verdict.kind), cleared ? "cleared" : Pokemon::to_string(check.outcome)
    ), cleared ? COLOR_ORANGE : COLOR_RED);
}

void InterruptLogGuard::on_screen(const Pokemon::StoryStep& step, const Pokemon::ScreenSettle& screen){
    m_logger.log(std::format("[Screen] before {}: {}", step.name, Pokemon::describe(screen)));
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
