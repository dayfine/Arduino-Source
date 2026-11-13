/*  BDSP Starter RNG Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_StarterRngAttempts_H
#define PokemonAutomation_PokemonBDSP_StarterRngAttempts_H

#include <stdint.h>
#include <functional>
#include <string>
#include "CommonFramework/Language.h"
#include "CommonFramework/Notifications/EventNotificationOption.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Pokemon_BdspRng.h"
#include "Pokemon/Pokemon_Xorshift128.h"
#include "PokemonBDSP/Options/PokemonBDSP_RngFilter.h"
#include "PokemonBDSP_RngAim.h"
#include "PokemonBDSP_RngDisplays.h"
#include "PokemonBDSP_StarterNavigation.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


enum class BdspStartPoint{
    Lakefront,
    Bedroom,
};


struct BdspSkipResult{
    bool success = false;
    Pokemon::Xorshift128State state;
    uint64_t target_advance = 0;
    uint64_t buffer = 0;
    std::string failure_reason;
};


enum class BdspAttemptOutcome{
    Abandoned,
    Missed,
    Hit,
    Unverifiable,
};


struct StarterRngStats : public StatsTracker{
    StarterRngStats();

    std::atomic<uint64_t>& resets;
    std::atomic<uint64_t>& missed;
    std::atomic<uint64_t>& hits;
    std::atomic<uint64_t>& shinies;
    std::atomic<uint64_t>& errors;
};


//  Evaluates whether a generated starter satisfies the filter snapshot.
//  Takes a snapshot rather than the table because scanning millions of
//  advances cannot afford cloning the table on every query.
bool starter_wanted(
    const BdspRngFilterSnapshot& filters,
    const Pokemon::BdspPokemonResult& pokemon
);


//  Settings for a Starter RNG attempt loop.
//
//  In-game state traversal after the starter is chosen and the wild Starly
//  battle begins at Lake Verity:
//  - Shiny hit (`Outcome::SHINY`):
//    `detect_shiny_battle(...)` finishes at the start of the Starly battle.
//    * When `finish_battle_on_shiny == false` (standalone `StarterRng` default):
//      returns immediately while still inside the Starly battle at Lake Verity.
//    * When `finish_battle_on_shiny == true` (for AutoStory):
//      calls `clear_starter_battle(env, context)`, which mashes A until
//      `EndBattleWatcher` detects the black-screen battle exit transition, and
//      when `close_summary_after_read == true` also advances through Rowan's
//      post-battle scene (`open_menu`) to `VerityLakefront (17, 14..15)` and
//      closes the X menu so both `SHINY` and `HIT` end on the `VerityLakefront`
//      overworld ready for checkpoint `00.03`.
//  - Non-shiny starter (`clear_starter_battle` + `read_observed_pokemon`):
//    1. `clear_starter_battle(env, context)` mashes A until `EndBattleWatcher`
//       detects the end of the Starly battle.
//    2. `read_observed_pokemon(...)` calls `open_menu(env, context)`, which
//       mashes `B, B, X` until `MenuWatcher` detects the overworld X menu.
//       Because the X menu cannot open during the post-battle Lake Verity
//       cutscene or the warp out of the lake, `open_menu` advances through
//       all of Rowan's and Barry's post-battle dialogue (`after_starly`) and
//       the warp back to `VerityLakefront (17, 14..15)`, then opens the X menu
//       once the player has overworld control on `VerityLakefront`.
//    3. `open_starter_summary(env, context)` selects `POKEMON` (one entry left
//       on the 2-icon pre-Pokedex X menu), opens the starter's Summary screen,
//       reads Nature & Gender on the 2nd tab (Trainer Memo), and presses Right
//       to read the 6 stats on the 3rd tab (Skills/Stats).
//    * When `close_summary_after_read == false` (standalone `StarterRng` default):
//      leaves the game on the 3rd tab of the starter's Summary screen.
//    * When `close_summary_after_read == true` (for AutoStory):
//      calls `close_starter_summary(env, context)` after reading the summary,
//      which presses B back to the X menu and X to close it, leaving the
//      player standing on the `VerityLakefront` overworld at `(17, 14..15)`
//      (with `after_starly` already completed, ready for checkpoint `00.03`).
struct StarterRngSettings{
    Language language = Language::English;
    uint8_t player_model = 1;
    BdspStartPoint start_point = BdspStartPoint::Lakefront;
    BdspStarter starter = BdspStarter::Turtwig;

    //  Predicate for target selection and hit verification. If `wanted` is
    //  empty and `filters` is non-null, `starter_wanted(*filters, ...)` is used.
    std::function<bool(const Pokemon::BdspPokemonResult&)> wanted;
    const BdspRngFilterSnapshot* filters = nullptr;

    uint16_t max_resets = 0;
    uint16_t max_target_wait_minutes = 30;
    uint16_t max_skip_minutes = 30;

    bool auto_calibrate = true;
    bool use_sound_detection = true;
    bool take_video = true;

    bool reset_first = false;
    bool finish_battle_on_shiny = false;
    bool close_summary_after_read = false;
};


struct StarterRngAttemptIO{
    BlinkCollectionDisplay* collection_display = nullptr;
    RngStateDisplay* state_display = nullptr;
    RngTargetDisplay* target_display = nullptr;
    EventNotificationOption* notification_shiny = nullptr;
    EventNotificationOption* notification_error = nullptr;
    StarterRngStats* stats = nullptr;

    std::function<void()> on_reset;
    std::function<void()> on_miss;
    std::function<void()> on_hit;
    std::function<void()> on_shiny;
    std::function<void()> on_error;
};


enum class StarterRngOutcome{
    HIT,
    SHINY,
    OUT_OF_RESETS,
    ABANDONED,
};


struct StarterRngRunResult{
    using Outcome = StarterRngOutcome;
    using enum StarterRngOutcome;

    Outcome outcome = Outcome::ABANDONED;
    Pokemon::BdspPokemonResult target{};
    uint64_t target_advance = 0;
    uint64_t resets_used = 0;
    std::string last_failure_reason;
    //  False when a shiny was caught but the game was not left where
    //  `finish_battle_on_shiny` and `close_summary_after_read` promise: the
    //  caller must not go on from it.
    bool settled = true;
};


struct BdspStarterAttemptResult{
    BdspAttemptOutcome outcome = BdspAttemptOutcome::Abandoned;
    bool shiny = false;
    Pokemon::BdspPokemonResult target{};
    uint64_t target_advance = 0;
    std::string failure_reason;
    //  See StarterRngRunResult::settled.
    bool settled = true;
};


//  Pure helper that maps the latest attempt result and reset count to a
//  finished `StarterRngRunResult` when the loop should terminate, or returns
//  `false` when the loop should continue to the next reset.
bool resolve_starter_rng_step(
    const BdspStarterAttemptResult& attempt,
    uint64_t resets_used,
    uint16_t max_resets,
    StarterRngRunResult& result
);


BdspStarterAttemptResult run_starter_rng_attempt(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const StarterRngSettings& settings,
    const StarterRngAttemptIO& io,
    RngAim& aim
);


StarterRngRunResult run_starter_rng_attempts(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const StarterRngSettings& settings,
    const StarterRngAttemptIO& io,
    RngAim& aim
);


}
}
}
#endif
