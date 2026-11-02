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
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SeedsDatabase.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryRngPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class AutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutoStory_Descriptor();
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
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
    //  The Starter RNG attempts the RNG plan options ask for. Throws
    //  UserSetupError for an option it can't read, or when no target passes.
    std::vector<RngAttempt> plan_starter_attempts(ConsoleHandle& console) const;

    //  The start and end checkpoints, attempts, checks and language, as in
    //  every game's AutoStory.
    Pokemon::StoryRunOptions RUN;
    BooleanCheckBoxOption NEW_GAME_FROM_TITLE;
    StringOption PLAYER_NAME;
    //  The RNG programs the run stops for (StoryHandoff).
    BooleanCheckBoxOption STOP_FOR_SID_HELPER;
    BooleanCheckBoxOption STOP_FOR_STARTER_RNG;
    //  What the Starter RNG stop lists (PokemonFRLG_AutoStoryRngPlan.h).
    EnumDropdownOption<SoundSetting> RNG_SOUND;
    SimpleIntegerOption<uint16_t> RNG_TID;
    StringOption RNG_SID;
    SimpleIntegerOption<uint32_t> SID_HELPER_ADVANCES;
    SimpleIntegerOption<uint8_t> SID_HELPER_CANDIDATES;
    BooleanCheckBoxOption RNG_SHINY_REQUIRED;
    StringOption RNG_NATURES;
    StringOption RNG_MIN_IVS;
    SimpleIntegerOption<uint8_t> RNG_MAX_ATTEMPTS;
    EventNotificationsOption NOTIFICATIONS;
};



}
}
}
#endif
