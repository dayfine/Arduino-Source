/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plays Pokemon Legends: Z-A's main story: the story table
 *  (PokemonLZA_AutoStoryTable.h) run by the shared story engine
 *  (Pokemon/AutoStory), from the start checkpoint to the end one.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStory_H
#define PokemonAutomation_PokemonLZA_AutoStory_H

#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Descriptor for the Pokemon Legends: Z-A AutoStory program.
class AutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutoStory_Descriptor();

    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


//  Runs the story's checkpoints from the start one to the end one on the
//  shared story engine.
class AutoStory : public SingleSwitchProgramInstance{
public:
    using Descriptor = AutoStory_Descriptor;
    AutoStory();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    //  The start and end checkpoints, attempts, checks and language.
    Pokemon::StoryRunOptions RUN;
    EnumDropdownOption<StarterChoice> STARTER;
    BattleAIOption BATTLE_AI;

    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;
    EventNotificationsOption NOTIFICATIONS;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
