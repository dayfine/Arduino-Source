/*  Mail Glitch Activation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Automates activating the FRLG Retro Mail Glitch (Knock Off + Recycle)
 *  on MIMIEN from any outdoor Fly-capable save with Celadon, Vermilion,
 *  Pewter, and Lavender unlocked, using a linear pokesim-authored-checkpoint/1
 *  plan (mg-01..mg-05) on StoryEngine.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MailGlitchActivation_H
#define PokemonAutomation_PokemonFRLG_MailGlitchActivation_H

#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class MailGlitchActivation_Descriptor : public SingleSwitchProgramDescriptor{
public:
    MailGlitchActivation_Descriptor();
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class MailGlitchActivation : public SingleSwitchProgramInstance{
public:
    using Descriptor = MailGlitchActivation_Descriptor;
    MailGlitchActivation();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

private:
    Pokemon::StoryRunOptions RUN;
    EnumDropdownOption<PlayerGender> PLAYER_GENDER;
    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
