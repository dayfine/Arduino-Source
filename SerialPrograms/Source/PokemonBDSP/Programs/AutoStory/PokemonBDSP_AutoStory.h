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
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


class BDSPAutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    BDSPAutoStory_Descriptor();

    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class BDSPAutoStory : public SingleSwitchProgramInstance{
public:
    using Descriptor = BDSPAutoStory_Descriptor;
    BDSPAutoStory();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    //  The start and end checkpoints, attempts, checks and language.
    Pokemon::StoryRunOptions RUN;
    BooleanCheckBoxOption NEW_GAME_INTRO;
    StringOption PLAYER_NAME;
    EnumDropdownOption<StarterChoice> STARTER;

    EventNotificationsOption NOTIFICATIONS;
};



}
}
}
#endif
