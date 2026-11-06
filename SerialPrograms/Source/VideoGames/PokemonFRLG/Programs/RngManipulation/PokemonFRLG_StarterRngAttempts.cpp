/*  Starter RNG Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <set>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattlePokemonDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleLevelUpReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_HardReset.h"
#include "PokemonFRLG_RngNavigation.h"
#include "PokemonFRLG_StarterRngAttempts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const int64_t FIXED_SEED_OFFSET = -845; // milliseconds, approximate

const uint64_t CONTINUE_SCREEN_FRAMES = 200;

const uint64_t INITIAL_ADVANCES_RADIUS = 1024;

const uint8_t MAX_HISTORY_LENGTH = 10;

const int16_t GENDER_THRESHOLD = 30;

const std::set<std::string>& starter_species(){
    static const std::set<std::string> SPECIES_LIST = { "bulbasaur", "squirtle", "charmander" };
    return SPECIES_LIST;
}

} // namespace


BaseStats starter_base_stats(StarterSpecies starter){
    switch (starter){
    case StarterSpecies::bulbasaur:
        return { 45, 49, 49, 65, 65, 45 };
    case StarterSpecies::squirtle:
        return { 44, 48, 65, 50, 64, 43 };
    case StarterSpecies::charmander:
        return { 39, 52, 43, 60, 50, 65 };
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown starter.");
}

EVs rival_ev_yield(StarterSpecies starter){
    Pokemon::EVs evyield = {0, 0, 0, 0, 0, 0};
    switch(starter){
        case StarterSpecies::bulbasaur:
            evyield.speed = 1;   // from charmander
            break;
        case StarterSpecies::squirtle:
            evyield.spatk = 1;   // from bulbasaur
            break;
        case StarterSpecies::charmander:
            evyield.defense = 1; // from squirtle
    }
    return evyield;
}

int steps_left_to_lab_door(StarterSpecies starter){
    switch(starter){
    case StarterSpecies::bulbasaur:
        return 2;
    case StarterSpecies::squirtle:
        return 3;
    case StarterSpecies::charmander:
        return 4;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown starter.");
}


StarterRngOutcome identify_shiny(uint16_t seed, uint64_t advances, const std::vector<AdvRngState>& hits){
    if (hits.empty()){
        return StarterRngOutcome::shiny;
    }
    const bool on_target = std::any_of(hits.begin(), hits.end(), [&](const AdvRngState& hit){
        return have_hit_target(seed, advances, hit);
    });
    return on_target ? StarterRngOutcome::shiny_on_target : StarterRngOutcome::shiny_off_target;
}

std::optional<AdvRngState> reset_hit(
    const RngUncertainHistory& uncertain, const RngCalibrationHistory& calibration, const std::vector<AdvRngState>& hits
){
    //  update_history() clears `uncertain` only when it takes a hit, and adds
    //  the reset's last after the earlier ones it settles.
    if (hits.empty() || !uncertain.results.empty() || calibration.results.empty()){
        return std::nullopt;
    }
    return calibration.results.back();
}


StarterRngStats::StarterRngStats()
    : resets(m_stats["Resets"])
    , shinies(m_stats["Shinies"])
    , nonshiny(m_stats["Non-Shiny Hits"])
    , wildshinies(m_stats["Wild Shinies"])
    , errors(m_stats["Errors"])
{
    m_display_order.emplace_back("Resets");
    m_display_order.emplace_back("Shinies");
    m_display_order.emplace_back("Non-Shiny Hits", HIDDEN_IF_ZERO);
    m_display_order.emplace_back("Wild Shinies", HIDDEN_IF_ZERO);
    m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
}


StarterRngAttempts::StarterRngAttempts(
    StarterRngTarget target,
    StarterRngSettings settings,
    const StarterRngDisplays& displays,
    const StarterRngNotifications& notifications,
    StarterRngStats& stats
)
    : m_target(std::move(target))
    , m_settings(std::move(settings))
    , m_displays(displays)
    , m_notifications(notifications)
    , m_stats(stats)
    , m_base_stats(starter_base_stats(m_target.starter))
    , m_searcher(m_target.seed, m_target.advances, AdvRngMethod::Method1)
    , m_launch_delay(INITIAL_LAUNCH_DELAY)
{}


StarterRngResult StarterRngAttempts::run(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("RNG Target: " + std::to_string(static_cast<size_t>(m_target.starter)));
    env.log("Target Seed: " + to_hex_string(m_target.seed));
    env.log("Target Advances: " + std::to_string(m_target.advances));

    AdvPokemonResult target_result = m_searcher.generate_pokemon();
    m_displays.target.set_target(target_result, GENDER_THRESHOLD);
    log_target_pokemon(env.console, target_result);

    m_calibrations = {
        m_displays.calibration.seed_calibration / FRLG_FRAME_DURATION,
        m_displays.calibration.csf_calibration,
        m_displays.calibration.advances_calibration
    };
    log_calibrations(env.console, m_calibrations, true);

    while (true){
        std::optional<StarterRngOutcome> outcome = finished(env);
        if (outcome){
            StarterRngResult result{.outcome = *outcome, .pokemon = std::nullopt};
            if (*outcome == StarterRngOutcome::target_hit){
                result.pokemon = target_result;
            }
            return result;
        }

        reset_game(env, context);

        if (check_shiny_starter(env, context)){
            const StarterRngOutcome shiny = m_settings.identify_shiny ? identify_shiny_starter(env, context) : StarterRngOutcome::shiny;
            const bool on_target = shiny == StarterRngOutcome::shiny_on_target;
            report_read(true);
            return StarterRngResult{.outcome = shiny, .pokemon = on_target ? std::optional(target_result) : std::nullopt};
        }

        run_attempt(env, context);
        report_read(false);
    }
}

void StarterRngAttempts::report_read(bool shiny){
    if (m_settings.on_read){
        m_settings.on_read(RngResetRead{
            .resets = m_stats.resets.load(),
            .hits = m_search_hits,
            .hit = shiny ? std::nullopt : reset_hit(m_uncertain_history, m_calibration_history, m_search_hits),
            .shiny = shiny,
        });
    }
}

std::optional<StarterRngOutcome> StarterRngAttempts::finished(SingleSwitchProgramEnvironment& env){
    if (m_calibration_history.results.size() > 0){
        env.log("Checking for nonshiny target hit...");
        if (have_hit_target(m_target.seed, m_target.advances, m_calibration_history.results.back())){
            env.log("Target Hit!");
            m_stats.nonshiny++;
            return StarterRngOutcome::target_hit;
        }
        env.log("Missed target.");
    }

    if (m_failed_searches >= 5){
        env.log("Failed to find any matches 5 times in a row");
        return StarterRngOutcome::no_matches;
    }

    if (m_stats.resets > m_settings.max_resets()){
        env.log("Max resets reached.");
        return StarterRngOutcome::out_of_resets;
    }

    if (m_wildshiny_found){
        return StarterRngOutcome::wild_shiny;
    }

    return std::nullopt;
}

void StarterRngAttempts::reset_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    send_program_status_notification(
        env, m_notifications.status_update,
        "Calibrating."
    );
    env.update_stats();


    m_advances_radius = get_advances_radius(env.console, m_calibration_history, INITIAL_ADVANCES_RADIUS);

    const SeedMatch& seed_match = m_target.seed_match;
    if (m_calibration_history.results.size() > 0){
        m_calibrations = get_calibrations(env.console, m_calibration_history, seed_match.seed_values, seed_match.seed_position, m_target.advances);
    }

    // if previous resets had uncertain advances, slightly modify the seed delay to try to hit a different target
    apply_seed_bump(m_calibrations, m_uncertain_history);

    uint64_t ingame_advances = m_target.advances - CONTINUE_SCREEN_FRAMES;

    RngTimings timings = prepare_timings(
        env.console, PokemonFRLG_RngTarget::starters,
        seed_match.seed_delay, CONTINUE_SCREEN_FRAMES, ingame_advances,
        false, m_calibrations,
        FIXED_SEED_OFFSET
    );

    env.log("Resetting Game...");
    reset_and_perform_blind_sequence(
        env.console, context,
        m_settings.language, PokemonFRLG_RngTarget::starters,
        seed_match.seed_button, seed_match.extra_button, timings,
        m_launch_delay, false, m_settings.profile
    );
    m_stats.resets++;
    m_search_hits.clear();
    if (m_settings.on_reset){
        m_settings.on_reset(StarterRngProgress{.resets = m_stats.resets.load(), .calibrations = m_calibrations});
    }

    m_displays.filters.reset();
    m_displays.calibration.set_calibrations(m_calibrations);
    m_displays.calibration.reset_hits();
}

bool StarterRngAttempts::check_shiny_starter(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    bool shiny_found = check_for_shiny(env.console, context, PokemonFRLG_RngTarget::starters);

    if (shiny_found){
        env.log("Shiny found!");
        m_stats.shinies++;
        m_displays.calibration.hits.set("Shiny!");
        send_shiny_notification(env, context, "Shiny found!");
    }
    return shiny_found;
}

//  On the shiny's summary, as check_for_shiny() leaves it: reads it as
//  stage 1 of an attempt does and searches near the target.
StarterRngOutcome StarterRngAttempts::identify_shiny_starter(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    AdvObservedPokemon pokemon = read_summary(env.console, context, m_settings.language, starter_species());
    pokemon.level[0] = 5;
    AdvRngFilters filters = observation_to_filters(pokemon, m_base_stats);
    m_displays.filters.set(filters);
    const std::vector<AdvRngState> hits = search(env, filters);
    m_displays.calibration.set_hits(hits);
    const StarterRngOutcome outcome = identify_shiny(m_target.seed, m_target.advances, hits);
    env.log(std::format(
        "Shiny starter: {} states near the target match it; {}.", hits.size(),
        outcome == StarterRngOutcome::shiny_on_target ? "the target is one of them"
            : outcome == StarterRngOutcome::shiny_off_target ? "the target isn't" : "not known if it is the target"
    ));
    return outcome;
}

void StarterRngAttempts::send_shiny_notification(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context, const std::string& title
){
    VideoSnapshot screen = env.console.video().snapshot();
    send_program_notification(
        env,
        m_notifications.shiny,
        COLOR_YELLOW,
        title,
        {}, "",
        screen,
        true
    );
    if (m_settings.take_video){
        pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 0ms);
    }
}

std::vector<AdvRngState> StarterRngAttempts::search(SingleSwitchProgramEnvironment& env, AdvRngFilters& filters){
    m_search_hits = get_search_results(
        env.console, m_searcher, filters, m_target.seed_match.seed_values,
        m_target.advances, m_advances_radius, GENDER_THRESHOLD
    );
    return m_search_hits;
}

void StarterRngAttempts::record_error(SingleSwitchProgramEnvironment& env, const std::vector<AdvRngState>& search_hits){
    update_history(
        env.console, m_uncertain_history, m_calibration_history,
        MAX_HISTORY_LENGTH, m_calibrations, search_hits, 1, 2, true
    );
    m_stats.errors++;
}


void StarterRngAttempts::run_attempt(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // Stage 1: initial search -- starter received
    AdvObservedPokemon pokemon = read_summary(env.console, context, m_settings.language, starter_species());
    pokemon.level[0] = 5;
    AdvRngFilters filters = observation_to_filters(pokemon, m_base_stats);
    m_displays.filters.set(filters);

    std::vector<AdvRngState> search_hits = search(env, filters);
    m_displays.calibration.set_hits(search_hits);
    bool finished = update_history(
        env.console, m_uncertain_history, m_calibration_history,
        MAX_HISTORY_LENGTH, m_calibrations, search_hits, 1
    );
    if (finished){
        env.log("RNG search finished.");
        update_failed_searches(m_failed_searches, search_hits);
        return;
    }

    if (!run_rival_stage(env, context, pokemon, filters, search_hits)){
        return;
    }
    run_route1_stage(env, context, pokemon, filters, search_hits);
}

//  Returns false when the attempt ends here.
bool StarterRngAttempts::run_rival_stage(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    AdvObservedPokemon& pokemon, AdvRngFilters& filters, std::vector<AdvRngState>& search_hits
){
    // Stage 2: first search update -- post-rival-battle
    bool failed = walk_to_rival_battle(env, context);
    if (failed){
        record_error(env, search_hits);
        return false; // reset game
    }

    failed = auto_battle_rival(env, context, pokemon, filters);
    if (failed){
        record_error(env, search_hits);
        return false; // reset game
    }
    if (pokemon.level.size() > 1){
        search_hits = search(env, filters);
        m_displays.calibration.set_hits(search_hits);
        env.log("Number of search hits: " + std::to_string(search_hits.size()));
        bool finished = update_history(
            env.console, m_uncertain_history, m_calibration_history,
            MAX_HISTORY_LENGTH, m_calibrations, search_hits, 5
        );
        if (finished){
            env.log("RNG search finished.");
            update_failed_searches(m_failed_searches, search_hits);
            return false;
        }
    }
    return true;
}

void StarterRngAttempts::run_route1_stage(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    AdvObservedPokemon& pokemon, AdvRngFilters& filters, std::vector<AdvRngState>& search_hits
){
    // Stage 3: subsequent search updates -- leveling up from wild encounters
    bool failed = walk_to_route1_from_lab(env, context);
    if (failed){
        record_error(env, search_hits);
        return; // reset game
    }

    auto num_levels = pokemon.level.size();
    uint16_t MAX_LEVELS = 3;
    while(true){
        if (num_levels > MAX_LEVELS){
            env.log("RNG search not complete after 3 level-ups.");
            update_history(
                env.console, m_uncertain_history, m_calibration_history,
                MAX_HISTORY_LENGTH, m_calibrations, search_hits, 5, 2, true
            );
            return;
        }

        env.log("Level: " + std::to_string(4 + pokemon.level.size()));
        env.log("Speed EVs: " + std::to_string(pokemon.evs.back().speed));

        int ret2 = autolevel_on_route1(env, context, pokemon, filters);
        if (ret2 < 0){
            record_error(env, search_hits);
            return;
        }else if(ret2 == 1){
            if (stop_on_wild_shiny(env, context)){
                return;
            }
        }

        // force the search to finish after enough level-ups
        if (pokemon.level.size() > num_levels){
            finish_search(env, filters, search_hits);
            return;
        }
    }
}

//  Returns whether the attempts stop for the wild shiny.
bool StarterRngAttempts::stop_on_wild_shiny(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Wild shiny found!");
    m_stats.wildshinies++;
    send_shiny_notification(env, context, "Wild Shiny found!");
    if (!m_settings.ignore_wild_shinies){
        m_wildshiny_found = true;
        return true;
    }
    return false;
}

void StarterRngAttempts::finish_search(
    SingleSwitchProgramEnvironment& env, AdvRngFilters& filters, std::vector<AdvRngState>& search_hits
){
    search_hits = search(env, filters);
    m_displays.calibration.set_hits(search_hits);
    env.log("Number of search hits: " + std::to_string(search_hits.size()));
    update_history(
        env.console, m_uncertain_history, m_calibration_history,
        MAX_HISTORY_LENGTH, m_calibrations, search_hits, 5, 2, true
    );
    env.log("RNG search finished.");
    update_failed_searches(m_failed_searches, search_hits);
}


bool StarterRngAttempts::walk_to_rival_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // return to the overworld
    pbf_mash_button(context, BUTTON_B, 5000ms);
    int num_steps_to_the_left = steps_left_to_lab_door(m_target.starter);

    // dodge rival
    pbf_move_left_joystick(context, {0, -1}, 40ms, 460ms);
    pbf_move_left_joystick(context, {0, -1}, 100ms, 400ms);

    // line up with the doorway
    pbf_move_left_joystick(context, {-1, 0}, 40ms, 460ms); // pivot left
    for (int i=0; i<num_steps_to_the_left; i++){
        pbf_move_left_joystick(context, {-1, 0}, 100ms, 400ms);
    }

    // walk down and trigger battle
    BlackScreenWatcher black_screen(COLOR_RED);
    context.wait_for_all_requests();
    int ret =  run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            for (int i=0; i<5; i++){
                ssf_press_left_joystick(context, {0, -1}, 0ms, 20000ms, 0ms);
                ssf_mash1_button(context, BUTTON_B, 20000ms);
            }
        },
        { black_screen }
    );

    bool failed = ret < 0;
    if (failed){
        send_program_recoverable_error_notification(
            env, m_notifications.error_recoverable,
            "walk_to_rival_battle(): failed to initiate battle."
        );
    }
    return failed;
}

//  From the start of the rival battle: waits for the battle menu, uses the
//  first move and waits out Oak's tutorial. Returns true on failure.
bool StarterRngAttempts::start_rival_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // detect the battle menu
    BattleMenuWatcher battle_ready(COLOR_RED);
    context.wait_for_all_requests();
    int ret1 = run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 30s);
        },
        { battle_ready }
    );
    if (ret1 < 0){
        send_program_recoverable_error_notification(
            env, m_notifications.error_recoverable,
            "auto_battle_rival(): failed to detect the battle menu."
        );
        return true;
    }
    env.log("Battle started. Using first move...");

    // perform the first move and get through Oak's dialogue,
    // which messes up most detectors when it dims the screen
    pbf_mash_button(context, BUTTON_A, 1000ms); // execute first move
    context.wait_for_all_requests();
    int ret2 = run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 30s);
        },
        { battle_ready }
    );
    if (ret2 < 0){
        send_program_recoverable_error_notification(
            env, m_notifications.error_recoverable,
            "auto_battle_rival(): failed to detect the battle menu."
        );
        return true;
    }
    env.log("Oak tutorial dialogue finished. Mashing A...");
    return false;
}

bool StarterRngAttempts::auto_battle_rival(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AdvObservedPokemon& pokemon,
    AdvRngFilters& filters
){
    Pokemon::EVs evyield = rival_ev_yield(m_target.starter);

    if (start_rival_battle(env, context)){
        return true;
    }

    // mash A until somebody faints
    BattleOpponentFaintWatcher player_won(COLOR_RED);
    BattleFaintWatcher player_lost(COLOR_RED);
    context.wait_for_all_requests();
    int ret3 = run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_A, 300s);
        },
        { player_won, player_lost }
    );

    switch(ret3){
    case 0:
        env.log("Won battle against rival. Watching for level-up stats...");
        break;
    case 1:
        env.log("Lost battle against rival.");
        pbf_mash_button(context, BUTTON_B, 20s); // exit battle and dialogue
        return false;
    default:
        send_program_recoverable_error_notification(
            env, m_notifications.error_recoverable,
            "auto_battle_rival(): no fainting detected with 5 minutes."
        );
        return true;
    }

    return read_rival_level_up(env, context, pokemon, filters, evyield);
}

//  After winning the rival battle: reads the level-up stats and leaves the
//  battle. Returns true on failure.
bool StarterRngAttempts::read_rival_level_up(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AdvObservedPokemon& pokemon,
    AdvRngFilters& filters,
    const EVs& evyield
){
    // slowly advance dialog until level-up stats are visible
    BattleLevelUpWatcher level_up_stats(COLOR_RED, BattleLevelUpDialog::stats, m_settings.language);
    BlackScreenWatcher black_screen(COLOR_RED);
    context.wait_for_all_requests();
    int ret4 = run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            for(int i=0; i<60; i++){
                pbf_press_button(context, BUTTON_A, 200ms, 1800ms);
            }
        },
        { level_up_stats, black_screen }
    );

    switch(ret4){
    case 0:
        env.log("Level-up stats detected.");
        break;
    case 1:
        env.log("Battle exited without detecting level-up stats");
        return true; // will cause issues with keeping track of level and EVs
    default:
        env.log("auto_battle_rival(): no recognized state within 2 minutes of winning battle.");
        return true;
    }

    read_rival_level_up_stats(env, pokemon, filters, evyield);

    // exit battle
    pbf_mash_button(context, BUTTON_B, 20s);
    context.wait_for_all_requests();

    return false;
}

void StarterRngAttempts::read_rival_level_up_stats(
    SingleSwitchProgramEnvironment& env,
    AdvObservedPokemon& pokemon,
    AdvRngFilters& filters,
    const EVs& evyield
){
    // read stats
    BattleLevelUpReader reader(COLOR_RED);
    VideoOverlaySet overlays(env.console.overlay());
    reader.make_overlays(overlays);

    env.log("Reading stats...");
    VideoSnapshot screen = env.console.video().snapshot();
    StatReads stats = reader.read_stats(env.logger(), screen);

    update_filters(filters, pokemon, stats, evyield, m_base_stats);
    m_displays.filters.set(filters);
}



bool StarterRngAttempts::walk_to_route1_from_lab(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // exit the lab
    env.log("Exiting the lab...");
    BlackScreenWatcher black_screen(COLOR_RED);
    context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            pbf_move_left_joystick(context, {0, -1}, 10s, 0ms);
        },
        { black_screen }
    );

    if (ret < 0){
        send_program_recoverable_error_notification(
            env, m_notifications.error_recoverable,
            "walk_to_route1_from_lab(): failed to exit lab."
        );
        return true;
    }

    env.log("Lab exited. Walking to Route 1...");
    pbf_wait(context, 5000ms);
    pbf_move_left_joystick(context, {-1, 0}, 1280ms, 300ms);
    pbf_move_left_joystick(context, {0, +1}, 3150ms, 300ms);
    pbf_move_left_joystick(context, {+1, 0}, 330ms, 300ms);
    pbf_move_left_joystick(context, {0, +1}, 720ms, 300ms);
    context.wait_for_all_requests();
    return false;
}

bool StarterRngAttempts::walk_to_route1_from_home(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    // exit the house
    env.log("Exiting the house...");
    BlackScreenWatcher black_screen(COLOR_RED);
    context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            pbf_move_left_joystick(context, {0, -1}, 1000ms, 300ms);
            pbf_move_left_joystick(context, {-1, 0}, 900ms, 300ms);
            pbf_move_left_joystick(context, {0, -1}, 1000ms, 300ms);
        },
        { black_screen }
    );

    if (ret < 0){
        send_program_recoverable_error_notification(
            env, m_notifications.error_recoverable,
            "walk_to_route1_from_home(): failed to exit the house."
        );
        return true;
    }

    env.log("House exited. Walking to Route 1...");
    pbf_wait(context, 5s);
    pbf_move_left_joystick(context, {+1, 0}, 1370ms, 300ms);
    pbf_move_left_joystick(context, {0, +1}, 1450ms, 300ms);
    pbf_move_left_joystick(context, {+1, 0}, 300ms,  300ms);
    pbf_move_left_joystick(context, {0, +1}, 1250ms, 300ms);

    return false;
}

int StarterRngAttempts::autolevel_on_route1(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AdvObservedPokemon& pokemon,
    AdvRngFilters& filters
){
    Pokemon::EVs evyield = {0, 0, 0, 0, 0, 0};

    env.log("Arrived at Route 1.");
    bool leftright = true;
    context.wait_for_all_requests();

    while (true){
        int ret = trigger_route1_encounter(env, context, leftright);
        if (ret < 0){
            return -1;
        }

        if (ret == 1){ // shiny
            return 1;
        }

        // auto battle
        BattleResult ret2 = spam_first_move(env.console, context);

        switch (ret2){
        case BattleResult::opponentfainted:
            env.log("Opponent fainted.");
            evyield.speed++; // always rattata or pidgey
            leftright = !leftright;
            if (read_wild_level_up(env, context, pokemon, filters, evyield)){
                return 0;
            }
            continue;
        case BattleResult::playerfainted:
            if (return_to_route1_after_faint(env, context)){
                return -1;
            }
            leftright = true;
            continue;
        case BattleResult::outofpp:
        case BattleResult::unknown:
        default:
            return -1;
        }
    }
}

//  Spins in the grass until a wild encounter. Returns grass_spin()'s result.
int StarterRngAttempts::trigger_route1_encounter(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool leftright
){
    // trigger encounter
    env.log("Triggering wild encounters...");
    int ret = grass_spin(env.console, context, leftright);
    if (ret < 0){
        send_program_recoverable_error_notification(
            env, m_notifications.error_recoverable,
            "autolevel_on_route1(): failed to trigger encounter."
        );
    }
    return ret;
}

//  After the player's Pokemon fainted: walks from home back to Route 1.
//  Returns true on failure.
bool StarterRngAttempts::return_to_route1_after_faint(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Pokemon fainted. Mashing B through dialogues...");
    pbf_mash_button(context, BUTTON_B, 30s); // skip through a few transitions and lots of dialogue
    if (walk_to_route1_from_home(env, context)){
        return true;
    }
    context.wait_for_all_requests();
    return false;
}

//  After a wild Pokemon fainted: reads the level-up stats if they show and
//  leaves the battle. Returns whether the stats were read.
bool StarterRngAttempts::read_wild_level_up(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AdvObservedPokemon& pokemon,
    AdvRngFilters& filters,
    const EVs& evyield
){
    BattleLevelUpWatcher level_up(COLOR_RED, BattleLevelUpDialog::stats, m_settings.language);
    BlackScreenWatcher black_screen(COLOR_RED);
    BattleLevelUpReader reader;

    context.wait_for_all_requests();
    int ret3 = run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            for (int i=0; i<5; i++){
                pbf_press_button(context, BUTTON_B, 200ms, 1800ms);
            }
        },
        { level_up, black_screen }
    );

    switch (ret3){
    case 0:{
        env.log("Level-up stats detected. Reading stats...");
        VideoSnapshot screen = env.console.video().snapshot();
        Pokemon::StatReads stats = reader.read_stats(env.logger(), screen);
        update_filters(filters, pokemon, stats, evyield, m_base_stats);
        m_displays.filters.set(filters);
        exit_wild_battle(env.console, context, false, true);
        return true;
    }
    case -1:
        exit_wild_battle(env.console, context, false, true);
        [[fallthrough]];
    default:
        pbf_wait(context, 1000ms);
        context.wait_for_all_requests();
        return false;
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
