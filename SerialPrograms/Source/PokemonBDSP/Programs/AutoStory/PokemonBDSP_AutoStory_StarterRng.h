/*  BDSP AutoStory: Starter RNG
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Run Starter RNG in AutoStory: its options, and the action that 00.02 runs
 *  in place of its Lake Verity steps (the Starter RNG program's reset loop,
 *  PokemonBDSP_StarterRngAttempts.h, called from the story).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StarterRng_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StarterRng_H

#include <stdint.h>
#include <functional>
#include <string>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/Notifications/EventNotificationOption.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "PokemonBDSP/Options/PokemonBDSP_PlayerModelOption.h"
#include "PokemonBDSP/Options/PokemonBDSP_RngFilter.h"
#include "PokemonBDSP/Programs/RngManipulation/PokemonBDSP_StarterRngAttempts.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Maps AutoStory's starter option to Starter RNG's starter enum.
BdspStarter bdsp_starter_for_choice(StarterChoice choice);


//  What the story tells Starter RNG about the game it plays.
struct StoryStarterRngConfig{
    Language language = Language::English;
    //  The player's appearance, 1 to 8: the lake's eye templates depend on it.
    uint8_t player_model = 1;
    StarterChoice starter = StarterChoice::CHIMCHAR;
    uint16_t max_resets = 0;
};


//  AutoStory's options for Starter RNG. Only RUN is always shown; the rest
//  show while it is on.
class StoryStarterRngOptions{
public:
    StoryStarterRngOptions();

    void set_details_visible(bool visible);
    //  The options' values, for a story in `language` with `starter`.
    StoryStarterRngConfig config(Language language, StarterChoice starter) const;

    BooleanCheckBoxOption& run(){ return RUN; }
    PlayerModelOption& player_model(){ return PLAYER_MODEL; }
    SimpleIntegerOption<uint16_t>& max_resets(){ return MAX_RESETS; }
    BdspRngFilterTable& filters(){ return FILTERS; }
    EventNotificationOption& notification_shiny(){ return NOTIFICATION_SHINY; }

private:
    BooleanCheckBoxOption RUN;
    PlayerModelOption PLAYER_MODEL;
    SimpleIntegerOption<uint16_t> MAX_RESETS;
    BdspRngFilterTable FILTERS;
    EventNotificationOption NOTIFICATION_SHINY;
};


//  Starter RNG's settings for the run in 00.02: from the Verity Lakefront
//  save, through the Starly battle and Rowan's scene on a shiny or not, to
//  the Lakefront overworld with the menu closed.
StarterRngSettings make_starter_rng_settings(const StoryStarterRngConfig& config, const BdspRngFilterSnapshot& filters);

//  Why the story cannot go on after Starter RNG ended as `result`; empty if
//  it can: a starter was caught and the game stands on the Lakefront.
std::string starter_rng_failure(const StarterRngRunResult& result);

//  The action 00.02 runs. Throws a UserSetupError now if the filter table is
//  empty. The action itself throws when Starter RNG fails, so that no row
//  retry reruns the whole reset loop.
std::function<Pokemon::Outcome()> make_starter_rng_hook(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryStarterRngOptions& options, const StoryStarterRngConfig& config,
    EventNotificationOption& error_notification
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
