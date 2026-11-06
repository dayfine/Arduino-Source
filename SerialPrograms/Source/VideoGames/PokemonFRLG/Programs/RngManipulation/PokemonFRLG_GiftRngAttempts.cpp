/*  Gift RNG Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <set>
#include <string>
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonFRLG_RngNavigation.h"
#include "PokemonFRLG_HardReset.h"
#include "PokemonFRLG_RngLoopRoutines.h"
#include "PokemonFRLG_GiftRngAttempts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const int64_t FIXED_SEED_OFFSET = -845; // milliseconds, approximate

const uint64_t CONTINUE_SCREEN_FRAMES = 200;

const std::set<std::string>& gift_species_names(){
    static const std::set<std::string> SPECIES_LIST = {
        "magikarp", "hitmonchan", "hitmonlee", "eevee", "lapras",
        "omanyte", "kabuto", "aerodactyl",
        "abra", "clefairy", "dratini", "scyther", "pinsir", "porygon",
        "togepi"
    };
    return SPECIES_LIST;
}

} // namespace


GiftRngSpecies gift_rng_species(PokemonFRLG_RngTarget target){
    switch (target){
    case PokemonFRLG_RngTarget::magikarp:
        return {.base_stats = { 20, 10, 55, 15, 20, 80 }, .gender_threshold = 126, .level = 5};
    case PokemonFRLG_RngTarget::hitmonchan:
        return {.base_stats = { 50, 105, 79, 35, 110, 76 }, .gender_threshold = -1, .level = 25};
    case PokemonFRLG_RngTarget::hitmonlee:
        return {.base_stats = { 50, 120, 53, 35, 110, 87 }, .gender_threshold = -1, .level = 25};
    case PokemonFRLG_RngTarget::eevee:
        return {.base_stats = { 55, 55, 50, 45, 65, 55 }, .gender_threshold = 30, .level = 25};
    case PokemonFRLG_RngTarget::lapras:
        return {.base_stats = { 130, 85, 80, 85, 95, 60 }, .gender_threshold = 126, .level = 25};
    case PokemonFRLG_RngTarget::omanyte:
        return {.base_stats = { 35, 40, 100, 90, 55, 35 }, .gender_threshold = 30, .level = 5};
    case PokemonFRLG_RngTarget::kabuto:
        return {.base_stats = { 30, 80, 90, 55, 45, 55 }, .gender_threshold = 30, .level = 5};
    case PokemonFRLG_RngTarget::aerodactyl:
        return {.base_stats = { 80, 105, 65, 60, 75, 130 }, .gender_threshold = 30, .level = 5};
    case PokemonFRLG_RngTarget::gamecornerabra:
        return {.base_stats = { 25, 20, 15, 105, 55, 90 }, .gender_threshold = 63, .level = 0};
    case PokemonFRLG_RngTarget::gamecornerclefairy:
        return {.base_stats = { 70, 45, 48, 60, 65, 35 }, .gender_threshold = 190, .level = 0};
    case PokemonFRLG_RngTarget::gamecornerdratinifr:
    case PokemonFRLG_RngTarget::gamecornerdratinilg:
        return {.base_stats = { 41, 64, 45, 50, 50, 50 }, .gender_threshold = 126, .level = 0};
    case PokemonFRLG_RngTarget::gamecornerscyther:
        return {.base_stats = { 70, 110, 80, 55, 80, 105 }, .gender_threshold = 126, .level = 25};
    case PokemonFRLG_RngTarget::gamecornerpinsir:
        return {.base_stats = { 65, 125, 100, 55, 70, 85 }, .gender_threshold = 126, .level = 18};
    case PokemonFRLG_RngTarget::gamecornerporygon:
        return {.base_stats = { 65, 60, 70, 85, 75, 40 }, .gender_threshold = -1, .level = 0};
    case PokemonFRLG_RngTarget::togepi:
    case PokemonFRLG_RngTarget::togepifast:
        return {.base_stats = { 35, 20, 65, 40, 65, 20 }, .gender_threshold = 30, .level = 5};
    default:
        return {};
    }
}


GiftRngOutcome gift_shiny_outcome(uint16_t seed, uint64_t advances, const std::vector<AdvRngState>& hits){
    if (hits.empty()){
        return GiftRngOutcome::shiny;
    }
    const bool on_target = std::any_of(hits.begin(), hits.end(), [&](const AdvRngState& hit){
        return have_hit_target(seed, advances, hit);
    });
    return on_target ? GiftRngOutcome::shiny_on_target : GiftRngOutcome::shiny_off_target;
}


GiftRngStats::GiftRngStats()
    : resets(m_stats["Resets"])
    , shinies(m_stats["Shinies"])
    , nonshiny(m_stats["Non-Shiny Hits"])
    , errors(m_stats["Errors"])
{
    m_display_order.emplace_back("Resets");
    m_display_order.emplace_back("Shinies");
    m_display_order.emplace_back("Non-Shiny Hits", HIDDEN_IF_ZERO);
    m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
}


GiftRngAttempts::GiftRngAttempts(
    GiftRngTarget target,
    GiftRngSettings settings,
    const GiftRngDisplays& displays,
    const GiftRngNotifications& notifications,
    GiftRngStats& stats
)
    : m_target(std::move(target))
    , m_settings(std::move(settings))
    , m_displays(displays)
    , m_notifications(notifications)
    , m_stats(stats)
    , m_species(gift_rng_species(m_target.target))
    , m_initial_advances_radius(m_settings.use_teachy_tv ? 4096 : 1024)
    , m_max_history_length(m_settings.use_teachy_tv ? 2 : 10)
    , m_searcher(m_target.seed, m_target.advances, AdvRngMethod::Method1)
    , m_launch_delay(INITIAL_LAUNCH_DELAY)
{}


GiftRngResult GiftRngAttempts::run(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("RNG Target: " + std::to_string(static_cast<size_t>(m_target.target)));
    env.log("Target Seed: " + to_hex_string(m_target.seed));
    env.log("Target Advances: " + std::to_string(m_target.advances));

    AdvPokemonResult target_result = m_searcher.generate_pokemon();
    m_displays.target.set_target(target_result, m_species.gender_threshold);
    log_target_pokemon(env.console, target_result);

    m_calibrations = {
        m_displays.calibration.seed_calibration / FRLG_FRAME_DURATION,
        m_displays.calibration.csf_calibration,
        m_displays.calibration.advances_calibration
    };
    log_calibrations(env.console, m_calibrations, true);

    while (true){
        std::optional<GiftRngOutcome> outcome = finished(env);
        if (outcome){
            GiftRngResult result{.outcome = *outcome, .pokemon = std::nullopt};
            if (*outcome == GiftRngOutcome::target_hit){
                result.pokemon = target_result;
            }
            return result;
        }

        reset_game(env, context);

        if (check_shiny_gift(env, context)){
            const GiftRngOutcome shiny = m_settings.identify_shiny ? identify_shiny_gift(env, context) : GiftRngOutcome::shiny;
            const bool on_target = shiny == GiftRngOutcome::shiny_on_target;
            report_read(true);
            return GiftRngResult{.outcome = shiny, .pokemon = on_target ? std::optional(target_result) : std::nullopt};
        }

        run_attempt(env, context);
        report_read(false);
    }
}

void GiftRngAttempts::report_read(bool shiny){
    if (m_settings.on_read){
        m_settings.on_read(RngResetRead{
            .resets = m_stats.resets.load(),
            .hits = m_search_hits,
            .hit = shiny ? std::nullopt : reset_hit(m_uncertain_history, m_calibration_history, m_search_hits),
            .shiny = shiny,
        });
    }
}

std::optional<GiftRngOutcome> GiftRngAttempts::finished(SingleSwitchProgramEnvironment& env){
    if (m_calibration_history.results.size() > 0){
        env.log("Checking for nonshiny target hit...");
        if (have_hit_target(m_target.seed, m_target.advances, m_calibration_history.results.back())){
            env.log("Target Hit!");
            m_stats.nonshiny++;
            return GiftRngOutcome::target_hit;
        }
        env.log("Missed target.");
    }

    if (m_failed_searches >= 5){
        env.log("Failed to find any matches 5 times in a row");
        return GiftRngOutcome::no_matches;
    }

    if (m_stats.resets > m_settings.max_resets()){
        env.log("Max resets reached.");
        return GiftRngOutcome::out_of_resets;
    }

    return std::nullopt;
}

void GiftRngAttempts::reset_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    send_program_status_notification(
        env, m_notifications.status_update,
        "Calibrating."
    );
    env.update_stats();


    m_advances_radius = get_advances_radius(env.console, m_calibration_history, m_initial_advances_radius);

    const SeedMatch& seed_match = m_target.seed_match;
    if (m_calibration_history.results.size() > 0){
        m_calibrations = get_calibrations(env.console, m_calibration_history, seed_match.seed_values, seed_match.seed_position, m_target.advances);
    }

    // if previous resets had uncertain advances, slightly modify the seed delay to try to hit a different target
    apply_seed_bump(m_calibrations, m_uncertain_history);

    uint64_t ingame_advances = m_target.advances - CONTINUE_SCREEN_FRAMES;

    RngTimings timings = prepare_timings(
        env.console, m_target.target,
        seed_match.seed_delay, CONTINUE_SCREEN_FRAMES, ingame_advances,
        m_settings.use_teachy_tv, m_calibrations,
        FIXED_SEED_OFFSET, m_settings.language
    );

    env.log("Resetting Game...");
    reset_and_perform_blind_sequence(
        env.console, context, m_settings.language, m_target.target,
        seed_match.seed_button, seed_match.extra_button, timings,
        m_launch_delay, false, m_settings.profile
    );
    m_stats.resets++;
    m_search_hits.clear();
    if (m_settings.on_reset){
        m_settings.on_reset(GiftRngProgress{.resets = m_stats.resets.load(), .calibrations = m_calibrations});
    }

    m_displays.filters.reset();
    m_displays.calibration.set_calibrations(m_calibrations);
    m_displays.calibration.reset_hits();
}

bool GiftRngAttempts::check_shiny_gift(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    bool shiny_found = check_for_shiny(env.console, context, m_target.target);

    if (shiny_found){
        env.log("Shiny found!");
        m_stats.shinies++;
        m_displays.calibration.hits.set("Shiny!");
        send_program_notification(
            env,
            m_notifications.shiny,
            COLOR_YELLOW,
            "Shiny found!",
            {}, "",
            env.console.video().snapshot(),
            true
        );
        if (m_settings.take_video){
            pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 0ms);
        }
    }
    return shiny_found;
}

//  On the shiny's summary, as check_for_shiny() leaves it: reads it as an
//  attempt does and searches near the target, without Rare Candies.
GiftRngOutcome GiftRngAttempts::identify_shiny_gift(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    AdvObservedPokemon pokemon = read_gift(env, context);
    AdvRngFilters filters = observation_to_filters(pokemon, m_species.base_stats);
    m_displays.filters.set(filters);
    const std::vector<AdvRngState> hits = search(env, filters);
    m_displays.calibration.set_hits(hits);
    const GiftRngOutcome outcome = gift_shiny_outcome(m_target.seed, m_target.advances, hits);
    env.log(std::format(
        "Shiny gift: {} states near the target match it; {}.", hits.size(),
        outcome == GiftRngOutcome::shiny_on_target ? "the target is one of them"
            : outcome == GiftRngOutcome::shiny_off_target ? "the target isn't" : "not known if it is the target"
    ));
    return outcome;
}

AdvObservedPokemon GiftRngAttempts::read_gift(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    AdvObservedPokemon pokemon = read_summary(env.console, context, m_settings.language, gift_species_names());
    if (m_species.level > 0){
        pokemon.level[0] = m_species.level;
    }
    return pokemon;
}

std::vector<AdvRngState> GiftRngAttempts::search(SingleSwitchProgramEnvironment& env, AdvRngFilters& filters){
    m_search_hits = get_search_results(
        env.console, m_searcher, filters, m_target.seed_match.seed_values,
        m_target.advances, m_advances_radius, m_species.gender_threshold
    );
    return m_search_hits;
}


void GiftRngAttempts::run_attempt(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    AdvObservedPokemon pokemon = read_gift(env, context);
    AdvRngFilters filters = observation_to_filters(pokemon, m_species.base_stats);
    m_displays.filters.set(filters);

    std::vector<AdvRngState> search_hits = refine_calibration_with_rare_candy(
        env, context, m_settings.language, pokemon, filters, m_species.base_stats,
        m_uncertain_history, m_calibration_history, m_calibrations,
        m_max_history_length, m_settings.max_rare_candies(), AdvRngMethod::Method1, false,
        m_stats.errors, m_notifications.error_recoverable,
        [&](AdvRngFilters& f){
            return search(env, f);
        },
        [&](const std::vector<AdvRngState>& h){ m_displays.calibration.set_hits(h); },
        [&](const AdvRngFilters& f){ m_displays.filters.set(f); },
        [&](const std::vector<AdvRngState>& h){
            return all_equal(h) || all_indistinguishable(h, m_searcher, m_species.gender_threshold);
        }
    );

    m_search_hits = search_hits;
    env.log("RNG search finished.");
    update_failed_searches(m_failed_searches, search_hits);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
