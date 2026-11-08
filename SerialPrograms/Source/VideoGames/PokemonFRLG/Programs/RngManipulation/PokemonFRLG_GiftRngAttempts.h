/*  Gift RNG Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Gift RNG's reset, calibrate and attempt loop, for one target. The Gift
 *  RNG program runs it with its options, stats and displays; another
 *  program (AutoStory) can run it with its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_GiftRngAttempts_H
#define PokemonAutomation_PokemonFRLG_GiftRngAttempts_H

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
#include "PokemonFRLG_BlindNavigation.h"
#include "PokemonFRLG_RngCalibration.h"
#include "PokemonFRLG_RngDisplays.h"
#include "PokemonFRLG_SeedsDatabase.h"
#include "PokemonFRLG_StarterRngAttempts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What the search needs to know about a gift: its base stats, its gender
//  threshold (-1 if genderless) and the level it is received at (0 to keep
//  the level read from its summary).
struct GiftRngSpecies{
    BaseStats base_stats{};
    int16_t gender_threshold = -1;
    uint8_t level = 0;
};

//  The gift that "target" gives. Targets that aren't gifts get the defaults.
GiftRngSpecies gift_rng_species(PokemonFRLG_RngTarget target);


//  Statistics tracker for Gift RNG attempts.
struct GiftRngStats : public StatsTracker{
    GiftRngStats();

    std::atomic<uint64_t>& resets;
    std::atomic<uint64_t>& shinies;
    std::atomic<uint64_t>& nonshiny;
    std::atomic<uint64_t>& errors;
};


//  Target seed and advances for Gift RNG manipulation.
struct GiftRngTarget{
    PokemonFRLG_RngTarget target = PokemonFRLG_RngTarget::magikarp;
    uint16_t seed = 0;
    uint64_t advances = 0;
    //  The seed's entry in the seeds database (found).
    SeedMatch seed_match;
};

//  Where an attempt is after a reset: the resets so far, and the
//  calibration that reset was timed with.
struct GiftRngProgress{
    uint64_t resets = 0;
    RngCalibrations calibrations{};
};

//  Settings controlling Gift RNG reset loops.
struct GiftRngSettings{
    Language language = Language::None;
    //  The Switch profile's position on the user selection screen; 0 for
    //  the default.
    uint8_t profile = 0;
    //  Advance the RNG in the Teachy TV.
    bool use_teachy_tv = false;
    //  Record a video when a shiny is found.
    bool take_video = false;
    //  After a shiny, read its summary and search for the state hit, to
    //  tell whether it was the target (shiny_on_target or
    //  shiny_off_target). Off, the attempts end on the shiny at once.
    bool identify_shiny = false;
    //  Read before each reset, since Gift RNG lets the user change it
    //  while running.
    std::function<uint64_t()> max_resets;
    //  The Rare Candies at the top of the bag. Read before each search, as
    //  max_resets is.
    std::function<uint64_t()> max_rare_candies;
    //  Called after each reset, if set, for a caller that logs the attempts.
    std::function<void(const GiftRngProgress&)> on_reset;
    //  Called after each reset's reads, if set, as on_reset is.
    std::function<void(const RngResetRead&)> on_read;
};

//  Where the attempts show the target, the filters read so far and the
//  calibration. The calibration display also holds the starting
//  calibration.
struct GiftRngDisplays{
    RngTargetDisplay& target;
    RngFilterDisplay& filters;
    RngCalibrationDisplay& calibration;
};

//  Notification options for Gift RNG events.
struct GiftRngNotifications{
    EventNotificationOption& shiny;
    EventNotificationOption& status_update;
    EventNotificationOption& error_recoverable;
};


//  The outcome of an individual Gift RNG reset attempt.
enum class GiftRngOutcome{
    shiny,              //  The gift was shiny after a reset; not known if on the target.
    shiny_on_target,    //  Shiny, and the target is among the states its stats match.
    shiny_off_target,   //  Shiny, and the states its stats match aren't the target.
    target_hit,         //  The calibration shows the target was hit; not shiny.
    out_of_resets,      //  More resets than max_resets.
    no_matches,         //  5 searches in a row matched no state near the target.
};

//  A shiny gift's outcome from the states its stats match near the target
//  ("hits"): on the target if it is among them, off it if not, and just
//  shiny if none match.
GiftRngOutcome gift_shiny_outcome(uint16_t seed, uint64_t advances, const std::vector<AdvRngState>& hits);

//  The result of a Gift RNG attempt sequence.
struct GiftRngResult{
    GiftRngOutcome outcome = GiftRngOutcome::out_of_resets;
    //  The target's PID, nature and IVs, on target_hit and shiny_on_target.
    std::optional<AdvPokemonResult> pokemon;
};


//  Automates Gift RNG attempts until a target outcome is reached.
class GiftRngAttempts{
public:
    GiftRngAttempts(
        GiftRngTarget target,
        GiftRngSettings settings,
        const GiftRngDisplays& displays,
        const GiftRngNotifications& notifications,
        GiftRngStats& stats
    );
    GiftRngAttempts(const GiftRngAttempts&) = delete;
    GiftRngAttempts& operator=(const GiftRngAttempts&) = delete;

    //  From the HOME menu with the game closed: resets the game and
    //  receives the gift at the target's timing until the outcome is known.
    //  Each non-shiny gift is read, leveled with Rare Candies if needed, to
    //  find the state hit and recalibrate. Ends in the game on the last
    //  screen the attempts reached. Call once per object.
    GiftRngResult run(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

private:
    std::optional<GiftRngOutcome> finished(SingleSwitchProgramEnvironment& env);
    void reset_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    bool check_shiny_gift(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    GiftRngOutcome identify_shiny_gift(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    AdvObservedPokemon read_gift(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    std::vector<AdvRngState> search(SingleSwitchProgramEnvironment& env, AdvRngFilters& filters);
    void run_attempt(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    void report_read(bool shiny);

    const GiftRngTarget m_target;
    const GiftRngSettings m_settings;
    GiftRngDisplays m_displays;
    GiftRngNotifications m_notifications;
    GiftRngStats& m_stats;
    const GiftRngSpecies m_species;
    const uint64_t m_initial_advances_radius;
    const uint8_t m_max_history_length;

    AdvRngSearcher m_searcher;
    RngCalibrations m_calibrations{};
    Milliseconds m_launch_delay;
    RngUncertainHistory m_uncertain_history;
    RngCalibrationHistory m_calibration_history;
    uint64_t m_advances_radius = 0;
    uint16_t m_failed_searches = 0;
    //  The last search's hits, for on_read.
    std::vector<AdvRngState> m_search_hits;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
