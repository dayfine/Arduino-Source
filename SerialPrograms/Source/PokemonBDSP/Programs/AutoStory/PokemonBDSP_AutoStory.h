/*  BDSP AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 *  Plays Brilliant Diamond / Shining Pearl from a new game to Floaroma Town:
 *  the story table (PokemonBDSP_AutoStory_StoryTable.h) run by the shared
 *  story engine, from the start checkpoint to the end one.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_H
#define PokemonAutomation_PokemonBDSP_AutoStory_H

#include <stdint.h>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "PokemonBDSP/Options/PokemonBDSP_RngFilter.h"
#include "PokemonBDSP/Programs/RngManipulation/PokemonBDSP_StarterNavigation.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Maps AutoStory's starter option to Starter RNG's starter enum.
BdspStarter bdsp_starter_for_choice(StarterChoice choice);


//  Descriptor for the BDSP AutoStory single-switch program.
class BDSPAutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    BDSPAutoStory_Descriptor();

    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


//  Single-switch program instance executing BDSP story automation.
class BDSPAutoStory : public SingleSwitchProgramInstance, public ConfigOption::Listener{
public:
    using Descriptor = BDSPAutoStory_Descriptor;
    ~BDSPAutoStory();
    BDSPAutoStory();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    virtual void on_config_value_changed(void* object) override;

    //  The start and end checkpoints, attempts, checks and language.
    Pokemon::StoryRunOptions RUN;
    BooleanCheckBoxOption NEW_GAME_INTRO;
    StringOption PLAYER_NAME;
    EnumDropdownOption<StarterChoice> STARTER;
    //  The RNG program the run stops for or runs in 00.02 (StoryHandoff).
    BooleanCheckBoxOption STOP_FOR_STARTER_RNG;
    BooleanCheckBoxOption RUN_STARTER_RNG;
    SimpleIntegerOption<uint16_t> STARTER_RNG_MAX_RESETS;
    BdspRngFilterTable STARTER_RNG_FILTERS;

    EventNotificationOption NOTIFICATION_SHINY;
    EventNotificationsOption NOTIFICATIONS;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
