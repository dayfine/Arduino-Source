/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plays Pokemon Legends: Z-A's main story: the story table
 *  (PokemonLZA_AutoStoryTable.h) run by the shared story engine
 *  (Pokemon/AutoStory), from the start checkpoint to the end one.
 *
 *  For lab tuning: at each start, a checkpoint file in
 *  <user files>/AutoStoryDev/plza/ (story_dev_folder("plza")), e.g.
 *  plza-02.01.plan.json, replaces the built-in one of the same name, so a
 *  walk's legs can be retimed without a rebuild. The log names each file
 *  used; a bad one fails the run.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStory_H
#define PokemonAutomation_PokemonLZA_AutoStory_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonTools/Options/LanguageOCROption.h"
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
    //  The language the game is set to, required: the steps that read text
    //  (the fast travel menu's place names) read it in this language. The
    //  program does not check that the game matches.
    OCR::LanguageOCROption LANGUAGE;
    //  The start and end checkpoints, attempts and checks. Its own language
    //  dropdown is hidden: LANGUAGE above is PLZA's.
    Pokemon::StoryRunOptions RUN;
    EnumDropdownOption<StarterChoice> STARTER;
    BattleAIOption BATTLE_AI;
    //  Off: a run of several checkpoints runs straight on at the rows whose
    //  plans say "reload_when_chained"; on: it reloads the save there.
    BooleanCheckBoxOption RELOAD_AT_BOUNDARIES;
    //  On: the run starts from the backup save; off: from the latest, or
    //  the backup save when the latest is past the start check.
    BooleanCheckBoxOption START_FROM_BACKUP_SAVE;

    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;
    EventNotificationsOption NOTIFICATIONS;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
