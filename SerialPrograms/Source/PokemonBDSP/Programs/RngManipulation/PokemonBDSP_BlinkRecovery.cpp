/*  BDSP Blink Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceSession.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP_BlinkRecovery.h"
#include "PokemonBDSP_StateReidentifier.h"
#include "PokemonBDSP_StateSolver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace Pokemon;
using namespace std::chrono_literals;


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
    for (size_t c = 0; c < matches.size(); c++){
        streams.emplace_back(group_blinks(extract_blinks(matches[c], thresholds[c], origin)));
    }
    return streams;
}

//  If enough samples have been collected, drop old ones in case a misread is
//  preventing a solution. One cutoff for every stream: a stream cut later than
//  another would read its dropped blinks as rolls that did not blink.
static std::optional<double> window_start(
    const std::vector<std::vector<BlinkEvent>>& streams,
    size_t keep
){
    std::vector<double> seconds;
    for (const std::vector<BlinkEvent>& stream : streams){
        for (const BlinkEvent& event : stream){
            seconds.emplace_back(event.seconds);
        }
    }
    if (seconds.size() <= keep){
        return std::nullopt;
    }
    std::vector<double>::iterator oldest_kept = seconds.end() - keep;
    std::nth_element(seconds.begin(), oldest_kept, seconds.end());
    //  A second before the oldest blink being kept, so its whole dip survives.
    return *oldest_kept - 1.0;
}

static void discard_blinks_before(
    WallClock origin,
    double seconds,
    std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers,
    BlinkSampleLog& sample_log
){
    WallClock keep_from = origin + std::chrono::duration_cast<WallDuration>(
        std::chrono::duration<double>(seconds)
    );
    for (size_t c = 0; c < watchers.size(); c++){
        watchers[c]->discard_before(keep_from);
        sample_log.window(c, keep_from);
    }
}

//  prevent the screen from dimming with the right joystick
static void keep_awake_if_due(ProControllerContext& context, WallClock& next, Seconds interval){
    if (current_time() < next){
        return;
    }
    pbf_move_right_joystick(context, {1.0, 0.0}, 80ms, 0ms);
    context.wait_for_all_requests();
    next = current_time() + interval;
}


//  Logs sparse collection milestones until a provisional state is found.
static void log_search_progress(
    const BlinkRecoveryConfig& config,
    size_t events,
    size_t search_updates,
    SingleSwitchProgramEnvironment& env
){
    if (events < config.min_rolls_to_try){
        if (events != 1 && events % 5 != 0){
            return;
        }
        env.log(std::format(
            "Calibrating blink detection: {}/{} rolls collected.",
            events, config.min_rolls_to_try
        ));
        return;
    }
    if (search_updates == 1 || search_updates % 5 == 0){
        env.log(std::format(
            "Still searching for the first RNG state: {} rolls available.", events
        ));
    }
}

//  Reports the active calibration or confirmation phase.
static void report_blink_progress(
    const BlinkRecoveryConfig& config,
    size_t events,
    size_t search_updates,
    bool have_candidate,
    size_t candidate_events,
    SingleSwitchProgramEnvironment& env
){
    if (!have_candidate){
        log_search_progress(config, events, search_updates, env);
        return;
    }
    //  confirmation_events is deliberately small (six by default), so report
    //  every fresh roll in this bounded phase.
    size_t confirmed = std::min(events - candidate_events, config.confirmation_events);
    env.log(std::format(
        "Confirming the provisional RNG state: {}/{} additional rolls collected.",
        confirmed, config.confirmation_events
    ));
}

static size_t report_fresh_progress(
    const BlinkRecoveryConfig& config,
    size_t events,
    size_t search_updates,
    bool have_candidate,
    size_t candidate_events,
    SingleSwitchProgramEnvironment& env
){
    search_updates += !have_candidate && events >= config.min_rolls_to_try ? 1 : 0;
    report_blink_progress(config, events, search_updates, have_candidate, candidate_events, env);
    return search_updates;
}

ProvisionalStep update_provisional(
    ProvisionalState& provisional,
    bool solved,
    const Xorshift128State& state,
    size_t events,
    size_t confirmation_events
){
    if (!solved){
        if (!provisional.held){
            return ProvisionalStep::KEEP_COLLECTING;
        }
        provisional.failed_solves++;
        if (provisional.failed_solves < confirmation_events){
            return ProvisionalStep::KEEP_COLLECTING;
        }
        provisional = ProvisionalState();
        return ProvisionalStep::DROPPED;
    }
    provisional.failed_solves = 0;
    if (provisional.held && state == provisional.state){
        return ProvisionalStep::CONFIRMED;
    }
    const bool was_held = provisional.held;
    provisional.held = true;
    provisional.state = state;
    provisional.events = events;
    return was_held ? ProvisionalStep::CHANGED : ProvisionalStep::PROVISIONAL;
}

//  Logs a step of the provisional state; true once it is confirmed.
static bool report_provisional_step(
    SingleSwitchProgramEnvironment& env,
    ProvisionalStep step,
    const Xorshift128State& state,
    size_t events,
    size_t confirmation_events
){
    switch (step){
    case ProvisionalStep::PROVISIONAL:
        env.log("Provisional state " + state.to_string() + " from "
            + std::to_string(events) + " rolls. Collecting "
            + std::to_string(confirmation_events) + " more to confirm.");
        return false;
    case ProvisionalStep::CHANGED:
        env.log(
            "The RNG state changed as more blinks arrived, so the earlier one was "
            "wrong. Continuing.",
            COLOR_ORANGE
        );
        return false;
    case ProvisionalStep::DROPPED:
        env.log(
            "The provisional RNG state stopped solving as blinks arrived, so a blink "
            "was probably misread. Searching again over the newest blinks.",
            COLOR_ORANGE
        );
        return false;
    case ProvisionalStep::CONFIRMED:
        return true;
    default:
        return false;
    }
}

//  Solves with each rotation of the NPC slots until one solves.
static void solve_any_slot_order(
    const std::vector<std::vector<BlinkEvent>>& streams,
    BlinkWindowPass& pass
){
    std::vector<uint8_t> slots(streams.size());
    for (uint8_t c = 0; c < (uint8_t)streams.size(); c++){
        slots[c] = c;
    }
    for (size_t attempt = 0; attempt < streams.size(); attempt++){
        std::vector<BlinkSample> samples;
        std::string failure;
        if (build_samples(streams, slots, pass.period_seconds, samples, failure)){
            BlinkSolveResult attempt_result = solve_state_from_samples(samples, nullptr);
            if (attempt_result.success){
                pass.solved = attempt_result;
                pass.slots = slots;
                return;
            }
        }
        std::rotate(slots.begin(), slots.begin() + 1, slots.end());
    }
}

//  The solve of one pass, before the window slides.
static BlinkWindowPass solve_window_once(
    const std::vector<std::vector<BlinkEvent>>& streams,
    const BlinkRecoveryConfig& config,
    ProvisionalState& provisional
){
    BlinkWindowPass pass;
    size_t events = 0;
    const std::vector<BlinkEvent>* longest = &streams[0];
    for (const std::vector<BlinkEvent>& stream : streams){
        events += stream.size();
        if (stream.size() > longest->size()){
            longest = &stream;
        }
    }
    if (longest->size() < 3){
        return pass;
    }
    TickFit fit = fit_tick_period(*longest);
    if (!(fit.period_seconds > 0)){
        return pass;
    }
    if (provisional.held && events < provisional.events + config.confirmation_events){
        return pass;
    }
    if (events < config.min_rolls_to_try){
        return pass;
    }
    pass.attempted = true;
    pass.period_seconds = fit.period_seconds;
    solve_any_slot_order(streams, pass);
    pass.step = update_provisional(
        provisional, pass.solved.success, pass.solved.state, events, config.confirmation_events
    );
    return pass;
}

BlinkWindowPass solve_blink_window(
    const std::vector<std::vector<BlinkEvent>>& streams,
    const BlinkRecoveryConfig& config,
    ProvisionalState& provisional
){
    BlinkWindowPass pass = solve_window_once(streams, config, provisional);
    //  Not before the solve: a state found on this pass is pinned to its
    //  oldest blink, and a later solve repeats it only from the same one.
    if (!provisional.held){
        pass.discard_before = window_start(streams, config.window_events);
    }
    return pass;
}

struct ThresholdEstimates{
    std::vector<double> value;
    std::vector<size_t> from;
};

//  Re-estimates a watcher's threshold whenever its sample count has moved by a
//  quarter since the last estimate.
static double estimated_threshold(
    size_t watcher,
    const std::vector<BlinkMatchSample>& samples,
    ThresholdEstimates& estimates,
    BlinkSampleLog& log
){
    size_t counted = estimates.from[watcher];
    size_t held = samples.size();
    if (estimates.value[watcher] <= 0
        || held >= counted + counted / 4
        || counted >= held + held / 4
    ){
        estimates.value[watcher] = auto_blink_threshold(samples);
        estimates.from[watcher] = held;
        log.threshold(watcher, "estimated", estimates.value[watcher], held);
    }
    return estimates.value[watcher];
}

//  Logs a solve that did not confirm; a confirmed state is logged once its
//  clock is anchored.
static void log_unconfirmed_pass(
    BlinkSampleLog& log,
    const BlinkWindowPass& pass,
    size_t events,
    const std::vector<std::vector<BlinkEvent>>& streams,
    WallClock origin
){
    switch (pass.step){
    case ProvisionalStep::KEEP_COLLECTING:
        log.decision({.kind = "solve_failed", .rolls = events}, /*streams=*/{}, origin);
        return;
    case ProvisionalStep::DROPPED:
        log.decision({.kind = "dropped", .rolls = events}, streams, origin);
        return;
    case ProvisionalStep::PROVISIONAL:
        log.decision({.kind = "provisional", .rolls = events, .state = pass.solved.state}, streams, origin);
        return;
    case ProvisionalStep::CHANGED:
        log.decision({.kind = "changed", .rolls = events, .state = pass.solved.state}, streams, origin);
        return;
    default:
        return;
    }
}

BlinkRecovery recover_state_from_blinks(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers,
    std::vector<PeriodicInferenceCallback>& callbacks,
    BlinkCollectionDisplay& display,
    const BlinkRecoveryConfig& config,
    BlinkSampleLog* sample_log
){
    BlinkRecovery ret;
    BlinkSampleLog no_log;
    BlinkSampleLog& log = sample_log != nullptr ? *sample_log : no_log;

    size_t give_up_after = config.max_failed_windows * config.window_events;
    size_t failed_solves = 0;

    std::vector<double> frozen_thresholds(watchers.size(), -1);

    ThresholdEstimates estimates{std::vector<double>(watchers.size(), -1), std::vector<size_t>(watchers.size(), 0)};

    ProvisionalState provisional;
    size_t search_updates = 0;
    std::vector<WallClock> newest_blink(watchers.size(), WallClock::min());

    WallClock next_nudge = current_time() + config.keep_awake_interval;

    //  Only trips if blinks stop arriving entirely, since no blink means no failed solve
    WallClock deadline = current_time() + config.liveness_timeout;

    //  Held across the whole loop
    CancellableHolder<CancellableScope> subcontext(static_cast<CancellableScope&>(context));
    InferenceSession session(subcontext, env.console, callbacks);

    while (true){
        if (failed_solves >= give_up_after){
            ret.failure_reason = "no RNG state solved over "
                + std::to_string(give_up_after) + " blinks";
            log.decision({.kind = "gave_up", .note = ret.failure_reason}, /*streams=*/{}, /*origin=*/WallClock::min());
            return ret;
        }
        if (current_time() > deadline){
            ret.failure_reason = "no RNG state solved in "
                + std::to_string(config.liveness_timeout.count() / 60) + " minutes";
            log.decision({.kind = "gave_up", .note = ret.failure_reason}, /*streams=*/{}, /*origin=*/WallClock::min());
            return ret;
        }
        subcontext.wait_until(current_time() + config.poll_interval);

        keep_awake_if_due(context, next_nudge, config.keep_awake_interval);

        std::vector<std::vector<BlinkMatchSample>> matches;
        WallClock origin = WallClock::max();
        if (!collect_blink_matches(watchers, matches, origin)){
            continue;
        }
        log.add_samples(matches);

        std::vector<double> thresholds;
        for (size_t w = 0; w < watchers.size(); w++){
            double threshold = frozen_thresholds[w];
            if (threshold <= 0){
                threshold = estimated_threshold(w, matches[w], estimates, log);
            }
            if (!(threshold > 0)){
                display.set_note("Determining blink threshold...");
                break;
            }
            thresholds.emplace_back(threshold);
        }
        if (thresholds.size() != watchers.size()){
            continue;
        }
        std::vector<std::vector<BlinkEvent>> streams =
            build_blink_streams(matches, thresholds, origin);

        size_t events = 0;
        for (const std::vector<BlinkEvent>& stream : streams){
            events += stream.size();
        }
        display.set_progress(events, config.min_rolls_to_try + config.confirmation_events);

        bool fresh = false; // count doesn't change once the window is full
        for (size_t w = 0; w < streams.size(); w++){
            if (streams[w].empty()){
                continue;
            }
            WallClock newest = origin + std::chrono::duration_cast<WallDuration>(
                std::chrono::duration<double>(streams[w].back().seconds)
            );
            if (newest > newest_blink[w] + std::chrono::seconds(1)){
                fresh = true;
            }
            newest_blink[w] = std::max(newest_blink[w], newest);
        }
        if (!fresh){
            continue;
        }
        search_updates = report_fresh_progress(config, events, search_updates, provisional.held, provisional.events, env);

        const BlinkWindowPass pass = solve_blink_window(streams, config, provisional);
        if (pass.discard_before.has_value()){
            discard_blinks_before(origin, *pass.discard_before, watchers, log);
        }
        if (!pass.attempted){
            continue;
        }
        failed_solves = pass.solved.success ? 0 : failed_solves + 1;
        if (pass.solved.success && frozen_thresholds[0] <= 0){
            std::string report;
            for (size_t w = 0; w < watchers.size(); w++){
                frozen_thresholds[w] = thresholds[w];
                log.threshold(w, "frozen", thresholds[w], matches[w].size());
                report += (report.empty() ? "" : ", ")
                    + watchers[w]->label() + " " + tostr_default(thresholds[w]);
            }
            env.log("Thresholds fixed so that blink numbering stops moving: " + report + ".");
        }
        log_unconfirmed_pass(log, pass, events, streams, origin);
        if (!report_provisional_step(env, pass.step, pass.solved.state, events, config.confirmation_events)){
            continue;
        }

        uint64_t anchor_advance = 0;
        double anchor_seconds = 0;
        size_t anchor_stream = 0;
        if (!last_blink_anchor(
            streams, pass.slots, pass.period_seconds,
            anchor_advance, anchor_seconds, anchor_stream
        )){
            ret.failure_reason = "the RNG state was found but no blink could anchor the clock";
            log.decision({.kind = "gave_up", .rolls = events, .state = pass.solved.state, .note = ret.failure_reason}, streams, origin);
            return ret;
        }
        ret.anchor_stream = anchor_stream;
        ret.clock.anchor_advance = anchor_advance;
        ret.clock.anchor_time = origin + std::chrono::duration_cast<WallDuration>(
            std::chrono::duration<double>(anchor_seconds)
        );
        ret.clock.tick_seconds = pass.period_seconds;
        ret.clock.npcs = config.npcs;
        ret.state = pass.solved.state;
        ret.events = events;
        ret.thresholds = frozen_thresholds;
        ret.slots = pass.slots;
        ret.success = true;
        log.decision({.kind = "confirmed", .rolls = events, .state = pass.solved.state}, streams, origin);
        return ret;
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
