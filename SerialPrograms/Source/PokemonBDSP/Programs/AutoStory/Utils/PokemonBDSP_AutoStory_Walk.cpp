/*  BDSP AutoStory - Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BattleDialogWatcher adapted from PokemonAutomation/Arduino-Source#1717 by
 *  Developer-Butters.
 */

#include <algorithm>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_TransitionWatchers.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
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

//  The battle's text box, held for 250ms.
class BattleDialogWatcher : public DetectorToFinder<BattleDialogDetector>{
public:
    explicit BattleDialogWatcher(Color color)
        : DetectorToFinder("BattleDialogWatcher", std::chrono::milliseconds(250), color)
    {}
};

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

//  A grass encounter opens with a leaf animation, neither white nor black, and
//  shows the battle's text box a few seconds later. After a change of the
//  whole screen that walking cannot make, wait for the battle to show.
bool battle_follows(VideoStream& stream, ProControllerContext& context){
    BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
    BattleDialogWatcher battle_dialog(COLOR_YELLOW);
    return wait_until(stream, context, 5s, {{battle_menu}, {battle_dialog}}) >= 0;
}

}

//  A step in tall grass that was started before the stick was released still
//  finishes, and its encounter starts after the walk has ended. Watch a little
//  longer, and run from it. Returns whether there was one.
bool run_from_late_encounter(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const std::string& label){
    VideoSnapshot before = stream.video().snapshot();
    WhiteScreenOverWatcher encounter_flash(COLOR_YELLOW, {0.1, 0.1, 0.8, 0.8});
    BlackScreenWatcher black(COLOR_RED);
    BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
    BattleDialogWatcher battle_dialog(COLOR_YELLOW);
    const int ret = wait_until(stream, context, 1200ms, {{encounter_flash}, {black}, {battle_menu}, {battle_dialog}});
    bool battle = ret == 0 || ret == 2 || ret == 3 || (ret == 1 && black_screen_was_battle(stream, context));
    if (ret < 0){
        VideoSnapshot after = stream.video().snapshot();
        battle = before && after && walk_screen_change(*before.frame, *after.frame) > WALK_SCENE_CHANGE &&
            battle_follows(stream, context);
    }
    if (!battle){
        return false;
    }
    stream.log("[Walk] " + label + ": wild battle after the walk; running.");
    escape(stream, context);
    context.wait_for_all_requests();
    return true;
}
namespace{

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


namespace{

//  What a walk presses.
enum class WalkInput{
    STICK,
    DPAD,
    DPAD_RUN,   //  the D-pad with B held
    TAPS,       //  a D-pad tap a tile, 800 ms each
};
//  What a battle that starts on the way is.
enum class WildPolicy{
    RUN_FROM,   //  a wild battle: run, and walk on
    STOP_AT,    //  no wild Pokemon here: a trainer's, so the walk stops with DIALOG
};

struct WalkSpec{
    JoystickPosition stick;
    Milliseconds limit = 0ms;
    std::string label;
    WalkArrived arrived = nullptr;
    WalkInput input = WalkInput::STICK;
    WildPolicy wild = WildPolicy::RUN_FROM;
};

//  What a chunk of the walk decided: stop with `stop`, walk the next chunk,
//  or (after the watchers) look at how the screen changed.
enum class ChunkNext{
    STOP,
    NEXT_CHUNK,
    LOOK,
};
struct ChunkVerdict{
    ChunkNext next = ChunkNext::LOOK;
    WalkStop stop = WalkStop::LIMIT;
};

//  walk_until() and its variants: holds the input a chunk at a time, and
//  after each chunk reads the watchers, then the screen.
class WalkUntil{
public:
    WalkUntil(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const WalkSpec& spec)
        : m_stream(stream)
        , m_context(context)
        , m_escape(escape)
        , m_spec(spec)
        , m_dpad(spec.stick.x < 0 ? DPAD_LEFT : spec.stick.x > 0 ? DPAD_RIGHT : spec.stick.y > 0 ? DPAD_UP : DPAD_DOWN)
    {}

    WalkResult run(){
        //  A tile a tap: 160 ms held, the rest released, so each chunk is one tile.
        const Milliseconds CHUNK = m_spec.input == WalkInput::TAPS ? 800ms : 400ms;
        m_context.wait_for_all_requests();
        m_before = snapshot_or_fail(m_stream, m_spec.label);
        while (true){
            if (m_spec.arrived && m_spec.arrived(*m_before.frame)){
                return finish(WalkStop::ARRIVED);
            }
            if (m_result.walked >= m_spec.limit){
                return finish(WalkStop::LIMIT);
            }
            const Milliseconds chunk = std::min(CHUNK, m_spec.limit - m_result.walked);
            const int ret = hold(chunk);
            m_result.walked += chunk;
            ChunkVerdict verdict = after_watchers(ret);
            if (verdict.next == ChunkNext::LOOK){
                verdict = look();
            }
            if (verdict.next == ChunkNext::STOP){
                return finish(verdict.stop);
            }
        }
    }

private:
    int hold(Milliseconds chunk){
        WhiteScreenOverWatcher encounter_flash(COLOR_YELLOW, {0.1, 0.1, 0.8, 0.8});
        BlackScreenWatcher black(COLOR_RED);
        BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
        ShortDialogWatcher dialog(COLOR_YELLOW);
        BattleDialogWatcher battle_dialog(COLOR_YELLOW);
        return run_until<ProControllerContext>(
            m_stream, m_context,
            [&](ProControllerContext& context){
                press(context, chunk);
            },
            {{encounter_flash}, {black}, {battle_menu}, {dialog}, {battle_dialog}}
        );
    }

    void press(ProControllerContext& context, Milliseconds chunk) const{
        switch (m_spec.input){
        case WalkInput::TAPS:
            pbf_press_dpad(context, m_dpad, 160ms, chunk - 160ms);
            return;
        case WalkInput::DPAD_RUN:
            ssf_press_button(context, BUTTON_B, 0ms, chunk, 0ms);
            ssf_press_dpad(context, m_dpad, chunk, chunk, 0ms);
            return;
        case WalkInput::DPAD:
            pbf_press_dpad(context, m_dpad, chunk, 0ms);
            return;
        case WalkInput::STICK:
            pbf_move_left_joystick(context, m_spec.stick, chunk, 0ms);
            return;
        }
    }

    //  The watchers: 0 the encounter flash, 1 black, 2 the battle menu, 3 a
    //  dialogue, 4 the battle's text box.
    ChunkVerdict after_watchers(int ret){
        bool battle = ret == 0 || ret == 2 || ret == 4;
        if (ret == 1){
            battle = black_screen_was_battle(m_stream, m_context);
            if (!battle){
                return {ChunkNext::STOP, WalkStop::TRANSITION};
            }
        }
        if (battle && m_spec.wild == WildPolicy::STOP_AT){
            m_stream.log("[Walk] " + m_spec.label + ": a battle started; a trainer's here.");
            return {ChunkNext::STOP, WalkStop::DIALOG};
        }
        if (battle){
            escape_battle("wild battle; running.");
            return {ChunkNext::NEXT_CHUNK, WalkStop::LIMIT};
        }
        if (ret == 3){
            return {ChunkNext::STOP, WalkStop::DIALOG};
        }
        return {ChunkNext::LOOK, WalkStop::LIMIT};
    }

    ChunkVerdict look(){
        m_context.wait_for_all_requests();
        VideoSnapshot after = snapshot_or_fail(m_stream, m_spec.label);
        const double change = walk_screen_change(*m_before.frame, *after.frame);
        m_stream.log("[Walk] " + m_spec.label + ": " + std::to_string(m_result.walked.count()) +
            "ms, screen change " + std::to_string(change) + ".");
        if (change > WALK_SCENE_CHANGE && m_spec.wild == WildPolicy::STOP_AT && battle_follows(m_stream, m_context)){
            m_stream.log("[Walk] " + m_spec.label + ": a battle started; a trainer's here.");
            return {ChunkNext::STOP, WalkStop::DIALOG};
        }
        if (change > WALK_SCENE_CHANGE && battle_follows(m_stream, m_context)){
            escape_battle("wild battle (leaf opening); running.");
            return {ChunkNext::NEXT_CHUNK, WalkStop::LIMIT};
        }
        //  Two still chunks in a row, so one dropped frame does not stop us. A
        //  frame exactly the same is a stalled video or a game hitch, not a wall:
        //  standing at a wall still animates.
        if (change == 0){
            m_before = std::move(after);
            return {ChunkNext::NEXT_CHUNK, WalkStop::LIMIT};
        }
        const bool still = still_chunk(change);
        if (!still){
            m_result.moved_chunks++;
        }
        m_unchanged = still ? m_unchanged + 1 : 0;
        if (m_unchanged >= 2){
            return {ChunkNext::STOP, WalkStop::BLOCKED};
        }
        m_before = std::move(after);
        return {ChunkNext::NEXT_CHUNK, WalkStop::LIMIT};
    }

    //  The largest change a chunk of this walk made is a step. A bump into an
    //  NPC measured 17.2 after steps of 63 to 65 (r250a, Floaroma's old
    //  woman), over the fixed line of 16, so for taps "still" is also
    //  anything under 40% of the walk's own steps.
    bool still_chunk(double change){
        m_largest = std::max(m_largest, change);
        const double line = m_spec.input == WalkInput::TAPS
            ? std::max(WALK_TAP_BLOCKED_CHANGE, 0.4 * m_largest)
            : WALK_BLOCKED_CHANGE;
        return change < line;
    }

    void escape_battle(const std::string& what){
        m_result.wild_battles++;
        m_stream.log("[Walk] " + m_spec.label + ": " + what);
        m_escape(m_stream, m_context);
        m_context.wait_for_all_requests();
        m_before = snapshot_or_fail(m_stream, m_spec.label);
        m_unchanged = 0;
    }

    WalkResult finish(WalkStop stop){
        m_result.stop = stop;
        if (m_spec.wild == WildPolicy::RUN_FROM && (stop == WalkStop::LIMIT || stop == WalkStop::BLOCKED) &&
            run_from_late_encounter(m_stream, m_context, m_escape, m_spec.label)
        ){
            m_result.wild_battles++;
        }
        m_stream.log("[Walk] " + m_spec.label + ": " + walk_stop_name(stop) + " after " +
            std::to_string(m_result.walked.count()) + "ms, " +
            std::to_string(m_result.wild_battles) + " wild battle(s).");
        return m_result;
    }

    VideoStream& m_stream;
    ProControllerContext& m_context;
    const WildBattleEscape& m_escape;
    const WalkSpec& m_spec;
    const DpadPosition m_dpad;
    WalkResult m_result{WalkStop::LIMIT, 0ms, 0};
    VideoSnapshot m_before;
    size_t m_unchanged = 0;
    double m_largest = 0;
};

WalkResult walk_until_spec(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const WalkSpec& spec
){
    return WalkUntil(stream, context, escape, spec).run();
}

}

WalkResult walk_until(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label,
    const WalkArrived& arrived
){
    return walk_until_spec(stream, context, escape, {
        .stick = stick, .limit = limit, .label = label, .arrived = arrived,
        .input = WalkInput::STICK, .wild = WildPolicy::RUN_FROM,
    });
}

WalkResult walk_until_dpad(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label,
    const DpadWalkOptions& options
){
    return walk_until_spec(stream, context, escape, {
        .stick = stick, .limit = limit, .label = label, .arrived = options.arrived,
        .input = options.pace == DpadPace::RUN ? WalkInput::DPAD_RUN : WalkInput::DPAD,
        .wild = WildPolicy::RUN_FROM,
    });
}

WalkResult walk_tiles_dpad(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, size_t tiles, const std::string& label
){
    return walk_until_spec(stream, context, escape, {
        .stick = stick, .limit = 800ms * tiles, .label = label, .arrived = nullptr,
        .input = WalkInput::TAPS, .wild = WildPolicy::RUN_FROM,
    });
}

WalkResult walk_until_no_wild(
    VideoStream& stream, ProControllerContext& context,
    JoystickPosition stick, Milliseconds limit, const std::string& label
){
    //  With no wild Pokemon any battle is a trainer's, and the walk stops at
    //  it (DIALOG) without running.
    const WildBattleEscape no_wild = [](VideoStream& stream, ProControllerContext&){
        throw InternalProgramError(&stream.logger(), PA_CURRENT_FUNCTION, "walk_until_no_wild() ran from a battle.");
    };
    return walk_until_spec(stream, context, no_wild, {
        .stick = stick, .limit = limit, .label = label, .arrived = nullptr,
        .input = WalkInput::STICK, .wild = WildPolicy::STOP_AT,
    });
}

WalkResult walk_for(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds duration, const std::string& label
){
    WalkResult result{WalkStop::LIMIT, duration, 0};
    context.wait_for_all_requests();
    WhiteScreenOverWatcher encounter_flash(COLOR_YELLOW, {0.1, 0.1, 0.8, 0.8});
    BlackScreenWatcher black(COLOR_RED);
    BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
    ShortDialogWatcher dialog(COLOR_YELLOW);
    BattleDialogWatcher battle_dialog(COLOR_YELLOW);
    const int ret = run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            pbf_move_left_joystick(context, stick, duration, 0ms);
        },
        {{encounter_flash}, {black}, {battle_menu}, {dialog}, {battle_dialog}}
    );
    bool battle = ret == 0 || ret == 2 || ret == 4;
    if (ret == 1){
        battle = black_screen_was_battle(stream, context);
        if (!battle){
            result.stop = WalkStop::TRANSITION;
        }
    }
    if (battle){
        result.wild_battles = 1;
        stream.log("[Walk] " + label + ": wild battle; running.");
        escape(stream, context);
        context.wait_for_all_requests();
    }
    if (ret == 3){
        result.stop = WalkStop::DIALOG;
    }
    //  An encounter after the hold leaves us where the walk ended, so it does
    //  not count as cutting the walk short.
    if (ret < 0){
        run_from_late_encounter(stream, context, escape, label);
    }
    stream.log("[Walk] " + label + ": " + walk_stop_name(result.stop) + " after a " +
        std::to_string(duration.count()) + "ms hold, " + std::to_string(result.wild_battles) + " wild battle(s).");
    return result;
}

WalkResult walk_to_talk(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label
){
    WalkResult total{WalkStop::LIMIT, 0ms, 0};
    while (total.walked < limit){
        const WalkResult step = walk_until(stream, context, escape, stick,
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
    JoystickPosition stick, Milliseconds limit,
    double moving_change, const std::string& label
){
    WalkProbe result;
    context.wait_for_all_requests();
    WalkProbeSampler sampler(result, moving_change, current_time());
    run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            pbf_move_left_joystick(context, stick, limit, 0ms);
        },
        {{sampler}}
    );
    context.wait_for_all_requests();
    stream.log("[Walk] " + label + ": probe moved from " + std::to_string(result.first_move.count()) +
        "ms to " + std::to_string(result.last_move.count()) + "ms.");
    return result;
}

WalkResult walk_to_wall(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label
){
    const WalkResult result = walk_until(stream, context, escape, stick, limit, label);
    if (result.stop == WalkStop::DIALOG || result.stop == WalkStop::TRANSITION){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": " + walk_stop_name(result.stop) + " instead of a wall.", stream);
    }
    if (result.stop == WalkStop::LIMIT){
        stream.log("[Walk] " + label + ": held for the whole limit without seeing the stop; taking the wall as reached.");
    }
    return result;
}

WalkResult walk_expecting(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    JoystickPosition stick, Milliseconds limit, const std::string& label,
    const WalkExpectation& expected
){
    const WalkResult result = walk_until(stream, context, escape, stick, limit, label, expected.arrived);
    if (result.stop != expected.stop){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": expected " + walk_stop_name(expected.stop) + ", got " +
            walk_stop_name(result.stop) + ".", stream);
    }
    return result;
}


}
}
}
