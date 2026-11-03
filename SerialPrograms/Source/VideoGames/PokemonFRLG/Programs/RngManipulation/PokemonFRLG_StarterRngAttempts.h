/*  Starter RNG Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Starter RNG's reset, calibrate and attempt loop, for one target. The
 *  Starter RNG program runs it with its options, stats and displays; another
 *  program (AutoStory) can run it with its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_StarterRngAttempts_H
#define PokemonAutomation_PokemonFRLG_StarterRngAttempts_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>
#include "CommonFramework/Language.h"
#include "CommonFramework/Notifications/EventNotificationOption.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Pokemon_AdvRng.h"
#include "Pokemon/Pokemon_StatsCalculation.h"
#include "PokemonFRLG_RngCalibration.h"
#include "PokemonFRLG_RngDisplays.h"
#include "PokemonFRLG_SeedsDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


enum class StarterSpecies{
    bulbasaur,
    squirtle,
    charmander
};

//  The species' base stats.
BaseStats starter_base_stats(StarterSpecies starter);
//  The EVs the rival's starter gives when the player's "starter" beats it.
EVs rival_ev_yield(StarterSpecies starter);
//  After taking "starter" from its ball and dodging the rival, the steps
//  left that line the player up with the lab's door.
int steps_left_to_lab_door(StarterSpecies starter);


struct StarterRngStats : public StatsTracker{
    StarterRngStats();

    std::atomic<uint64_t>& resets;
    std::atomic<uint64_t>& shinies;
    std::atomic<uint64_t>& nonshiny;
    std::atomic<uint64_t>& wildshinies;
    std::atomic<uint64_t>& errors;
};


struct StarterRngTarget{
    StarterSpecies starter = StarterSpecies::bulbasaur;
    uint16_t seed = 0;
    uint64_t advances = 0;
    //  The seed's entry in the seeds database (found).
    SeedMatch seed_match;
};

//  Where an attempt is after a reset: the resets so far, and the
//  calibration that reset was timed with.
struct StarterRngProgress{
    uint64_t resets = 0;
    RngCalibrations calibrations{};
};

struct StarterRngSettings{
    Language language = Language::None;
    //  The Switch profile's position on the user selection screen; 0 for
    //  the default.
    uint8_t profile = 0;
    bool ignore_wild_shinies = false;
    //  Record a video when a shiny is found.
    bool take_video = false;
    //  Read before each reset, since Starter RNG lets the user change it
    //  while running.
    std::function<uint64_t()> max_resets;
    //  Called after each reset, if set, for a caller that logs the attempts.
    std::function<void(const StarterRngProgress&)> on_reset;
};

//  Where the attempts show the target, the filters read so far and the
//  calibration. The calibration display also holds the starting
//  calibration.
struct StarterRngDisplays{
    RngTargetDisplay& target;
    RngFilterDisplay& filters;
    RngCalibrationDisplay& calibration;
};

struct StarterRngNotifications{
    EventNotificationOption& shiny;
    EventNotificationOption& status_update;
    EventNotificationOption& error_recoverable;
};


enum class StarterRngOutcome{
    shiny,          //  The starter was shiny after a reset.
    target_hit,     //  The calibration shows the target was hit; not shiny.
    wild_shiny,     //  A wild shiny on Route 1, with ignore_wild_shinies off.
    out_of_resets,  //  More resets than max_resets.
    no_matches,     //  5 searches in a row matched no state near the target.
};

struct StarterRngResult{
    StarterRngOutcome outcome = StarterRngOutcome::out_of_resets;
    //  The target's PID, nature and IVs, on target_hit.
    std::optional<AdvPokemonResult> pokemon;
};


class StarterRngAttempts{
public:
    StarterRngAttempts(
        StarterRngTarget target,
        StarterRngSettings settings,
        const StarterRngDisplays& displays,
        const StarterRngNotifications& notifications,
        StarterRngStats& stats
    );
    StarterRngAttempts(const StarterRngAttempts&) = delete;
    StarterRngAttempts& operator=(const StarterRngAttempts&) = delete;

    //  From the HOME menu with the game closed: resets the game and takes
    //  the starter at the target's timing until the outcome is known. Each
    //  non-shiny starter is read, battled against the rival and leveled on
    //  Route 1 to find the state hit and recalibrate. Ends in the game on
    //  the last screen the attempts reached. Call once per object.
    StarterRngResult run(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

private:
    std::optional<StarterRngOutcome> finished(SingleSwitchProgramEnvironment& env);
    void reset_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    bool check_shiny_starter(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    void send_shiny_notification(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const std::string& title);
    std::vector<AdvRngState> search(SingleSwitchProgramEnvironment& env, AdvRngFilters& filters);
    void record_error(SingleSwitchProgramEnvironment& env, const std::vector<AdvRngState>& search_hits);

    void run_attempt(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    bool run_rival_stage(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        AdvObservedPokemon& pokemon, AdvRngFilters& filters, std::vector<AdvRngState>& search_hits
    );
    void run_route1_stage(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        AdvObservedPokemon& pokemon, AdvRngFilters& filters, std::vector<AdvRngState>& search_hits
    );
    bool stop_on_wild_shiny(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    void finish_search(
        SingleSwitchProgramEnvironment& env, AdvRngFilters& filters, std::vector<AdvRngState>& search_hits
    );

    bool walk_to_rival_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    bool start_rival_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    bool auto_battle_rival(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AdvObservedPokemon& pokemon,
        AdvRngFilters& filters
    );
    bool read_rival_level_up(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AdvObservedPokemon& pokemon,
        AdvRngFilters& filters,
        const EVs& evyield
    );
    void read_rival_level_up_stats(
        SingleSwitchProgramEnvironment& env,
        AdvObservedPokemon& pokemon,
        AdvRngFilters& filters,
        const EVs& evyield
    );

    bool walk_to_route1_from_lab(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    bool walk_to_route1_from_home(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    int trigger_route1_encounter(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool leftright
    );
    bool return_to_route1_after_faint(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    int autolevel_on_route1(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AdvObservedPokemon& pokemon,
        AdvRngFilters& filters
    );
    bool read_wild_level_up(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AdvObservedPokemon& pokemon,
        AdvRngFilters& filters,
        const EVs& evyield
    );

    const StarterRngTarget m_target;
    const StarterRngSettings m_settings;
    StarterRngDisplays m_displays;
    StarterRngNotifications m_notifications;
    StarterRngStats& m_stats;
    const BaseStats m_base_stats;

    AdvRngSearcher m_searcher;
    RngCalibrations m_calibrations{};
    Milliseconds m_launch_delay;
    RngUncertainHistory m_uncertain_history;
    RngCalibrationHistory m_calibration_history;
    uint64_t m_advances_radius = 0;
    bool m_wildshiny_found = false;
    uint16_t m_failed_searches = 0;
};



}
}
}
#endif
