/*  BDSP Starter RNG Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <cmath>
#include <format>
#include <memory>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Options/Pokemon_StatsHuntFilter.h"
#include "Pokemon/Pokemon_NatureChecker.h"
#include "Pokemon/Pokemon_Notification.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/Rng/PokemonBDSP_BlinkScenes.h"
#include "PokemonBDSP/Inference/ShinyDetection/PokemonBDSP_ShinyEncounterDetector.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameEntry.h"
#include "PokemonBDSP_BlinkRecovery.h"
#include "PokemonBDSP_BlinkSampleLog.h"
#include "PokemonBDSP_RngCalibration.h"
#include "PokemonBDSP_RngExecution.h"
#include "PokemonBDSP_RngTargets.h"
#include "PokemonBDSP_RngTimeline.h"
#include "PokemonBDSP_StarterRngAttempts.h"
#include "PokemonBDSP_SummaryNavigation.h"
#include "PokemonBDSP_TargetSelection.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace Pokemon;


namespace{

const size_t LAKE_BLINKS_PRESS_RETRIES = 10;

const double GAP_GUARD_SECONDS = 0.30;

const uint64_t HIT_SEARCH_RADIUS = 20;
const size_t MAX_SCHEDULES = 8;

const uint64_t SKIP_ARRIVAL_OVERSHOOT_ALLOWANCE = 200000;


//  Logs the lake observation phase before frame collection begins.
void log_calibration_start(size_t rolls, SingleSwitchProgramEnvironment& env){
    env.log(std::format(
        "Calibrating blink detection; collecting {} rolls before the first solve.", rolls
    ), COLOR_BLUE);
}

//  Logs why the program remains at the blink position and how it guards the clock.
void log_target_wait(
    uint64_t press_advance,
    double wait_seconds,
    Seconds reanchor_interval,
    SingleSwitchProgramEnvironment& env
){
    env.log(std::format(
        "Waiting at the blink position for press advance {} (about {}s); re-anchoring every {}s.",
        press_advance, tostr_default(wait_seconds), reanchor_interval.count()
    ), COLOR_BLUE);
}

//  Logs every scheduled input before the blind timeline begins.
void log_blind_timeline(
    uint64_t press_advance,
    const PressSchedule& schedule,
    uint64_t accept_advance,
    SingleSwitchProgramEnvironment& env
){
    env.log(std::format(
        "Blind timeline starts at advance {}. Starly press at {}s, selecting at {}s, "
        "confirming at {}s, on advance {}.",
        press_advance, tostr_default(schedule.starly_seconds), tostr_default(schedule.select_seconds),
        tostr_default(schedule.confirm_seconds), accept_advance
    ), COLOR_BLUE);
}


std::function<bool(const BdspPokemonResult&)> resolve_wanted_predicate(
    const StarterRngSettings& settings
){
    if (settings.wanted){
        return settings.wanted;
    }
    if (settings.filters != nullptr){
        const BdspRngFilterSnapshot* filters = settings.filters;
        return [filters](const BdspPokemonResult& result){
            return starter_wanted(*filters, result);
        };
    }
    return [](const BdspPokemonResult& result){
        return result.shiny != BdspShiny::None;
    };
}


struct AttemptDisplays{
    BlinkCollectionDisplay& collection;
    RngStateDisplay& state;
    RngTargetDisplay& target;
};

void validate_skip_budget(
    SingleSwitchProgramEnvironment& env,
    const StarterRngSettings& settings,
    const BdspRngTargetInfo& target_info
){
    const double longest_skip = settings.max_skip_minutes * BdspSkipRates::ADVANCES_PER_MINUTE;
    const double lake_minutes = bdsp_lake_advances_needed(longest_skip)
        * BDSP_NPC_TICK_SECONDS / (target_info.observation_npcs * 60.0);
    if (lake_minutes > settings.max_target_wait_minutes){
        throw UserSetupError(env.logger(),
            "A skip of up to " + std::to_string(settings.max_skip_minutes)
            + " minutes can leave up to "
            + tostr_fixed(lake_minutes, 1) + " minutes of waiting at the lake, but the target "
            "wait is capped at " + std::to_string(settings.max_target_wait_minutes)
            + ". Raise the target wait to at least "
            + std::to_string((uint16_t)std::ceil(lake_minutes))
            + " minutes, or lower the skip limit."
        );
    }
}

//  One file per attempt with every blink sample, so a failed recovery can be replayed.
BlinkSampleLog open_blink_sample_log(SingleSwitchProgramEnvironment& env){
    const std::string folder = std::format("{}StarterRng/blinks/", SCREENSHOTS_PATH());
    Filesystem::create_directories(Filesystem::Path(folder));
    const std::string path = std::format("{}{}.txt", folder, now_to_filestring());
    BlinkSampleLog log(path);
    if (log.is_open()){
        env.log(std::format("Blink samples: {}", path));
    }else{
        env.log(std::format("Could not open {}; this attempt's blinks are not logged.", path), COLOR_ORANGE);
    }
    return log;
}

BlinkRecovery recover_bedroom_state(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const StarterRngSettings& settings,
    AttemptDisplays& displays,
    BlinkSampleLog& sample_log
){
    env.log("Starting in the bedroom.");
    pbf_press_button(context, BUTTON_A, 100ms, 2000ms); // open dialogue box
    context.wait_for_all_requests();

    std::vector<BdspEyeTemplate> setups = bedroom_eye_templates(settings.player_model);
    std::vector<ImageRGB32> eyes = load_eye_templates(setups);
    std::vector<std::unique_ptr<EyeBlinkWatcher>> watchers;
    std::vector<PeriodicInferenceCallback> callbacks;
    make_blink_watchers(setups, eyes, watchers, callbacks);

    BlinkRecoveryConfig bedroom_config;
    bedroom_config.npcs = 1;
    sample_log.begin_recovery("bedroom", bedroom_config.npcs, watchers);
    return recover_state_from_blinks(
        env, context, watchers, callbacks, displays.collection, bedroom_config, &sample_log
    );
}

void wait_out_skip_duration(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    double wait_seconds
){
    navigate_bedroom_to_skip_spot(env.logger(), context);
    const WallClock leave_at = current_time() + std::chrono::duration_cast<WallDuration>(
        std::chrono::duration<double>(wait_seconds)
    );
    while (current_time() + 2s < leave_at){
        const WallClock now = current_time();
        const WallClock until = std::min(leave_at - 2s, now + 180s);
        pbf_wait(context, std::chrono::duration_cast<Milliseconds>(until - now));
        context.wait_for_all_requests();
    }
    const WallClock now = current_time();
    if (now < leave_at){
        pbf_wait(context, std::chrono::duration_cast<Milliseconds>(leave_at - now));
        context.wait_for_all_requests();
    }
    navigate_skip_spot_to_lakefront(env.logger(), context);
}

BdspSkipResult skip_advances_from_bedroom(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const StarterRngSettings& settings,
    const std::function<bool(const BdspPokemonResult&)>& wanted_fn,
    AttemptDisplays& displays,
    BlinkSampleLog& sample_log
){
    BdspSkipResult skip;
    const BdspRngTargetInfo& target_info = bdsp_rng_target_info(BdspRngTarget::Starter);
    validate_skip_budget(env, settings, target_info);

    const BlinkRecovery recovery = recover_bedroom_state(env, context, settings, displays, sample_log);
    if (!recovery.success){
        skip.failure_reason = "could not determine the RNG state in the bedroom: " + recovery.failure_reason;
        return skip;
    }
    skip.state = recovery.state;
    env.log("Bedroom RNG state: " + recovery.state.to_string(), COLOR_BLUE);
    displays.state.set_state(recovery.state, recovery.clock.anchor_advance);
    displays.state.set_confidence_unique();

    const uint64_t soonest = recovery.clock.advance_at(current_time())
        + (uint64_t)BdspSkipRates::NAVIGATION_ADVANCES + bdsp_skip_buffer(0);
    const uint64_t furthest = soonest
        + (uint64_t)(settings.max_skip_minutes * BdspSkipRates::ADVANCES_PER_MINUTE);
    BdspStaticSearcher searcher(recovery.state, target_info.pokemon, 0);
    const std::vector<BdspRngHit> hits = searcher.scan(soonest, furthest, wanted_fn, true, &context);
    if (hits.empty()){
        skip.failure_reason = "no target within " + std::to_string(settings.max_skip_minutes) + " minutes of skipping";
        return skip;
    }
    const BdspRngHit& hit = hits[0];
    skip.target_advance = hit.advances;
    displays.target.set_target(hit.result, hit.advances);

    const uint64_t start = recovery.clock.advance_at(current_time());
    const double raw_skip = (double)hit.advances - (double)start - BdspSkipRates::NAVIGATION_ADVANCES;
    const uint64_t buffer = bdsp_skip_buffer(raw_skip);
    const double to_skip = std::max(0.0, raw_skip - (double)buffer);
    const double wait_seconds = to_skip * 60 / BdspSkipRates::ADVANCES_PER_MINUTE;
    skip.buffer = buffer;
    env.log("Target advance " + std::to_string(hit.advances) + ": " + hit.result.to_string(), COLOR_BLUE);
    env.log("Skipping " + std::to_string((uint64_t)to_skip) + " advances from " + std::to_string(start)
        + ", which is " + tostr_fixed(wait_seconds, 1) + "s on the spot, stopping "
        + std::to_string(buffer) + " advances short to avoid overshooting.", COLOR_BLUE);
    wait_out_skip_duration(env, context, wait_seconds);
    skip.success = true;
    return skip;
}


void report_skip_arrival(
    SingleSwitchProgramEnvironment& env,
    const BdspSkipResult& skip,
    const BlinkRecovery& arrival
){
    //  Generous enough to catch an overshoot, and a match is unique anyway.
    uint64_t search_max = skip.target_advance + SKIP_ARRIVAL_OVERSHOOT_ALLOWANCE;
    uint64_t travelled = 0;
    if (!advances_between(skip.state, arrival.state, search_max, travelled)){
        env.log(
            "Could not place the lake state against the bedroom one, so the skip "
            "cannot be measured this attempt.",
            COLOR_ORANGE
        );
        return;
    }

    uint64_t here = travelled + arrival.clock.advance_at(current_time());
    int64_t short_by = (int64_t)skip.target_advance - (int64_t)here;
    env.log(
        "Skip landed on advance " + std::to_string(here) + " of "
        + std::to_string(skip.target_advance) + ": "
        + (short_by >= 0
            ? std::to_string(short_by) + " short, aimed for " + std::to_string(skip.buffer)
            : "OVERSHOT by " + std::to_string(-short_by)),
        short_by >= 0 ? COLOR_BLUE : COLOR_ORANGE
    );
}

} // namespace


StarterRngStats::StarterRngStats()
    : resets(m_stats["Resets"])
    , missed(m_stats["Missed"])
    , hits(m_stats["Hit"])
    , shinies(m_stats["Shiny"])
    , errors(m_stats["Errors"])
{
    m_display_order.emplace_back("Resets");
    m_display_order.emplace_back(Stat("Missed", HIDDEN_IF_ZERO));
    m_display_order.emplace_back(Stat("Hit", HIDDEN_IF_ZERO));
    m_display_order.emplace_back(Stat("Shiny", HIDDEN_IF_ZERO));
    m_display_order.emplace_back(Stat("Errors", HIDDEN_IF_ZERO));
}


bool starter_wanted(
    const BdspRngFilterSnapshot& filters,
    const BdspPokemonResult& pokemon
){
    auto exactly = [](uint8_t iv){
        return IvRange{(int8_t)iv, (int8_t)iv};
    };
    IvRanges ivs;
    ivs.hp      = exactly(pokemon.ivs.hp);
    ivs.attack  = exactly(pokemon.ivs.attack);
    ivs.defense = exactly(pokemon.ivs.defense);
    ivs.spatk   = exactly(pokemon.ivs.spatk);
    ivs.spdef   = exactly(pokemon.ivs.spdef);
    ivs.speed   = exactly(pokemon.ivs.speed);

    StatsHuntGenderFilter gender = StatsHuntGenderFilter::Genderless;
    switch (pokemon.gender){
    case BdspGender::Male:
        gender = StatsHuntGenderFilter::Male;
        break;
    case BdspGender::Female:
        gender = StatsHuntGenderFilter::Female;
        break;
    default:
        break;
    }

    return filters.get_action(
        pokemon.shiny != BdspShiny::None,
        gender,
        bdsp_nature_to_checker_value(pokemon.nature),
        pokemon.height,
        ivs
    ) != StatsHuntAction::Discard;
}


bool resolve_starter_rng_step(
    const BdspStarterAttemptResult& attempt,
    uint64_t resets_used,
    uint16_t max_resets,
    StarterRngRunResult& result
){
    result.resets_used = resets_used;
    if (attempt.target_advance != 0){
        result.target = attempt.target;
        result.target_advance = attempt.target_advance;
    }
    if (!attempt.failure_reason.empty()){
        result.last_failure_reason = attempt.failure_reason;
    }

    if (attempt.outcome == BdspAttemptOutcome::Hit){
        result.settled = attempt.settled;
        result.outcome = attempt.shiny
            ? StarterRngOutcome::SHINY
            : StarterRngOutcome::HIT;
        return true;
    }

    if (max_resets != 0 && resets_used >= max_resets){
        result.outcome = attempt.outcome == BdspAttemptOutcome::Abandoned
            ? StarterRngOutcome::ABANDONED
            : StarterRngOutcome::OUT_OF_RESETS;
        return true;
    }

    return false;
}


//  Leaves the game as the settings ask after a shiny: battle finished and, if
//  asked, the starter summary read and the menu closed. False if a step failed.
static bool settle_after_shiny(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context, const StarterRngSettings& settings
){
    if (!settings.finish_battle_on_shiny){
        return true;
    }
    if (!clear_starter_battle(env, context)){
        return false;
    }
    if (settings.close_summary_after_read){
        if (!open_menu(env, context)){
            return false;
        }
        pbf_press_button(context, BUTTON_X, 160ms, 1000ms);
        context.wait_for_all_requests();
        env.log("Closed the menu.");
    }
    return true;
}

namespace{

void record_miss_io(const StarterRngAttemptIO& io){
    if (io.stats != nullptr){
        io.stats->missed++;
    }
    if (io.on_miss){
        io.on_miss();
    }
}

void record_hit_io(const StarterRngAttemptIO& io){
    if (io.stats != nullptr){
        io.stats->hits++;
    }
    if (io.on_hit){
        io.on_hit();
    }
}

void record_shiny_io(const StarterRngAttemptIO& io){
    if (io.stats != nullptr){
        io.stats->shinies++;
    }
    if (io.on_shiny){
        io.on_shiny();
    }
}

void record_error_io(const StarterRngAttemptIO& io){
    if (io.stats != nullptr){
        io.stats->errors++;
    }
    if (io.on_error){
        io.on_error();
    }
}

BdspStarterAttemptResult abandon_attempt(
    SingleSwitchProgramEnvironment& env,
    AttemptDisplays& displays,
    const std::string& reason
){
    env.log("Abandoning this attempt: " + reason, COLOR_ORANGE);
    displays.target.set_note(reason);
    return BdspStarterAttemptResult{
        .outcome = BdspAttemptOutcome::Abandoned,
        .failure_reason = reason,
    };
}

struct LakeBlinkSession{
    std::vector<BdspEyeTemplate> setups;
    std::vector<ImageRGB32> eyes;
    std::vector<std::unique_ptr<EyeBlinkWatcher>> watchers;
    std::vector<PeriodicInferenceCallback> callbacks;
    BlinkRecoveryConfig config;
    BlinkRecovery recovery;
};

void log_lake_recovery(
    SingleSwitchProgramEnvironment& env,
    const BlinkRecovery& recovery,
    AttemptDisplays& displays
){
    uint64_t seed0 = 0;
    uint64_t seed1 = 0;
    xorshift128_state_to_seed_pair(recovery.state, seed0, seed1);
    env.log("RNG state: " + recovery.state.to_string(), COLOR_BLUE);
    env.log("PokeFinder seeds: " + tostr_hex_padded(16, seed0)
        + " " + tostr_hex_padded(16, seed1), COLOR_BLUE);
    env.log("Tick fitted at " + tostr_default(recovery.clock.tick_seconds) + "s over "
        + std::to_string(recovery.events) + " rolls.", COLOR_BLUE);
    env.log("Anchored on advance " + std::to_string(recovery.clock.anchor_advance)
        + "; now at advance " + std::to_string(recovery.clock.advance_at(current_time()))
        + ".", COLOR_BLUE);
    displays.state.set_state(recovery.state, recovery.clock.anchor_advance);
    displays.state.set_confidence_unique();
}

std::string recover_lake_session(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const StarterRngSettings& settings,
    const BdspRngTargetInfo& target_info,
    AttemptDisplays& displays,
    LakeBlinkSession& session,
    BlinkSampleLog& sample_log
){
    navigate_to_lake_blinks(env.logger(), context);
    session.setups = lake_eye_templates(settings.player_model);
    session.eyes = load_eye_templates(session.setups);

    ShortDialogDetector dialog;
    BlinkSceneConfig scene;
    scene.frame_filter = [&dialog](const ImageViewRGB32& frame){
        return dialog.detect(frame);
    };
    scene.press_retries = LAKE_BLINKS_PRESS_RETRIES;
    if (!wait_for_blink_scene(env, context, session.setups, session.eyes, scene, "Lake blinks")){
        return "never reached the lake blink position";
    }

    make_blink_watchers(session.setups, session.eyes, session.watchers, session.callbacks);
    session.config.npcs = target_info.observation_npcs;
    log_calibration_start(session.config.min_rolls_to_try, env);
    sample_log.begin_recovery("lake", session.config.npcs, session.watchers);
    session.recovery = recover_state_from_blinks(
        env, context, session.watchers, session.callbacks, displays.collection, session.config, &sample_log
    );
    if (!session.recovery.success){
        return "could not determine the RNG state: " + session.recovery.failure_reason;
    }
    log_lake_recovery(env, session.recovery, displays);
    return {};
}

TargetSelectionResult find_starter_target(
    SingleSwitchProgramEnvironment& env,
    const StarterRngSettings& settings,
    const BdspRngTargetInfo& target_info,
    const BlinkRecovery& recovery,
    const std::function<bool(const BdspPokemonResult&)>& wanted_fn,
    const RngAim& aim,
    double& lead_seconds
){
    const StarterSceneDurations durations = StarterSceneDurations::measured(settings.language);
    NavigationTimings timings;
    timings.ready_seconds = durations.briefcase_to_starly_ready;
    timings.move_seconds = seconds_to_move_to_starter(settings.starter) + 0.25;
    timings.prompt_seconds = durations.select_to_confirm;
    timings.guard_seconds = GAP_GUARD_SECONDS;

    lead_seconds = durations.to_pre_briefcase + 15;
    const uint64_t lead_advances = (uint64_t)(
        lead_seconds * recovery.clock.npcs / recovery.clock.tick_seconds
    ) + 1;

    TargetSearchRequest search;
    search.state = recovery.state;
    search.timeline = target_info.timeline;
    search.timings = timings;
    search.pokemon = target_info.pokemon;
    search.npcs = recovery.clock.npcs;
    search.tick_seconds = recovery.clock.tick_seconds;
    search.bias = aim.bias();
    search.first_press = recovery.clock.advance_at(current_time()) + lead_advances;
    search.window_advances = (uint64_t)(
        (double)settings.max_target_wait_minutes * 60 * recovery.clock.npcs / recovery.clock.tick_seconds
    );
    search.max_schedules = MAX_SCHEDULES;
    search.wanted = wanted_fn;

    TargetSelectionResult selection = select_target(search);
    if (selection.matches_found != 0){
        env.log(
            std::to_string(selection.matches_found) + " matching advance(s) in "
            + std::to_string(selection.advances_scanned),
            COLOR_BLUE
        );
    }
    return selection;
}

struct AttemptRunState{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    const StarterRngSettings& settings;
    const StarterRngAttemptIO& io;
    const BdspRngTargetInfo& target_info;
    const std::function<bool(const BdspPokemonResult&)>& wanted_fn;
    AttemptDisplays& displays;
    RngAim& aim;
};

void log_selected_target(
    const AttemptRunState& run,
    const LakeBlinkSession& session,
    const TargetSelectionResult& selection
){
    const double wait_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(
        session.recovery.clock.time_of_advance(selection.press_advance) - current_time()
    ).count();
    run.env.log("Target advance " + std::to_string(selection.target_advance) + ": " + selection.target.to_string(), COLOR_BLUE);
    log_target_wait(selection.press_advance, wait_seconds, session.config.reanchor_interval, run.env);
    run.env.log("Schedule spans " + std::to_string(selection.schedule.span) + " advances, confirming after "
        + std::to_string(selection.schedule.blinks_before_confirm) + " Starly blink(s), "
        + tostr_default(selection.schedule.confirm_seconds) + "s in"
        + run.aim.describe_correction() + ".", COLOR_BLUE);
    run.displays.target.set_target(selection.target, selection.target_advance);
}

std::string run_blind_starter_sequence(
    const AttemptRunState& run,
    const TargetSelectionResult& selection,
    LakeBlinkSession& session,
    double lead_seconds
){
    const uint64_t press_advance = selection.press_advance;
    const uint64_t target_advance = selection.target_advance;
    const PressSchedule& schedule = selection.schedule;

    hold_and_reanchor(
        run.env, run.context, session.watchers, session.callbacks, session.recovery, session.config,
        press_advance, lead_seconds, run.displays.state
    );
    navigate_to_pre_briefcase(run.env, run.context);

    const WallClock briefcase_time = session.recovery.clock.middle_of_advance(press_advance);
    while (current_time() + 3s < briefcase_time){
        run.displays.state.set_advances(session.recovery.clock.advance_at(current_time()));
        pbf_wait(run.context, 1000ms);
        run.context.wait_for_all_requests();
    }
    run.displays.state.set_advances(press_advance);
    log_blind_timeline(
        press_advance, schedule, target_advance - run.target_info.timeline.advances_after_accept, run.env
    );

    std::string sequence_failure;
    if (!wait_until_moment(run.context, run.env.logger(), briefcase_time)){
        return "the briefcase press moment passed before it could be sent";
    }
    if (!issue_starter_sequence(
        run.env, run.context, run.settings.starter, run.settings.language, schedule,
        sequence_failure
    )){
        return sequence_failure;
    }
    run.env.log("Blind timeline finished; verifying the obtained starter.", COLOR_BLUE);
    run.displays.state.set_advances(target_advance);
    return {};
}

BdspStarterAttemptResult finish_shiny_starter(
    const AttemptRunState& run,
    const TargetSelectionResult& selection,
    const ShinyDetectionResult& own
){
    record_shiny_io(run.io);
    if (run.io.notification_shiny != nullptr){
        send_program_notification(
            run.env, *run.io.notification_shiny,
            COLOR_STAR_SHINY, "Shiny Starter",
            {{"Advance", std::to_string(selection.target_advance)}, {"Details", selection.target.to_string()}},
            "", own.get_best_screenshot()
        );
    }
    if (run.settings.take_video){
        pbf_wait(run.context, 5000ms);
        pbf_press_button(run.context, BUTTON_CAPTURE, 2000ms, 5000ms);
        run.context.wait_for_all_requests();
    }
    const bool settled = settle_after_shiny(run.env, run.context, run.settings);
    if (!settled){
        record_error_io(run.io);
    }
    return BdspStarterAttemptResult{
        .outcome = BdspAttemptOutcome::Hit,
        .shiny = true,
        .target = selection.target,
        .target_advance = selection.target_advance,
        .settled = settled,
    };
}

BdspStarterAttemptResult evaluate_observed_hit(
    const AttemptRunState& run,
    const BlinkRecovery& recovery,
    const TargetSelectionResult& selection,
    const BdspObservedStarter& observed
){
    const BdspHitIdentification hit = identify_hit_advance(
        recovery.state, run.target_info.pokemon, selection.target_advance, HIT_SEARCH_RADIUS, observed
    );
    if (!hit.success){
        run.env.log("Could not tell which advance that was: " + hit.failure_reason, COLOR_ORANGE);
        if (consistent_with(selection.target, observed)){
            run.env.log("What was read matches the target, so it counts as a hit.", COLOR_BLUE);
            record_hit_io(run.io);
            return {.outcome = BdspAttemptOutcome::Hit, .target = selection.target, .target_advance = selection.target_advance};
        }
        run.env.log("What was read is not the target, so it counts as a miss.", COLOR_ORANGE);
        record_miss_io(run.io);
        return {.outcome = BdspAttemptOutcome::Missed, .target = selection.target, .target_advance = selection.target_advance, .failure_reason = hit.failure_reason};
    }
    if (run.aim.record_offset(run.env.logger(), hit.offset, run.settings.auto_calibrate)){
        run.displays.target.set_correction(run.aim.bias());
    }
    if (hit.offset == 0){
        run.env.log("Landed on advance " + std::to_string(selection.target_advance) + ", as aimed.", COLOR_BLUE);
        record_hit_io(run.io);
        return {.outcome = BdspAttemptOutcome::Hit, .target = selection.target, .target_advance = selection.target_advance};
    }
    run.env.log("Landed on advance " + std::to_string(hit.advance) + " instead of "
        + std::to_string(selection.target_advance) + " — " + (hit.offset > 0 ? "late by " : "early by ")
        + std::to_string(hit.offset > 0 ? hit.offset : -hit.offset) + " advance(s).", COLOR_ORANGE);
    record_miss_io(run.io);

    BdspStaticSearcher searcher(recovery.state, run.target_info.pokemon, 0);
    const BdspPokemonResult actual = searcher.generate(hit.advance);
    if (run.wanted_fn(actual)){
        run.env.log("It passes the filter anyway, so this one will do. Stopping.", COLOR_BLUE);
        return {.outcome = BdspAttemptOutcome::Hit, .target = actual, .target_advance = hit.advance};
    }
    return {.outcome = BdspAttemptOutcome::Missed, .target = selection.target, .target_advance = selection.target_advance};
}

BdspStarterAttemptResult verify_obtained_starter(
    const AttemptRunState& run,
    const BlinkRecovery& recovery,
    const TargetSelectionResult& selection
){
    DoublesShinyDetection wild;
    ShinyDetectionResult own;
    detect_shiny_battle(
        run.env, run.env.console, run.context, wild, own,
        run.io.notification_error != nullptr ? *run.io.notification_error : EventNotificationOption_NOOP(),
        YOUR_POKEMON, 30s, run.settings.use_sound_detection
    );
    if (is_likely_shiny(own.shiny_type)){
        return finish_shiny_starter(run, selection, own);
    }

    BdspObservedStarter observed;
    const ObservedPokemonRequest request{
        .language = run.settings.language,
        .base_stats = starter_base_stats(run.settings.starter),
        .level = 5,
        .shiny = own.shiny_type != ShinyType::UNKNOWN ? ObservedShiny::NotShiny : ObservedShiny::Unknown,
    };
    if (!clear_starter_battle(run.env, run.context) || !read_observed_pokemon(run.env, run.context, request, observed)){
        record_error_io(run.io);
        return {
            .outcome = BdspAttemptOutcome::Unverifiable,
            .target = selection.target,
            .target_advance = selection.target_advance,
            .failure_reason = "could not verify the obtained starter",
        };
    }
    if (run.settings.close_summary_after_read && !close_starter_summary(run.env, run.context)){
        record_error_io(run.io);
    }
    return evaluate_observed_hit(run, recovery, selection, observed);
}

BdspStarterAttemptResult run_single_reset_cycle(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const StarterRngSettings& settings,
    const StarterRngAttemptIO& io,
    RngAim& aim,
    bool& reset
){
    bool started = true;
    if (reset){
        go_home(env.console, context);
        if (!reset_game_from_home(
            env, env.console, context,
            ConsoleSettings::instance().TOLERATE_SYSTEM_UPDATE_MENU_FAST
        )){
            started = false;
        }
    }
    if (!started){
        return BdspStarterAttemptResult{
            .outcome = BdspAttemptOutcome::Abandoned,
            .failure_reason = "failed to reset the game from the Home screen",
        };
    }
    reset = true;
    return run_starter_rng_attempt(env, context, settings, io, aim);
}

} // namespace

BdspStarterAttemptResult run_starter_rng_attempt(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const StarterRngSettings& settings,
    const StarterRngAttemptIO& io,
    RngAim& aim
){
    BlinkCollectionDisplay fallback_collection(true);
    RngStateDisplay fallback_state;
    RngTargetDisplay fallback_target;
    AttemptDisplays displays{
        io.collection_display != nullptr ? *io.collection_display : fallback_collection,
        io.state_display != nullptr ? *io.state_display : fallback_state,
        io.target_display != nullptr ? *io.target_display : fallback_target,
    };
    displays.collection.reset();
    displays.state.reset();
    displays.target.reset();

    const std::function<bool(const BdspPokemonResult&)> wanted_fn = resolve_wanted_predicate(settings);
    const BdspRngTargetInfo& target_info = bdsp_rng_target_info(BdspRngTarget::Starter);
    const AttemptRunState run{env, context, settings, io, target_info, wanted_fn, displays, aim};
    BlinkSampleLog sample_log = open_blink_sample_log(env);
    BdspSkipResult skip;
    if (settings.start_point == BdspStartPoint::Bedroom){
        skip = skip_advances_from_bedroom(env, context, settings, wanted_fn, displays, sample_log);
        if (!skip.success){
            return abandon_attempt(env, displays, skip.failure_reason);
        }
    }

    LakeBlinkSession session;
    if (const std::string err = recover_lake_session(env, context, settings, target_info, displays, session, sample_log); !err.empty()){
        return abandon_attempt(env, displays, err);
    }
    if (skip.success){
        report_skip_arrival(env, skip, session.recovery);
    }

    double lead_seconds = 0;
    const TargetSelectionResult selection = find_starter_target(env, settings, target_info, session.recovery, wanted_fn, aim, lead_seconds);
    if (!selection.success){
        return abandon_attempt(env, displays, selection.failure_reason);
    }
    log_selected_target(run, session, selection);
    if (const std::string err = run_blind_starter_sequence(run, selection, session, lead_seconds); !err.empty()){
        return abandon_attempt(env, displays, err);
    }
    return verify_obtained_starter(run, session.recovery, selection);
}


StarterRngRunResult run_starter_rng_attempts(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const StarterRngSettings& settings,
    const StarterRngAttemptIO& io,
    RngAim& aim
){
    StarterRngRunResult result;
    bool reset = settings.reset_first;
    uint64_t resets = 0;
    BdspStarterAttemptResult last_attempt;

    while (true){
        if (settings.max_resets != 0 && resets >= settings.max_resets){
            env.log("Reached the reset limit.", COLOR_ORANGE);
            resolve_starter_rng_step(last_attempt, resets, settings.max_resets, result);
            break;
        }
        resets++;
        last_attempt = run_single_reset_cycle(env, context, settings, io, aim, reset);
        if (io.stats != nullptr){
            io.stats->resets++;
        }
        if (io.on_reset){
            io.on_reset();
        }
        env.update_stats();

        if (last_attempt.outcome == BdspAttemptOutcome::Hit){
            resolve_starter_rng_step(last_attempt, resets, settings.max_resets, result);
            break;
        }
        if (!last_attempt.failure_reason.empty()){
            result.last_failure_reason = last_attempt.failure_reason;
        }
        result.resets_used = resets;
        if (last_attempt.target_advance != 0){
            result.target = last_attempt.target;
            result.target_advance = last_attempt.target_advance;
        }
    }

    env.update_stats();
    return result;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
