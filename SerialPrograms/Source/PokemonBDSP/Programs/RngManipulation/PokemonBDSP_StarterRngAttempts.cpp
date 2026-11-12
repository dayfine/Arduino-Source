/*  BDSP Starter RNG Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <cmath>
#include <memory>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/PrettyPrint.h"
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


BdspSkipResult skip_advances_from_bedroom(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const StarterRngSettings& settings,
    const std::function<bool(const BdspPokemonResult&)>& wanted_fn,
    BlinkCollectionDisplay& collection_display,
    RngStateDisplay& state_display,
    RngTargetDisplay& target_display
){
    BdspSkipResult skip;
    const BdspRngTargetInfo& target_info = bdsp_rng_target_info(BdspRngTarget::Starter);

    //  Expected wait duration at the lake after a long skip
    double longest_skip = settings.max_skip_minutes * BDSP_SKIP_ADVANCES_PER_MINUTE;
    double lake_minutes = bdsp_lake_advances_needed(longest_skip)
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

    BlinkRecovery recovery = recover_state_from_blinks(
        env, context, watchers, callbacks, collection_display, bedroom_config
    );
    if (!recovery.success){
        skip.failure_reason = "could not determine the RNG state in the bedroom: "
            + recovery.failure_reason;
        return skip;
    }
    skip.state = recovery.state;
    env.log("Bedroom RNG state: " + recovery.state.to_string(), COLOR_BLUE);
    state_display.set_state(recovery.state, recovery.clock.anchor_advance);
    state_display.set_confidence_unique();

    //  take into account remaining navigation time and the desired buffer
    uint64_t soonest = recovery.clock.advance_at(current_time())
        + (uint64_t)BDSP_SKIP_NAVIGATION_ADVANCES + bdsp_skip_buffer(0);
    uint64_t furthest = soonest
        + (uint64_t)(settings.max_skip_minutes * BDSP_SKIP_ADVANCES_PER_MINUTE);

    BdspStaticSearcher searcher(recovery.state, target_info.pokemon, 0);
    std::vector<BdspRngHit> hits = searcher.scan(
        soonest, furthest,
        wanted_fn,
        true, &context
    );
    if (hits.empty()){
        skip.failure_reason = "no target within " + std::to_string(settings.max_skip_minutes)
            + " minutes of skipping";
        return skip;
    }
    const BdspRngHit& hit = hits[0];
    skip.target_advance = hit.advances;
    target_display.set_target(hit.result, hit.advances);

    uint64_t start = recovery.clock.advance_at(current_time());

    //  The buffer depends on how far the skip runs, so size it from the raw distance.
    double raw_skip = (double)hit.advances - (double)start - BDSP_SKIP_NAVIGATION_ADVANCES;
    uint64_t buffer = bdsp_skip_buffer(raw_skip);
    double to_skip = raw_skip - (double)buffer;
    if (to_skip < 0){
        to_skip = 0;
    }
    double wait_seconds = to_skip * 60 / BDSP_SKIP_ADVANCES_PER_MINUTE;
    skip.buffer = buffer;

    env.log("Target advance " + std::to_string(hit.advances) + ": " + hit.result.to_string(),
        COLOR_BLUE);
    env.log("Skipping " + std::to_string((uint64_t)to_skip) + " advances from "
        + std::to_string(start) + ", which is " + tostr_fixed(wait_seconds, 1)
        + "s on the spot, stopping " + std::to_string(buffer)
        + " advances short to avoid overshooting.", COLOR_BLUE);

    navigate_bedroom_to_skip_spot(env.logger(), context);

    WallClock leave_at = current_time() + std::chrono::duration_cast<WallDuration>(
        std::chrono::duration<double>(wait_seconds)
    );

    while (current_time() + 2s < leave_at){
        WallClock now = current_time();
        WallClock until = std::min(leave_at - 2s, now + 180s);
        pbf_wait(context, std::chrono::duration_cast<Milliseconds>(until - now));
        context.wait_for_all_requests();
    }
    WallClock now = current_time();
    if (now < leave_at){
        pbf_wait(context, std::chrono::duration_cast<Milliseconds>(leave_at - now));
        context.wait_for_all_requests();
    }

    navigate_skip_spot_to_lakefront(env.logger(), context);
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

    BlinkCollectionDisplay& collection_display = io.collection_display != nullptr
        ? *io.collection_display
        : fallback_collection;
    RngStateDisplay& state_display = io.state_display != nullptr
        ? *io.state_display
        : fallback_state;
    RngTargetDisplay& target_display = io.target_display != nullptr
        ? *io.target_display
        : fallback_target;

    collection_display.reset();
    state_display.reset();
    target_display.reset();

    auto record_miss = [&](){
        if (io.stats != nullptr){
            io.stats->missed++;
        }
        if (io.on_miss){
            io.on_miss();
        }
    };
    auto record_hit = [&](){
        if (io.stats != nullptr){
            io.stats->hits++;
        }
        if (io.on_hit){
            io.on_hit();
        }
    };
    auto record_shiny = [&](){
        if (io.stats != nullptr){
            io.stats->shinies++;
        }
        if (io.on_shiny){
            io.on_shiny();
        }
    };
    auto record_error = [&](){
        if (io.stats != nullptr){
            io.stats->errors++;
        }
        if (io.on_error){
            io.on_error();
        }
    };

    auto abandon = [&](const std::string& reason) -> BdspStarterAttemptResult{
        env.log("Abandoning this attempt: " + reason, COLOR_ORANGE);
        target_display.set_note(reason);
        return BdspStarterAttemptResult{
            .outcome = BdspAttemptOutcome::Abandoned,
            .failure_reason = reason,
        };
    };

    std::function<bool(const BdspPokemonResult&)> wanted_fn = resolve_wanted_predicate(settings);
    const BdspRngTargetInfo& target_info = bdsp_rng_target_info(BdspRngTarget::Starter);

    BdspSkipResult skip;
    if (settings.start_point == BdspStartPoint::Bedroom){
        skip = skip_advances_from_bedroom(
            env, context, settings, wanted_fn,
            collection_display, state_display, target_display
        );
        if (!skip.success){
            return abandon(skip.failure_reason);
        }
    }

    navigate_to_lake_blinks(env.logger(), context);

    std::vector<BdspEyeTemplate> setups = lake_eye_templates(settings.player_model);
    std::vector<ImageRGB32> eyes = load_eye_templates(setups);

    //  The rival's dialogue box being up is what says the pair have stopped walking.
    ShortDialogDetector dialog;
    BlinkSceneConfig scene;
    scene.frame_filter = [&dialog](const ImageViewRGB32& frame){
        return dialog.detect(frame);
    };
    scene.press_retries = LAKE_BLINKS_PRESS_RETRIES;

    if (!wait_for_blink_scene(env, context, setups, eyes, scene, "Lake blinks")){
        return abandon("never reached the lake blink position");
    }

    //  Now blink watchers can be created safely
    std::vector<std::unique_ptr<EyeBlinkWatcher>> watchers;
    std::vector<PeriodicInferenceCallback> callbacks;
    make_blink_watchers(setups, eyes, watchers, callbacks);

    BlinkRecoveryConfig blink_config;
    blink_config.npcs = target_info.observation_npcs;

    BlinkRecovery recovery = recover_state_from_blinks(
        env, context, watchers, callbacks, collection_display, blink_config
    );
    if (!recovery.success){
        return abandon("could not determine the RNG state: " + recovery.failure_reason);
    }

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
    state_display.set_state(recovery.state, recovery.clock.anchor_advance);
    state_display.set_confidence_unique();

    if (skip.success){
        report_skip_arrival(env, skip, recovery);
    }

    const BdspTimelineContext& timeline = target_info.timeline;

    NavigationTimings timings;
    timings.ready_seconds = seconds_from_briefcase_to_starly_ready();
    timings.move_seconds = seconds_to_move_to_starter(settings.starter) + 0.25; // small buffer
    timings.prompt_seconds = seconds_from_select_to_confirm();
    timings.guard_seconds = GAP_GUARD_SECONDS;

    BdspStaticSearcher searcher(recovery.state, target_info.pokemon, 0);

    //  A press cannot be sooner than the walk to the pre-briefcase position takes
    double lead_seconds = seconds_to_pre_briefcase() + 15;
    uint64_t lead_advances = (uint64_t)(
        lead_seconds * recovery.clock.npcs / recovery.clock.tick_seconds
    ) + 1;

    TargetSearchRequest search;
    search.state = recovery.state;
    search.timeline = timeline;
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
    if (!selection.success){
        return abandon(selection.failure_reason);
    }

    uint64_t press_advance = selection.press_advance;
    uint64_t target_advance = selection.target_advance;
    const PressSchedule& schedule = selection.schedule;
    const BdspPokemonResult& target = selection.target;

    double wait_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(
        recovery.clock.time_of_advance(press_advance) - current_time()
    ).count();
    env.log("Target advance " + std::to_string(target_advance) + ": " + target.to_string(),
        COLOR_BLUE);
    env.log("Pressing at advance " + std::to_string(press_advance) + ", in "
        + tostr_default(wait_seconds) + "s.", COLOR_BLUE);
    env.log("Schedule spans " + std::to_string(schedule.span) + " advances, confirming after "
        + std::to_string(schedule.blinks_before_confirm) + " Starly blink(s), "
        + tostr_default(schedule.confirm_seconds) + "s in"
        + aim.describe_correction() + ".", COLOR_BLUE);
    target_display.set_target(target, target_advance);

    hold_and_reanchor(
        env, context, watchers, callbacks, recovery, blink_config,
        press_advance, lead_seconds, state_display
    );

    navigate_to_pre_briefcase(env.logger(), context);

    //  FIRST PRESS, centred in the advance rather than aimed right at the beginning of it
    WallClock briefcase_time = recovery.clock.middle_of_advance(press_advance);

    while (current_time() + 3s < briefcase_time){
        state_display.set_advances(recovery.clock.advance_at(current_time()));
        pbf_wait(context, 1000ms);
        context.wait_for_all_requests();
    }

    state_display.set_advances(press_advance);

    env.log("Timeline starts at advance " + std::to_string(press_advance)
        + ". Blind from here: Starly press at " + tostr_default(schedule.starly_seconds)
        + "s, selecting at " + tostr_default(schedule.select_seconds)
        + "s, confirming at " + tostr_default(schedule.confirm_seconds)
        + "s, on advance " + std::to_string(target_advance - timeline.advances_after_accept)
        + ".", COLOR_BLUE);

    std::string sequence_failure;
    if (!wait_until_moment(context, env.logger(), briefcase_time)){
        return abandon("the briefcase press moment passed before it could be sent");
    }
    if (!issue_starter_sequence(
        context, settings.starter,
        schedule.starly_seconds, schedule.select_seconds, schedule.confirm_seconds,
        sequence_failure
    )){
        return abandon(sequence_failure);
    }
    env.log("Timeline finished; the starter should be in hand.", COLOR_BLUE);

    state_display.set_advances(target_advance);

    //  Check what was actually obtained.
    DoublesShinyDetection wild;
    ShinyDetectionResult own;
    detect_shiny_battle(
        env, env.console, context,
        wild, own,
        io.notification_error != nullptr ? *io.notification_error : EventNotificationOption_NOOP(),
        YOUR_POKEMON,
        30s,
        settings.use_sound_detection
    );
    bool shiny_known = own.shiny_type != ShinyType::UNKNOWN;

    if (is_likely_shiny(own.shiny_type)){
        record_shiny();
        if (io.notification_shiny != nullptr){
            send_program_notification(
                env, *io.notification_shiny,
                COLOR_STAR_SHINY, "Shiny Starter",
                {{"Advance", std::to_string(target_advance)}, {"Details", target.to_string()}},
                "", own.get_best_screenshot()
            );
        }
        if (settings.take_video){
            pbf_wait(context, 5000ms);
            pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 5000ms);
            context.wait_for_all_requests();
        }
        if (settings.finish_battle_on_shiny){
            if (!clear_starter_battle(env, context)){
                record_error();
            }else if (settings.close_summary_after_read){
                if (!open_menu(env, context)){
                    record_error();
                }else{
                    pbf_press_button(context, BUTTON_X, 160ms, 1000ms);
                    context.wait_for_all_requests();
                    env.log("Closed the menu.");
                }
            }
        }
        return BdspStarterAttemptResult{
            .outcome = BdspAttemptOutcome::Hit,
            .shiny = true,
            .target = target,
            .target_advance = target_advance,
        };
    }

    BdspObservedStarter observed;
    if (!clear_starter_battle(env, context)
        || !read_observed_pokemon(
            env, context, settings.language,
            starter_base_stats(settings.starter), 5, false, shiny_known, observed
        )
    ){
        record_error();
        return BdspStarterAttemptResult{
            .outcome = BdspAttemptOutcome::Unverifiable,
            .shiny = false,
            .target = target,
            .target_advance = target_advance,
            .failure_reason = "could not verify the obtained starter",
        };
    }

    if (settings.close_summary_after_read){
        if (!close_starter_summary(env, context)){
            record_error();
        }
    }

    BdspHitIdentification hit = identify_hit_advance(
        recovery.state, target_info.pokemon, target_advance, HIT_SEARCH_RADIUS, observed
    );
    if (!hit.success){
        env.log("Could not tell which advance that was: " + hit.failure_reason, COLOR_ORANGE);
        if (consistent_with(target, observed)){
            env.log("What was read matches the target, so it counts as a hit.", COLOR_BLUE);
            record_hit();
            return BdspStarterAttemptResult{
                .outcome = BdspAttemptOutcome::Hit,
                .shiny = false,
                .target = target,
                .target_advance = target_advance,
            };
        }
        env.log("What was read is not the target, so it counts as a miss.", COLOR_ORANGE);
        record_miss();
        return BdspStarterAttemptResult{
            .outcome = BdspAttemptOutcome::Missed,
            .shiny = false,
            .target = target,
            .target_advance = target_advance,
            .failure_reason = hit.failure_reason,
        };
    }
    if (aim.record_offset(env.logger(), hit.offset, settings.auto_calibrate)){
        target_display.set_correction(aim.bias());
    }

    if (hit.offset == 0){
        env.log("Landed on advance " + std::to_string(target_advance) + ", as aimed.",
            COLOR_BLUE);
        record_hit();
        return BdspStarterAttemptResult{
            .outcome = BdspAttemptOutcome::Hit,
            .shiny = false,
            .target = target,
            .target_advance = target_advance,
        };
    }

    env.log(
        "Landed on advance " + std::to_string(hit.advance) + " instead of "
        + std::to_string(target_advance) + " — "
        + (hit.offset > 0 ? "late by " : "early by ")
        + std::to_string(hit.offset > 0 ? hit.offset : -hit.offset) + " advance(s).",
        COLOR_ORANGE
    );
    record_miss();

    BdspPokemonResult actual = searcher.generate(hit.advance);
    if (wanted_fn(actual)){
        env.log("It passes the filter anyway, so this one will do. Stopping.", COLOR_BLUE);
        return BdspStarterAttemptResult{
            .outcome = BdspAttemptOutcome::Hit,
            .shiny = false,
            .target = actual,
            .target_advance = hit.advance,
        };
    }
    return BdspStarterAttemptResult{
        .outcome = BdspAttemptOutcome::Missed,
        .shiny = false,
        .target = target,
        .target_advance = target_advance,
    };
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

        if (started){
            reset = true;
            last_attempt = run_starter_rng_attempt(env, context, settings, io, aim);
        }else{
            last_attempt = BdspStarterAttemptResult{
                .outcome = BdspAttemptOutcome::Abandoned,
                .failure_reason = "failed to reset the game from the Home screen",
            };
        }

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


}
}
}
