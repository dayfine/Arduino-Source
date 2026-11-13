/*  BDSP AutoStory - Talk-over
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_ScreenEvents.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_TalkOver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


TalkOverEnd talk_over(VideoStream& stream, ProControllerContext& context, const std::string& label, const TalkOverSpec& spec){
    return talk_over_seen(stream, context, label, spec).end;
}

TalkOverSeen talk_over_seen(VideoStream& stream, ProControllerContext& context, const std::string& label, const TalkOverSpec& spec){
    const WallClock deadline = current_time() + TALK_OVER_LIMIT;
    TalkOverTally judge(spec.rule);
    BdspScreenObserver eyes(spec.watch == TalkOverWatch::TRAINER ? BattleCue::TRAINER : BattleCue::WILD_MENU, spec.banner);
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const WallClock now = current_time();
        switch (judge.look(eyes.observe(stream.video().snapshot(), now), now)){
        case SceneMove::DONE:
            stream.log(std::format("[Talk-over] {}: over; {}; a box in {} look(s).", label, judge.describe(), judge.box_looks()));
            return {.end = TalkOverEnd::QUIET, .box_looks = judge.box_looks()};
        case SceneMove::BATTLE:
            if (spec.watch == TalkOverWatch::TRAINER){
                stream.log("[Talk-over] " + label + ": a battle started.");
                return {.end = TalkOverEnd::BATTLE, .box_looks = judge.box_looks()};
            }
            stream.log("[Talk-over] " + label + ": in a wild battle; fleeing.");
            run_from_wild_battle(stream, context);
            continue;
        case SceneMove::NEXT:
            pbf_press_button(context, spec.next, TALK_OVER_HOLD, talk_over_release(spec.rule));
            continue;
        case SceneMove::B:
            pbf_press_button(context, BUTTON_B, TALK_OVER_HOLD, talk_over_release(spec.rule));

            continue;
        }
    }
    const long long seconds = std::chrono::duration_cast<std::chrono::seconds>(TALK_OVER_LIMIT).count();
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        !judge.warped()
            ? std::format("{}: no fade or banner within {} s, so we are not where the step goes (map mismatch).", label, seconds)
            : !spec.rule.end_on_quiet
            ? std::format("{}: no battle within {} s of the talk.", label, seconds)
            : std::format("{}: not back on a quiet overworld within {} s.", label, seconds),
        stream);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
