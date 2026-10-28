/*  BDSP AutoStory - Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_TransitionWatchers.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP_AutoStory_Segment_00_tools.h"
#include "PokemonBDSP_Autostory_Segment_01_tools.h"
#include "PokemonBDSP_AutoStory_Walk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


const char* walk_stop_name(WalkStop stop){
    switch (stop){
    case WalkStop::ARRIVED:     return "arrived";
    case WalkStop::DIALOG:      return "dialog";
    case WalkStop::TRANSITION:  return "transition";
    case WalkStop::BLOCKED:     return "blocked";
    case WalkStop::LIMIT:       return "limit";
    }
    return "?";
}

double walk_screen_change(const ImageViewRGB32& before, const ImageViewRGB32& after){
    //  Small copies: only the whole-view scroll matters, not detail.
    return ImageMatch::pixel_RMSD(before.scale_to(192, 108), after.scale_to(192, 108));
}


namespace{

VideoSnapshot snapshot_or_fail(VideoStream& stream, const std::string& label){
    VideoSnapshot frame = stream.video().snapshot();
    if (!frame){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": No video frame.", stream);
    }
    return frame;
}

//  After a black screen: a wild battle if a battle screen shows up, else a transition.
bool black_screen_was_battle(VideoStream& stream, ProControllerContext& context){
    BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
    BattleDialogWatcher battle_dialog(COLOR_YELLOW);
    BlackScreenWatcher gone(COLOR_RED, {0.1, 0.1, 0.8, 0.8}, 100, 10,
        BlackScreenWatcher::FinderType::GONE, 500ms);
    const int ret = wait_until(stream, context, 10s, {{battle_menu}, {battle_dialog}, {gone}});
    if (ret == 2){
        //  The battle intro also starts from black; give its first screen a moment.
        BattleMenuWatcher late_menu(BattleType::STANDARD, COLOR_YELLOW);
        BattleDialogWatcher late_dialog(COLOR_YELLOW);
        return wait_until(stream, context, 2s, {{late_menu}, {late_dialog}}) >= 0;
    }
    return ret >= 0;
}

//  A step in tall grass that was started before the stick was released still
//  finishes, and its encounter starts after the walk has ended. Watch a little
//  longer, and run from it. Returns whether there was one.
bool run_from_late_encounter(VideoStream& stream, ProControllerContext& context, const std::string& label){
    WhiteScreenOverWatcher encounter_flash(COLOR_YELLOW, {0.1, 0.1, 0.8, 0.8});
    BlackScreenWatcher black(COLOR_RED);
    BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
    const int ret = wait_until(stream, context, 1200ms, {{encounter_flash}, {black}, {battle_menu}});
    if (ret < 0 || (ret == 1 && !black_screen_was_battle(stream, context))){
        return false;
    }
    stream.log("[Walk] " + label + ": wild battle after the walk; running.");
    escape_wild_battle(stream, context, BattleType::STANDARD);
    context.wait_for_all_requests();
    return true;
}

//  Samples the screen every 100ms for probe_walk() and stops it once the view
//  has been still for 800ms.
class WalkProbeSampler : public VisualInferenceCallback{
public:
    WalkProbeSampler(WalkProbe& result, double moving_change, WallClock start)
        : VisualInferenceCallback("WalkProbeSampler")
        , m_result(result)
        , m_moving_change(moving_change)
        , m_start(start)
    {}
    virtual void make_overlays(VideoOverlaySet&) const override{}
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override{
        if (m_has_last && timestamp - m_last_time < 100ms){
            return false;
        }
        const Milliseconds at = std::chrono::duration_cast<Milliseconds>(timestamp - m_start);
        ImageRGB32 small = frame.scale_to(192, 108);
        if (m_has_last){
            const double change = ImageMatch::pixel_RMSD(m_last, small);
            m_result.samples.push_back({at, change});
            if (change >= m_moving_change){
                if (m_result.first_move < 0ms){
                    m_result.first_move = at;
                }
                m_result.last_move = at;
            }
        }
        m_last = std::move(small);
        m_last_time = timestamp;
        m_has_last = true;
        const Milliseconds still_since = m_result.last_move < 0ms ? 0ms : m_result.last_move;
        return at - still_since >= 800ms;
    }

private:
    WalkProbe& m_result;
    double m_moving_change;
    WallClock m_start;
    ImageRGB32 m_last;
    WallClock m_last_time;
    bool m_has_last = false;
};

}


WalkResult walk_until(
    VideoStream& stream, ProControllerContext& context,
    double x, double y, Milliseconds limit, const std::string& label,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived
){
    constexpr Milliseconds CHUNK = 400ms;
    WalkResult result{WalkStop::LIMIT, 0ms, 0};
    size_t unchanged = 0;

    context.wait_for_all_requests();
    VideoSnapshot before = snapshot_or_fail(stream, label);
    auto finish = [&](WalkStop stop){
        result.stop = stop;
        if ((stop == WalkStop::LIMIT || stop == WalkStop::BLOCKED) &&
            run_from_late_encounter(stream, context, label)
        ){
            result.wild_battles++;
        }
        stream.log("[Walk] " + label + ": " + walk_stop_name(stop) + " after " +
            std::to_string(result.walked.count()) + "ms, " +
            std::to_string(result.wild_battles) + " wild battle(s).");
        return result;
    };

    while (true){
        if (arrived && arrived(*before.frame)){
            return finish(WalkStop::ARRIVED);
        }
        if (result.walked >= limit){
            return finish(WalkStop::LIMIT);
        }
        const Milliseconds chunk = std::min(CHUNK, limit - result.walked);

        WhiteScreenOverWatcher encounter_flash(COLOR_YELLOW, {0.1, 0.1, 0.8, 0.8});
        BlackScreenWatcher black(COLOR_RED);
        BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
        ShortDialogWatcher dialog(COLOR_YELLOW);
        const int ret = run_until<ProControllerContext>(
            stream, context,
            [&](ProControllerContext& context){
                pbf_move_left_joystick(context, {x, y}, chunk, 0ms);
            },
            {{encounter_flash}, {black}, {battle_menu}, {dialog}}
        );
        result.walked += chunk;

        bool battle = ret == 0 || ret == 2;
        if (ret == 1){
            battle = black_screen_was_battle(stream, context);
            if (!battle){
                return finish(WalkStop::TRANSITION);
            }
        }
        if (battle){
            result.wild_battles++;
            stream.log("[Walk] " + label + ": wild battle; running.");
            escape_wild_battle(stream, context, BattleType::STANDARD);
            context.wait_for_all_requests();
            before = snapshot_or_fail(stream, label);
            unchanged = 0;
            continue;
        }
        if (ret == 3){
            return finish(WalkStop::DIALOG);
        }

        context.wait_for_all_requests();
        VideoSnapshot after = snapshot_or_fail(stream, label);
        const double change = walk_screen_change(*before.frame, *after.frame);
        stream.log("[Walk] " + label + ": " + std::to_string(result.walked.count()) +
            "ms, screen change " + std::to_string(change) + ".");
        //  Two still chunks in a row, so one dropped frame does not stop us. A
        //  frame exactly the same is a stalled video or a game hitch, not a wall:
        //  standing at a wall still animates.
        if (change == 0){
            before = std::move(after);
            continue;
        }
        unchanged = change < WALK_BLOCKED_CHANGE ? unchanged + 1 : 0;
        if (unchanged >= 2){
            return finish(WalkStop::BLOCKED);
        }
        before = std::move(after);
    }
}

WalkResult walk_for(
    VideoStream& stream, ProControllerContext& context,
    double x, double y, Milliseconds duration, const std::string& label
){
    WalkResult result{WalkStop::LIMIT, duration, 0};
    context.wait_for_all_requests();
    WhiteScreenOverWatcher encounter_flash(COLOR_YELLOW, {0.1, 0.1, 0.8, 0.8});
    BlackScreenWatcher black(COLOR_RED);
    BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
    ShortDialogWatcher dialog(COLOR_YELLOW);
    const int ret = run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            pbf_move_left_joystick(context, {x, y}, duration, 0ms);
        },
        {{encounter_flash}, {black}, {battle_menu}, {dialog}}
    );
    bool battle = ret == 0 || ret == 2;
    if (ret == 1){
        battle = black_screen_was_battle(stream, context);
        if (!battle){
            result.stop = WalkStop::TRANSITION;
        }
    }
    if (battle){
        result.wild_battles = 1;
        stream.log("[Walk] " + label + ": wild battle; running.");
        escape_wild_battle(stream, context, BattleType::STANDARD);
        context.wait_for_all_requests();
    }
    if (ret == 3){
        result.stop = WalkStop::DIALOG;
    }
    //  An encounter after the hold leaves us where the walk ended, so it does
    //  not count as cutting the walk short.
    if (ret < 0){
        run_from_late_encounter(stream, context, label);
    }
    stream.log("[Walk] " + label + ": " + walk_stop_name(result.stop) + " after a " +
        std::to_string(duration.count()) + "ms hold, " + std::to_string(result.wild_battles) + " wild battle(s).");
    return result;
}

WalkResult walk_to_talk(
    VideoStream& stream, ProControllerContext& context,
    double x, double y, Milliseconds limit, const std::string& label
){
    WalkResult total{WalkStop::LIMIT, 0ms, 0};
    while (total.walked < limit){
        const WalkResult step = walk_until(stream, context, x, y,
            std::min<Milliseconds>(400ms, limit - total.walked), label);
        total.walked += step.walked;
        total.wild_battles += step.wild_battles;
        if (step.stop == WalkStop::DIALOG || step.stop == WalkStop::TRANSITION){
            total.stop = step.stop;
            return total;
        }
        ShortDialogWatcher dialog(COLOR_YELLOW);
        const int ret = run_until<ProControllerContext>(
            stream, context,
            [](ProControllerContext& context){
                pbf_press_button(context, BUTTON_A, 80ms, 720ms);
            },
            {{dialog}}
        );
        if (ret == 0){
            total.stop = WalkStop::DIALOG;
            stream.log("[Walk] " + label + ": talking after " + std::to_string(total.walked.count()) + "ms.");
            return total;
        }
    }
    stream.log("[Walk] " + label + ": nobody to talk to within " + std::to_string(limit.count()) + "ms.");
    return total;
}

WalkProbe probe_walk(
    VideoStream& stream, ProControllerContext& context,
    double x, double y, Milliseconds limit,
    double moving_change, const std::string& label
){
    WalkProbe result;
    context.wait_for_all_requests();
    WalkProbeSampler sampler(result, moving_change, current_time());
    run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            pbf_move_left_joystick(context, {x, y}, limit, 0ms);
        },
        {{sampler}}
    );
    context.wait_for_all_requests();
    stream.log("[Walk] " + label + ": probe moved from " + std::to_string(result.first_move.count()) +
        "ms to " + std::to_string(result.last_move.count()) + "ms.");
    return result;
}

WalkResult walk_expecting(
    VideoStream& stream, ProControllerContext& context,
    double x, double y, Milliseconds limit, const std::string& label,
    WalkStop expected,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived
){
    const WalkResult result = walk_until(stream, context, x, y, limit, label, arrived);
    if (result.stop != expected){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": expected " + walk_stop_name(expected) + ", got " +
            walk_stop_name(result.stop) + ".", stream);
    }
    return result;
}


}
}
}
