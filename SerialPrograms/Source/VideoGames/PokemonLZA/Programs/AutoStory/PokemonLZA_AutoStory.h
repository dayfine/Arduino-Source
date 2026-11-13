/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plays Pokemon Legends: Z-A's main story: the story table
 *  (PokemonLZA_AutoStoryTable.h) run by the shared story engine
 *  (Pokemon/AutoStory), from the start checkpoint to the end one.
 *
 *  For lab tuning: at each start, a checkpoint file in
 *  <user files>/AutoStoryDev/plza/ (story_dev_folder()), e.g.
 *  plza-02.01.plan.json, replaces the built-in one of the same name, so a
 *  walk's legs can be retimed without a rebuild. The log names each file
 *  used; a bad one fails the run.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStory_H
#define PokemonAutomation_PokemonLZA_AutoStory_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StringOption.h"
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

    //  The start frame's black border check, except for a start checkpoint
    //  that launches the game (story_row_launches_game()): the launch checks
    //  the border on HOME instead.
    virtual void start_program_border_check(VideoStream& stream, FeedbackType feedback_type) override;

private:
    //  The new game's choices; with `check`, throws UserSetupError for ones the
    //  intro cannot play.
    NewGameChoices new_game_choices(ConsoleHandle& console, bool check) const;

    //  The start and end checkpoints, attempts, checks and language.
    Pokemon::StoryRunOptions RUN;
    EnumDropdownOption<StarterChoice> STARTER;
    BattleAIOption BATTLE_AI;
    //  The new game's choices (01.00): the language is RUN's. The look and
    //  the language list's order are guesses from Scarlet/Violet's screens.
    StringOption PLAYER_NAME;
    SimpleIntegerOption<uint8_t> PLAYER_LOOK;
    BooleanCheckBoxOption NAME_FIRST_LETTER_AUTO_CAPS;

    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;
    EventNotificationsOption NOTIFICATIONS;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
