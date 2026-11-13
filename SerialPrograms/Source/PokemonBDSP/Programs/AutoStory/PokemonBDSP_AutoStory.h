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

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "PokemonBDSP_AutoStory_StarterRng.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


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

    //  Starts the plan's story and runs it to its end or its stop.
    void run_plan(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        StoryExecutor& executor, const StoryRunPlan& plan, bool new_game
    );

    //  The start and end checkpoints, attempts, checks and language.
    Pokemon::StoryRunOptions RUN;
    BooleanCheckBoxOption NEW_GAME_INTRO;
    StringOption PLAYER_NAME;
    EnumDropdownOption<StarterChoice> STARTER;
    //  The RNG program the run stops for (StoryHandoff), or runs in 00.02.
    BooleanCheckBoxOption STOP_FOR_STARTER_RNG;
    StoryStarterRngOptions STARTER_RNG;

    EventNotificationsOption NOTIFICATIONS;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
