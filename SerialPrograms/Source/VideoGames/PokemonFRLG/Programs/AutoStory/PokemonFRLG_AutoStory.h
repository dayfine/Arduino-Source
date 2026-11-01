/*  AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plays FireRed from a new game to the Boulder Badge, as the story
 *  checkpoints of PokemonFRLG_AutoStoryTable.h run by the shared story
 *  engine. Each checkpoint saves at its end; one that fails is retried from
 *  that save, as the shared retry options say.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStory_H
#define PokemonAutomation_PokemonFRLG_AutoStory_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class AutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutoStory_Descriptor();
};

class AutoStory : public SingleSwitchProgramInstance{
public:
    using Descriptor = AutoStory_Descriptor;
    AutoStory();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

private:
    SimpleIntegerOption<uint8_t> START_CHECKPOINT;
    SimpleIntegerOption<uint8_t> END_CHECKPOINT;
    BooleanCheckBoxOption NEW_GAME_FROM_TITLE;
    Pokemon::StoryRunOptions RETRIES;
    EventNotificationsOption NOTIFICATIONS;
};



}
}
}
#endif
