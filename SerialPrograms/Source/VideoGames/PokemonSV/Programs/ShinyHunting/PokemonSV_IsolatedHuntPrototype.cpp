/*  Isolated Hunt Prototype
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Field question: can stationary Let's Go clearing acquire a refused target
 *  with ZR? Start at a prepared anchor, or scout the existing Chargestone court.
 *  Stop after a battle, an unresolved refusal, or bounded empty passes so the
 *  operator can inspect positioning before trying any spawn-reset movement.
 */

#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/ScopeExit.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonSV/Inference/Boxes/PokemonSV_IvJudgeReader.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldSensors.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_SweatBubbleDetector.h"
#include "VideoGames/PokemonSV/Programs/Battles/PokemonSV_Battles.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_Terarium.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_AreaZero.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "PokemonSV_LetsGoTools.h"
#include "PokemonSV_IsolatedHuntPrototype.h"
#include "PokemonSV_IsolatedHuntStats.h"
#include "PokemonSV_IsolatedHuntPowerConsole.h"
#include "PokemonSV_IsolatedHuntConsoleActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

IsolatedHuntPrototype_Descriptor::IsolatedHuntPrototype_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:IsolatedHuntPrototype",
        STRING_POKEMON + " SV", "Isolated Hunt Prototype",
        "", "Supervised target-specific sandwich and bounded hunting. Start at Zero Gate with a healthy lead. Checks Meal Powers and Central before clearing. Set an uphill destination marker and match the lower corridor at its prompt. Bounded ascents fly back to name-verified Central. Route geometry needs field calibration.",
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::VIDEO_AUDIO,
        AllowCommandsWhenRunning::ENABLE_COMMANDS
    )
{}

struct IsolatedHuntPrototype_Descriptor::Stats : public IsolatedHuntStats{};

std::unique_ptr<StatsTracker> IsolatedHuntPrototype_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

namespace{
const char* hunt_start_description(){
    return
        "<b>Start:</b><br>Full hunt: start at Zero Gate as if just flown there, on foot. "
        "Anchor resume: stand at the verified Central uphill patch in the overworld. Skips initial flight; confirm location, marker and heading at the path checkpoint. "
        "Existing sandwich: finish the result screen, pack up the picnic and return to the Zero Gate overworld first. Set the conservative remaining power budget below. "
        "Meal Power and flight selection are automatic. Use manual controls only at the Central path prompt. Other modes are controls. Prepared spot: stand on foot, facing the target patch. "
        "Central Plaza: start as if just flown there; uses the existing Chargestone court approach. "
        "Chespin requires the Canyon diversity upgrade and a verified Grass spawn patch.";
}
}

IsolatedHuntPrototype::IsolatedHuntPrototype()
    : LANGUAGE("<b>Game Language:</b>", IV_READER().languages(), LockMode::LOCK_WHILE_RUNNING, true)
    , START(
        hunt_start_description(),
        {
            {Start::HUNT_FROM_ZERO_GATE, "hunt-from-zero-gate", "Supervised hunt from Zero Gate"},
            {Start::EXISTING_POWER_FROM_ZERO_GATE, "existing-power-zero-gate", "Resume from Zero Gate with existing sandwich"},
            {Start::EXISTING_POWER_AT_ANCHOR, "existing-power-anchor", "Resume near hunting anchor with existing sandwich"},
            {Start::PREPARED_SPOT, "prepared", "Prepared hunting spot (Chespin or Charcadet)"},
            {Start::CENTRAL_PLAZA, "central-plaza", "Central Plaza to Chargestone court"},
            {Start::RETURN_TO_ZERO_GATE, "return-zerogate", "Return from Area Zero to Zero Gate (travel only)"},
        },
        LockMode::LOCK_WHILE_RUNNING, Start::HUNT_FROM_ZERO_GATE
    )
    , SANDWICH_OPTIONS(&LANGUAGE)
    , EXISTING_POWER_SECONDS("<b>Existing sandwich: remaining power budget (seconds):</b><br>Resume only. "
        "Set below the in-game remaining time, allowing for recovery and leaving a safety margin. "
        "Includes verification, flight and path setup. Power type and level are checked automatically; the timer is not read.",
        LockMode::LOCK_WHILE_RUNNING, 300, 30, 1740)
    , PATCH_VERIFIED("<b>Central path inspected:</b><br>At the path pause, position the player on the demonstrated "
        "lower uphill corridor by Central, on foot. Set a destination marker below the river bank. Check only after matching terrain, heading and safe clearing bounds.",
        LockMode::UNLOCK_WHILE_RUNNING, false)
    , RUN_SECONDS(
        "<b>Clearing time limit (seconds):</b><br>Starts after the approach. Stops between passes; each pass takes at most about 17 seconds.",
        LockMode::LOCK_WHILE_RUNNING, 120, 15, 600
    )
    , MAX_ROUNDS("<b>Maximum clearing rounds per target:</b>", LockMode::LOCK_WHILE_RUNNING, 100, 1, 1000)
    , EMPTY_PASSES(
        "<b>Stop after consecutive passes without a kill:</b><br>Stops for inspection without resetting nearby spawns.",
        LockMode::LOCK_WHILE_RUNNING, 3, 1, 10
    )
    , ASCENT_SEGMENTS("<b>Central ascent: maximum segments before flight reset:</b><br>Conservative cap; does not prove arrival or safe geometry.",
        LockMode::LOCK_WHILE_RUNNING, 6, 1, 30)
    , ASCENT_SEGMENT_MS("<b>Central ascent: forward segment (ms):</b>", LockMode::LOCK_WHILE_RUNNING, 600, 100, 2000)
    , ASCENT_CLEAR_MS("<b>Central ascent: clearing wait (ms):</b>", LockMode::LOCK_WHILE_RUNNING, 5000, 2000, 15000)
    , NOTIFICATIONS({
        &ENCOUNTER_OPTIONS.NOTIFICATION_NONSHINY,
        &ENCOUNTER_OPTIONS.NOTIFICATION_SHINY,
        &ENCOUNTER_OPTIONS.NOTIFICATION_CATCH_SUCCESS,
        &ENCOUNTER_OPTIONS.NOTIFICATION_CATCH_FAILED,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    add_options();
}

bool IsolatedHuntPrototype::existing_power() const{
    return START == Start::EXISTING_POWER_FROM_ZERO_GATE || START == Start::EXISTING_POWER_AT_ANCHOR;
}

void IsolatedHuntPrototype::add_options(){
    SANDWICH_OPTIONS.TYPE.set_visibility(ConfigOptionState::HIDDEN);
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(START);
    PA_ADD_OPTION(HUNTS);
    PA_ADD_OPTION(SANDWICH_OPTIONS);
    PA_ADD_OPTION(EXISTING_POWER_SECONDS);
    PA_ADD_OPTION(PATCH_VERIFIED);
    PA_ADD_OPTION(MAX_ROUNDS);
    PA_ADD_OPTION(RUN_SECONDS);
    PA_ADD_OPTION(EMPTY_PASSES);
    PA_ADD_OPTION(ASCENT_SEGMENTS);
    PA_ADD_OPTION(ASCENT_SEGMENT_MS);
    PA_ADD_OPTION(ASCENT_CLEAR_MS);
    PA_ADD_OPTION(ENCOUNTER_OPTIONS);
    PA_ADD_OPTION(NOTIFICATIONS);
}

bool IsolatedHuntPrototype::verify_setup(VideoStream& stream, ProControllerContext& context,
    const IsolatedHuntRequest& request, IsolatedHuntVerification stage
){
    if (stage == IsolatedHuntVerification::MEAL_POWER){
        return check_isolated_meal_power(stream, context, LANGUAGE, request.type, SANDWICH_OPTIONS.USE_HERBA ? 3 : 2);
    }
    const auto started = current_time();
    PATCH_VERIFIED = false;
    stream.log("SUPERVISED: stand on foot at the lower Central uphill corridor, set a marker with a river buffer, and match the safe heading. Then check Central path inspected.", COLOR_BLUE);
    const WallClock deadline = current_time() + 110s;
    while (!PATCH_VERIFIED && current_time() < deadline){ context.wait_for(250ms); }
    const bool accepted = PATCH_VERIFIED;
    PATCH_VERIFIED = false;
    stream.log("Isolated verification: stage=Central path, method=supervised, accepted=" + std::to_string(accepted) +
        ", elapsed ms=" + std::to_string(std::chrono::duration_cast<Milliseconds>(current_time() - started).count()));
    if (!accepted){ return false; }
    OverworldWatcher overworld(stream.logger(), COLOR_BLUE);
    return wait_until(stream, context, 5000ms, {overworld}) == 0;
}

bool IsolatedHuntPrototype::run_one_hunt(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context, const IsolatedHuntRequest& request
){
    auto& stats = env.current_stats<IsolatedHuntPrototype_Descriptor::Stats>();
    ConsoleIsolatedHuntActions actions(env, context, stats, LANGUAGE, SANDWICH_OPTIONS, ENCOUNTER_OPTIONS, request);
    if (START == Start::EXISTING_POWER_AT_ANCHOR){ actions.resume_at_anchor(); }
    actions.set_verifier([&](ProControllerContext& stage_context, const IsolatedHuntRequest& hunt, IsolatedHuntVerification stage){
        return verify_setup(env.console, stage_context, hunt, stage);
    });
    const IsolatedHuntLimits limits{
        .max_rounds = MAX_ROUNDS, .max_empty_rounds = EMPTY_PASSES,
        .duration_ms = (uint64_t)RUN_SECONDS * 1000,
        .power_duration_ms = existing_power() ? (uint64_t)EXISTING_POWER_SECONDS * 1000 : 29 * 60 * 1000,
        .ascent = {.enabled = request.location == IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL,
            .segments_per_ascent = ASCENT_SEGMENTS, .segment_ms = ASCENT_SEGMENT_MS, .clearing_wait_ms = ASCENT_CLEAR_MS},
        .use_existing_power = existing_power(),
        .resume_at_anchor = START == Start::EXISTING_POWER_AT_ANCHOR,
    };
    env.console.log("Isolated setup: existing power=" + std::to_string(limits.use_existing_power) +
        ", power budget ms=" + std::to_string(limits.power_duration_ms));
    const auto result = run_isolated_hunt(request, limits, actions);
    actions.report(request, result);
    if (!can_leave_isolated_hunt(result.stop)){
        env.console.log("Stopped at the current location for inspection; no departure after refusal, policy stop or failure.", COLOR_BLUE);
        return false;
    }
    if (!actions.return_to_zero_gate(request)){
        if (actions.battle_pending()){
            actions.handle_battle(request);
        }
        env.console.log("Return flight failed or was interrupted. Inspect location before restarting AZP.", COLOR_RED);
        return false;
    }
    env.console.log("Return flight completed and saved. Verify Zero Gate before handing back to AZP.");
    return true;
}

void IsolatedHuntPrototype::check_power_language(SingleSwitchProgramEnvironment& env) const{
    if (LANGUAGE != Language::English && LANGUAGE != Language::ChineseTraditional){
        throw UserSetupError(env.logger(), "Automatic Meal Power verification supports English and Traditional Chinese only.");
    }
}

void IsolatedHuntPrototype::run_hunts(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const auto plan = make_isolated_hunt_plan(HUNTS.snapshot());
    if (!plan.error.empty()){
        throw UserSetupError(env.logger(), plan.error);
    }
    if (existing_power() && plan.hunts.size() != 1){
        throw UserSetupError(env.logger(), "Existing sandwich resume requires exactly one enabled hunt.");
    }
    if (START == Start::EXISTING_POWER_AT_ANCHOR && plan.hunts.front().location != IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL){
        throw UserSetupError(env.logger(), "Anchor resume currently supports the supervised Central corridor only.");
    }
    if (LANGUAGE == Language::None){
        throw UserSetupError(env.logger(), "Select the game language before starting.");
    }
    check_power_language(env);
    assert_16_9_720p_min(env.logger(), env.console);
    require_player(env.console, context, BUTTON_LCLICK);
    OverworldWatcher overworld(env.logger(), COLOR_BLUE);
    if (wait_until(env.console, context, 5000ms, {overworld}) < 0){
        throw UserSetupError(env.logger(), "Start at the Zero Gate fly spot in the overworld.");
    }
    for (const auto& request : plan.hunts){
        if (!run_one_hunt(env, context, request)){
            break;
        }
    }
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}

bool IsolatedHuntPrototype::approach(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    if (START == Start::RETURN_TO_ZERO_GATE){
        env.console.log("Prototype state: returning from Area Zero to Zero Gate.");
        return_to_outside_zero_gate(env.program_info(), env.console, context);
        return false;
    }
    if (START == Start::CENTRAL_PLAZA){
        env.console.log("Prototype state: approach Chargestone court.");
        central_to_chargestone(env.program_info(), env.console, context);
        pbf_wait(context, 3000ms);
        // Match the existing BBQ court approach; its Chespin footprint still needs field verification.
        pbf_press_button(context, BUTTON_L, 160ms, 400ms);
        pbf_move_left_joystick(context, {0, +1}, 2000ms, 400ms);
        pbf_move_left_joystick(context, {+1, 0}, 1440ms, 400ms);
        pbf_press_button(context, BUTTON_L, 160ms, 400ms);
        context.wait_for_all_requests();
    }
    return true;
}

bool IsolatedHuntPrototype::clear_pass(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context, OverworldSensors& sensors, uint8_t& empty
){
    auto& stats = env.current_stats<IsolatedHuntPrototype_Descriptor::Stats>();
    const uint64_t before = stats.m_kills.load();
    env.console.log("Prototype state: clearing pass " + std::to_string(stats.passes.load() + 1));
    SweatBubbleWatcher bubble(COLOR_GREEN);
    int refused = run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            pbf_press_button(context, BUTTON_R, 160ms, 1600ms);
        },
        {bubble}
    );
    if (refused == 0){
        stats.refusals++;
        env.update_stats();
        env.console.log("Prototype state: refusal detected; trying ZR. Refusal does not confirm a shiny.");
        pbf_mash_button(context, BUTTON_ZR, 5000ms);
        pbf_wait(context, 10000ms);
        context.wait_for_all_requests();
        return false;
    }
    // A fixed observation window keeps a long kill chain from extending a pass indefinitely.
    pbf_wait(context, 12000ms);
    context.wait_for_all_requests();
    sensors.throw_if_no_sound();
    stats.passes++;
    uint64_t kills = stats.m_kills.load() - before;
    empty = kills == 0 ? empty + 1 : 0;
    env.update_stats();
    env.console.log("Prototype pass: kills=" + std::to_string(kills) +
        ", consecutive empty passes=" + std::to_string(empty));
    return true;
}

std::string IsolatedHuntPrototype::clear_spot(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context, OverworldSensors& sensors
){
    const auto started = current_time();
    auto& stats = env.current_stats<IsolatedHuntPrototype_Descriptor::Stats>();
    const auto seconds_before = stats.hunt_seconds.load();
    const auto kills_before = stats.m_kills.load();
    auto update_time = [&]{
        const auto elapsed = std::chrono::duration_cast<Milliseconds>(current_time() - started).count();
        stats.hunt_seconds.store(seconds_before + elapsed / 1000);
        env.update_stats();
        return elapsed;
    };
    ScopeExit report([&]{
        const auto elapsed = update_time();
        const auto kills = stats.m_kills.load() - kills_before;
        env.console.log("Isolated clearing summary: mode=prepared, elapsed ms=" + std::to_string(elapsed) +
            ", detected kills=" + std::to_string(kills) + ", kills/min=" +
            std::to_string(elapsed == 0 ? 0.0 : 60000.0 * kills / elapsed));
    });
    WallClock deadline = started + std::chrono::seconds(RUN_SECONDS);
    uint8_t empty = 0;
    while (current_time() < deadline){
        update_time();
        if (!clear_pass(env, context, sensors, empty)){
            return "Refusal acquisition timed out without a battle. Inspect the target and positioning.";
        }
        if (empty >= EMPTY_PASSES){
            return "No clearing progress. Inspect spawn density, lead health and target reachability.";
        }
    }
    return "Clearing time limit reached.";
}

bool IsolatedHuntPrototype::process_encounter(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context, OverworldSensors& sensors,
    OverworldBattleTracker& battle_tracker, LetsGoEncounterBotTracker& encounters
){
    auto& stats = env.current_stats<IsolatedHuntPrototype_Descriptor::Stats>();
    env.console.log("Prototype state: battle detected; using shared encounter/shiny handling.");
    sensors.throw_if_no_sound();
    stats.m_encounters++;
    env.update_stats();
    encounters.encounter_rate_tracker().report_encounter();
    bool caught = false, should_save = false;
    try{
        process_battle(caught, should_save, env, ENCOUNTER_OPTIONS, env.console, context,
            battle_tracker, encounters.encounter_frequencies(), stats.m_shinies, LANGUAGE);
    }catch (ProgramFinishedException&){
        env.console.log("Prototype stopped by encounter policy. Leaving the battle for inspection.", COLOR_BLUE);
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
        return true;
    }
    if (should_save){
        save_game_from_overworld(env.program_info(), env.console, context);
    }
    return false;
}

void IsolatedHuntPrototype::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    if (START == Start::HUNT_FROM_ZERO_GATE || existing_power()){
        run_hunts(env, context);
        return;
    }
    if (START != Start::RETURN_TO_ZERO_GATE){ check_power_language(env); }
    auto& stats = env.current_stats<IsolatedHuntPrototype_Descriptor::Stats>();
    assert_16_9_720p_min(env.logger(), env.console);
    require_player(env.console, context, BUTTON_LCLICK);
    OverworldWatcher overworld(env.logger(), COLOR_BLUE);
    if (wait_until(env.console, context, 5000ms, {overworld}) < 0){
        throw UserSetupError(env.logger(), "Start in the overworld at the selected anchor.");
    }
    // Stop audio inference before destroying the tracker captured by its kill callback.
    std::unique_ptr<LetsGoEncounterBotTracker> encounters;
    OverworldSensors sensors(env.logger(), env.console, context);
    OverworldBattleTracker battle_tracker(env.logger(), sensors);
    encounters = std::make_unique<LetsGoEncounterBotTracker>(env, env.console, stats, sensors.lets_go_kill);
    std::string stop_reason;
    NormalBattleMenuWatcher battle_menu(COLOR_RED);
    int result = run_until<ProControllerContext>(
        env.console, context,
        [&](ProControllerContext& context){
            if (!approach(env, context)){
                stop_reason = "Returned to Zero Gate. Ready for the release AZP program.";
                return;
            }
            const auto plan = make_isolated_hunt_plan(HUNTS.snapshot());
            if (!plan.error.empty()){
                throw UserSetupError(env.logger(), plan.error);
            }
            if (!verify_setup(env.console, context, plan.hunts.front(), IsolatedHuntVerification::MEAL_POWER)){
                stop_reason = "Active Meal Power was not verified; no clearing.";
                return;
            }
            stop_reason = clear_spot(env, context, sensors);
        },
        {battle_menu}
    );
    if (result == 0){
        if (process_encounter(env, context, sensors, battle_tracker, *encounters)){
            return;
        }
        // Do not resume a timed movement path after an encounter changes the anchor.
        stop_reason = "Battle handled. Inspect positioning before another run.";
    }
    env.console.log("Prototype stopped: " + stop_reason, COLOR_BLUE);
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}

}
}
}
