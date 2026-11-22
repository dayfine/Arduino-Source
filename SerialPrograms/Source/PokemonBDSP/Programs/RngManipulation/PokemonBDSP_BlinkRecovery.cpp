/*  BDSP Blink Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <utility>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Recording/StreamHistoryOption.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceSession.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP_BlinkRecovery.h"
#include "PokemonBDSP_BlinkWindowSolver.h"
#include "PokemonBDSP_StateReidentifier.h"
#include "PokemonBDSP_StateSolver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace Pokemon;
using namespace std::chrono_literals;

namespace{

void warn_blink_capture_health(SingleSwitchProgramEnvironment& env){
    if (GlobalSettings::instance().STREAM_HISTORY->enabled()){
        env.log(
            "Stream History is enabled; periodic frame encoding can stall capture and slow blink recovery.",
            COLOR_ORANGE
        );
    }
    double fps = env.console.video().fps_source();
    if (fps > 0 && fps < 28.0){
        env.log(
            std::format(
                "Capture is running at {:.1f} fps (below 28 fps); frame drops can hide double blinks.",
                fps
            ),
            COLOR_ORANGE
        );
    }
}

} // namespace


std::vector<ImageRGB32> load_eye_templates(
    const std::vector<BdspEyeTemplate>& setups
){
    std::vector<ImageRGB32> eyes;
    eyes.reserve(setups.size());
    for (const BdspEyeTemplate& setup : setups){
        eyes.emplace_back(RESOURCE_PATH() + "PokemonBDSP/Rng/" + setup.asset);
    }
    return eyes;
}


static bool blink_scene_ready(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    std::vector<EyeBlinkDetector>& detectors,
    const BlinkSceneConfig& config
){
    WallClock deadline = current_time() + config.attempt_timeout;
    size_t streak = 0;
    double best_seen = -1;
    while (current_time() < deadline){
        pbf_wait(context, 250ms);
        context.wait_for_all_requests();

        VideoSnapshot frame = env.console.video().snapshot();
        if (config.frame_filter && !config.frame_filter(frame)){
            streak = 0;
            continue;
        }
        double worst = 1.0;
        for (const EyeBlinkDetector& detector : detectors){
            worst = std::min(worst, detector.match(frame));
        }
        best_seen = std::max(best_seen, worst);
        if (worst < config.minimum_match){
            streak = 0;
            continue;
        }
        if (++streak >= config.required_streak){
            env.log("Characters in position, worst eye match "
                + tostr_default(worst) + ".");
            return true;
        }
    }
    env.log("Not settled yet; best worst-eye match was " + tostr_default(best_seen) + ".");
    return false;
}

bool wait_for_blink_scene(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const std::vector<BdspEyeTemplate>& setups,
    const std::vector<ImageRGB32>& eyes,
    const BlinkSceneConfig& config,
    const std::string& scene_name
){
    std::vector<EyeBlinkDetector> detectors;
    for (size_t c = 0; c < setups.size(); c++){
        detectors.emplace_back(eyes[c], setups[c].box);
    }

    //  Recover from possible dropped button presses
    for (size_t attempt = 0; attempt < config.press_retries; attempt++){
        if (blink_scene_ready(env, context, detectors, config)){
            return true;
        }
        env.log(scene_name + " position not reached. A press was probably dropped. "
            "Pressing again (" + std::to_string(attempt + 1) + " of "
            + std::to_string(config.press_retries) + ").", COLOR_ORANGE);
        pbf_press_button(context, config.retry_button, 100ms, 1200ms);
        context.wait_for_all_requests();
    }
    env.log("Never reached the " + scene_name + " position. Giving up on this attempt.",
        COLOR_RED);
    return false;
}

void make_blink_watchers(
    const std::vector<BdspEyeTemplate>& setups,
    const std::vector<ImageRGB32>& eyes,
    std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers,
    std::vector<PeriodicInferenceCallback>& callbacks
){
    for (size_t c = 0; c < setups.size(); c++){
        watchers.emplace_back(std::make_unique<EyeBlinkWatcher>(
            setups[c].label, eyes[c], setups[c].box, c == 0 ? COLOR_CYAN : COLOR_YELLOW
        ));
    }
    for (std::unique_ptr<EyeBlinkWatcher>& watcher : watchers){
        callbacks.emplace_back(*watcher, 16ms);
    }
}


bool collect_blink_matches(
    std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers,
    std::vector<std::vector<BlinkMatchSample>>& matches,
    WallClock& origin
){
    matches.clear();
    origin = WallClock::max();
    for (std::unique_ptr<EyeBlinkWatcher>& watcher : watchers){
        matches.emplace_back(watcher->samples());
        if (matches.back().empty()){
            return false;
        }
        origin = std::min(origin, matches.back()[0].timestamp);
    }
    return true;
}

std::vector<std::vector<BlinkEvent>> build_blink_streams(
    const std::vector<std::vector<BlinkMatchSample>>& matches,
    const std::vector<double>& thresholds,
    WallClock origin
){
    std::vector<std::vector<BlinkEvent>> streams;
    streams.reserve(matches.size());
    for (size_t c = 0; c < matches.size(); c++){
        std::vector<BlinkEvent> events = group_blinks(extract_blinks(matches[c], thresholds[c], origin));
        mark_uncertain_blink_types(matches[c], origin, BlinkStallGuardConfig{}, events);
        streams.emplace_back(std::move(events));
    }
    return streams;
}

//  If enough samples have been collected, 
//  drop old ones in case a misread is preventing a solution
static void trim_to_window(
    std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers,
    const std::vector<std::vector<BlinkEvent>>& streams,
    WallClock origin,
    size_t keep
){
    for (size_t c = 0; c < streams.size(); c++){
        if (streams[c].size() <= keep){
            continue;
        }
        //  A second before the oldest blink being kept, so its whole dip survives.
        double seconds = streams[c][streams[c].size() - keep].seconds - 1.0;
        watchers[c]->discard_before(
            origin + std::chrono::duration_cast<WallDuration>(
                std::chrono::duration<double>(seconds)
            )
        );
    }
}

//  prevent the screen from dimming with the right joystick
void keep_awake_if_due(ProControllerContext& context, WallClock& next, Seconds interval){
    if (current_time() < next){
        return;
    }
    pbf_move_right_joystick(context, {1.0, 0.0}, 80ms, 0ms);
    context.wait_for_all_requests();
    next = current_time() + interval;
}


static double resolve_stream_threshold(
    const std::vector<BlinkMatchSample>& stream,
    double frozen,
    double& estimated,
    size_t& estimated_from
){
    if (frozen > 0){
        return frozen;
    }
    const size_t counted = estimated_from;
    const size_t held = stream.size();
    if (estimated <= 0 || held >= counted + counted / 4 || counted >= held + held / 4){
        double fresh = auto_blink_threshold(stream);
        //  Keep the prior estimate if a trimmed window transiently spans < 180s (returns -1).
        if (fresh > 0){
            estimated = fresh;
            estimated_from = held;
        }
    }
    return estimated;
}

static bool resolve_blink_thresholds(
    const std::vector<std::vector<BlinkMatchSample>>& matches,
    const std::vector<double>& frozen,
    std::vector<double>& estimated,
    std::vector<size_t>& estimated_from,
    std::vector<double>& thresholds
){
    thresholds.clear();
    for (size_t w = 0; w < matches.size(); w++){
        double threshold = resolve_stream_threshold(
            matches[w], frozen[w], estimated[w], estimated_from[w]
        );
        if (!(threshold > 0)){
            return false;
        }
        thresholds.emplace_back(threshold);
    }
    return true;
}

static void freeze_blink_thresholds(
    SingleSwitchProgramEnvironment& env,
    const std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers,
    const std::vector<double>& thresholds,
    std::vector<double>& frozen
){
    if (frozen.empty() || frozen[0] > 0){
        return;
    }
    std::string report;
    for (size_t w = 0; w < watchers.size(); w++){
        frozen[w] = thresholds[w];
        report += std::format(
            "{}{} {}",
            report.empty() ? "" : ", ",
            watchers[w]->label(),
            tostr_default(thresholds[w])
        );
    }
    env.log(std::format("Thresholds fixed so that blink numbering stops moving: {}.", report));
}

static BlinkRecovery build_confirmed_recovery(
    const BlinkWindowStep& step, const std::vector<std::vector<BlinkEvent>>& streams,
    WallClock origin, const std::vector<double>& frozen_thresholds, uint8_t npcs
){
    BlinkRecovery ret;
    uint64_t anchor_advance = 0;
    double anchor_seconds = 0;
    size_t anchor_stream = 0;
    if (!last_blink_anchor(streams, step.slots, step.tick_seconds, anchor_advance, anchor_seconds, anchor_stream)){
        ret.failure_reason = "the RNG state was found but no blink could anchor the clock";
        return ret;
    }
    ret.anchor_stream = anchor_stream;
    ret.clock = AdvanceClock{
        origin + std::chrono::duration_cast<WallDuration>(std::chrono::duration<double>(anchor_seconds)),
        anchor_advance, step.tick_seconds, npcs
    };
    ret.state = step.state;
    ret.events = step.events;
    ret.thresholds = frozen_thresholds;
    ret.slots = step.slots;
    ret.success = true;
    return ret;
}

BlinkRecovery recover_state_from_blinks(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers,
    std::vector<PeriodicInferenceCallback>& callbacks,
    BlinkCollectionDisplay& display,
    const BlinkRecoveryConfig& config
){
    warn_blink_capture_health(env);
    BlinkRecovery ret;
    BlinkWindowSolver solver(BlinkWindowSolverConfig{
        .min_rolls_to_try = config.min_rolls_to_try,
        .confirmation_events = config.confirmation_events,
        .window_events = config.window_events,
        .max_failed_windows = config.max_failed_windows,
        .max_confirmation_failures = config.max_confirmation_failures,
    });
    std::vector<double> frozen_thresholds(watchers.size(), -1);
    std::vector<double> estimated_thresholds(watchers.size(), -1);
    std::vector<size_t> estimated_from(watchers.size(), 0);

    WallClock next_nudge = current_time() + config.keep_awake_interval;
    //  Only trips if blinks stop arriving entirely, since no blink means no failed solve
    WallClock deadline = current_time() + config.liveness_timeout;

    //  Held across the whole loop
    CancellableHolder<CancellableScope> subcontext(static_cast<CancellableScope&>(context));
    InferenceSession session(subcontext, env.console, callbacks);

    while (true){
        if (current_time() > deadline){
            ret.failure_reason = std::format(
                "no RNG state solved in {} minutes",
                config.liveness_timeout.count() / 60
            );
            return ret;
        }
        subcontext.wait_until(current_time() + config.poll_interval);

        keep_awake_if_due(context, next_nudge, config.keep_awake_interval);

        std::vector<std::vector<BlinkMatchSample>> matches;
        WallClock origin = WallClock::max();
        if (!collect_blink_matches(watchers, matches, origin)){
            continue;
        }
        std::vector<double> thresholds;
        if (!resolve_blink_thresholds(
            matches, frozen_thresholds, estimated_thresholds, estimated_from, thresholds
        )){
            display.set_note("Determining blink threshold...");
            continue;
        }
        std::vector<std::vector<BlinkEvent>> streams =
            build_blink_streams(matches, thresholds, origin);

        //  don't trim if there is a candidate (the solution is pinned to the earliest blink)
        if (!streams.empty() && solver.should_trim_window()){
            trim_to_window(watchers, streams, origin, config.window_events / streams.size());
        }

        BlinkWindowStep step = solver.step(streams, origin);
        display.set_progress(step.events, config.min_rolls_to_try + config.confirmation_events);

        switch (step.status){
        case BlinkWindowStatus::Waiting:
            continue;
        case BlinkWindowStatus::SolveFailed:
            env.log(std::format("{} rolls seen, no solution yet.", step.events));
            continue;
        case BlinkWindowStatus::ProvisionalFound:
            freeze_blink_thresholds(env, watchers, thresholds, frozen_thresholds);
            env.log(std::format(
                "Provisional state {} from {} rolls. Collecting {} more to confirm.",
                step.state.to_string(), step.events, config.confirmation_events
            ));
            continue;
        case BlinkWindowStatus::ProvisionalChanged:
            env.log(
                "The RNG state changed as more blinks arrived, so the earlier one was "
                "wrong. Continuing.",
                COLOR_ORANGE
            );
            continue;
        case BlinkWindowStatus::ProvisionalDropped:
            std::fill(frozen_thresholds.begin(), frozen_thresholds.end(), -1.0);
            std::fill(estimated_from.begin(), estimated_from.end(), 0);
            if (!streams.empty()){
                trim_to_window(watchers, streams, origin, config.window_events / streams.size());
            }
            env.log(
                "Provisional RNG state failed to confirm; resuming sliding-window search.",
                COLOR_ORANGE
            );
            continue;
        case BlinkWindowStatus::Confirmed:
            freeze_blink_thresholds(env, watchers, thresholds, frozen_thresholds);
            return build_confirmed_recovery(step, streams, origin, frozen_thresholds, config.npcs);
        case BlinkWindowStatus::GaveUp:
            ret.failure_reason = std::format(
                "no RNG state solved over {} blinks",
                config.max_failed_windows * config.window_events
            );
            return ret;
        }
    }
}


bool reanchor_absolute(
    const BlinkRecovery& recovery,
    const std::vector<std::vector<BlinkEvent>>& streams,
    WallClock origin,
    const BlinkRecoveryConfig& config,
    AdvanceClock& clock,
    Logger& logger
){
    if (recovery.anchor_stream >= streams.size() || config.reanchor_blinks < 2){
        return false;
    }
    const std::vector<BlinkEvent>& stream = streams[recovery.anchor_stream];
    if (stream.size() < config.reanchor_blinks){
        return false;
    }
    size_t first = stream.size() - config.reanchor_blinks;

    auto time_of = [&](size_t index){
        return origin + std::chrono::duration_cast<WallDuration>(
            std::chrono::duration<double>(stream[index].seconds)
        );
    };

    std::vector<uint32_t> intervals;
    intervals.reserve(config.reanchor_blinks - 1);
    for (size_t c = first + 1; c < stream.size(); c++){
        double gap = (stream[c].seconds - stream[c - 1].seconds) / clock.tick_seconds;
        long long ticks = std::llround(gap);
        if (ticks < 1){
            //  Two events on one tick means the grouping is wrong
            return false;
        }
        intervals.emplace_back((uint32_t)ticks);
    }

    ReidentifyRequest request;
    request.base_state = recovery.state;
    request.npcs = clock.npcs;
    request.method = ReidentifyMethod::Intervals;
    request.intervals = intervals;

    uint64_t estimate = clock.advance_at(time_of(first));
    request.search_min = estimate > config.reanchor_search_radius
        ? estimate - config.reanchor_search_radius
        : 0;
    request.search_max = estimate + config.reanchor_search_radius;

    ReidentifyResult result = reidentify_advances(request);
    if (!result.success){
        logger.log(
            "Absolute re-anchor found nothing usable, so the stepped clock stands. "
            + result.failure_reason,
            COLOR_ORANGE
        );
        return false;
    }

    //  How far the clock had drifted
    WallClock last_time = time_of(stream.size() - 1);
    double drift_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(
        last_time - clock.time_of_advance(result.advances_to_last_blink)
    ).count();
    double seconds_per_advance = clock.npcs == 0
        ? clock.tick_seconds
        : clock.tick_seconds / (double)clock.npcs;
    double drift_advances = drift_seconds / seconds_per_advance;

    clock.anchor_advance = result.advances_to_last_blink;
    clock.anchor_time = last_time;

    std::string note = "Re-anchored at advance " + std::to_string(clock.anchor_advance)
        + ", " + tostr_fixed(drift_seconds, 3) + "s ("
        + tostr_fixed(drift_advances, 2) + " advances) off what the clock predicted.";
    //  Half an advance is where the aim would actually land somewhere else.
    logger.log(note, std::abs(drift_advances) >= 0.5 ? COLOR_ORANGE : COLOR_BLUE);
    return true;
}


void hold_and_reanchor(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers,
    std::vector<PeriodicInferenceCallback>& callbacks,
    BlinkRecovery& recovery,
    const BlinkRecoveryConfig& config,
    uint64_t press_advance,
    double lead_seconds,
    RngStateDisplay& display
){
    AdvanceClock& clock = recovery.clock;
    WallClock next_nudge = current_time() + config.keep_awake_interval;

    const uint64_t entry_advance = clock.anchor_advance;
    const WallClock entry_time = clock.anchor_time;
    size_t reanchors = 0;
    size_t consecutive_failures = 0;
    WallClock next_reanchor = current_time();

    CancellableHolder<CancellableScope> subcontext(static_cast<CancellableScope&>(context));
    InferenceSession session(subcontext, env.console, callbacks);

    while (true){
        //  Recomputed every pass, because re-anchoring is exactly what changes it.
        WallClock leave_at = clock.time_of_advance(press_advance)
            - std::chrono::duration_cast<WallDuration>(
                std::chrono::duration<double>(lead_seconds)
            );
        // the press is timed off this anchor, so this is the most valuable check
        bool leaving = current_time() >= leave_at;
        if (!leaving){
            subcontext.wait_until(std::min({
                leave_at, next_reanchor, current_time() + config.poll_interval
            }));
            keep_awake_if_due(context, next_nudge, config.keep_awake_interval);
        }

        display.set_advances(clock.advance_at(current_time()));
        if (!leaving && current_time() < next_reanchor){
            continue;
        }
        next_reanchor = current_time() + config.reanchor_interval;

        bool reanchored = false;
        std::vector<std::vector<BlinkMatchSample>> matches;
        WallClock origin = WallClock::max();
        if (collect_blink_matches(watchers, matches, origin)){
            WallClock keep_from = current_time() - config.blink_retention;
            for (std::unique_ptr<EyeBlinkWatcher>& watcher : watchers){
                watcher->discard_before(keep_from);
            }
            std::vector<std::vector<BlinkEvent>> streams =
                build_blink_streams(matches, recovery.thresholds, origin);
            reanchored = reanchor_absolute(
                recovery, streams, origin, config, clock, env.logger()
            );
        }

        if (reanchored){
            reanchors++;
            consecutive_failures = 0;
        }else{
            consecutive_failures++;
            if (consecutive_failures >= config.max_reanchor_failures){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::NO_ERROR_REPORT,
                    std::to_string(consecutive_failures)
                    + " consecutive re-anchor failures: the blinks can no longer be read, "
                    "so the clock cannot be trusted to time the press.",
                    env.console
                );
            }
        }

        if (!leaving){
            continue;
        }
        if (current_time() < clock.time_of_advance(press_advance)
            - std::chrono::duration_cast<WallDuration>(
                std::chrono::duration<double>(lead_seconds)
            )
        ){
            continue;
        }

        if (reanchors == 0){
            env.log("Held without re-anchoring: the clock is still the one the "
                "recovery produced.", COLOR_BLUE);
            return;
        }
        int64_t moved = (int64_t)clock.anchor_advance - (int64_t)entry_advance;
        double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(
            clock.anchor_time - entry_time
        ).count();
        std::string note = "Re-anchored " + std::to_string(reanchors) + " time(s) over "
            + tostr_fixed(seconds, 1) + "s, moving the anchor "
            + std::to_string(moved) + " advance(s)";
        if (moved > 0 && clock.npcs != 0){
            double implied = seconds / ((double)moved / (double)clock.npcs);
            note += ": implied tick " + tostr_fixed(implied, 5)
                + "s against the fitted " + tostr_fixed(clock.tick_seconds, 5)
                + "s (" + tostr_fixed(100 * (implied / clock.tick_seconds - 1), 3) + "%)";
        }
        env.log(note + ".", COLOR_BLUE);
        return;
    }
}




}
}
}
