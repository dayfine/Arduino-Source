/*  Encounter Bot Common
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_EncounterBotCommon_H
#define PokemonAutomation_PokemonSV_EncounterBotCommon_H

#include "Common/Cpp/Options/BatchOption.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "CommonFramework/Notifications/EventNotificationOption.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonSV_EncounterActionsTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

using namespace Pokemon;


class EncounterBotCommonOptions : public BatchOption{
public:
    EncounterBotCommonOptions()
        : BatchOption(LockMode::UNLOCK_WHILE_RUNNING)
        , USE_FIRST_MOVE_IF_CANNOT_THROW_BALL(
            "<b>Use 1st Move if Cannot Throw Ball:</b><br>"
            "If you can't throw a ball because the opponent is semi-invulnerable, use the 1st move instead. "
            "Therefore, your first move should be non-damaging to avoid killing the wild " + STRING_POKEMON + ".",
            LockMode::UNLOCK_WHILE_RUNNING,
            true
        )
        , USE_FIRST_MOVE_BEFORE_THROWING(
            "<b>Use 1st Move Once Before Throwing Balls:</b><br>"
            "When the action is to throw balls, use the 1st move once before the first throw, "
            "e.g. Thunder Wave to paralyze the target. It must not deal damage.",
            LockMode::UNLOCK_WHILE_RUNNING,
            false
        )
        , VIDEO_ON_SHINY(
            "<b>Video Capture:</b><br>Take a video of the encounter if it is shiny.",
            LockMode::UNLOCK_WHILE_RUNNING,
            true
        )
        , NOTIFICATION_NONSHINY(
            "Non-Shiny Encounter",
            true, false,
            {"Notifs"},
            std::chrono::seconds(3600)
        )
        , NOTIFICATION_SHINY(
            "Shiny Encounter",
            true, true, ImageAttachmentMode::JPG,
            {"Notifs", "Showcase"}
        )
        , NOTIFICATION_CATCH_SUCCESS(
            "Catch Success",
            true, false,
            {"Notifs"}
        )
        , NOTIFICATION_CATCH_FAILED(
            "Catch Failed",
            true, true,
            {"Notifs"}
        )
    {
        PA_ADD_OPTION(ACTIONS_TABLE);
        PA_ADD_OPTION(USE_FIRST_MOVE_IF_CANNOT_THROW_BALL);
        PA_ADD_OPTION(USE_FIRST_MOVE_BEFORE_THROWING);
        PA_ADD_OPTION(VIDEO_ON_SHINY);
    }

    EncounterActionsTable ACTIONS_TABLE;
    BooleanCheckBoxOption USE_FIRST_MOVE_IF_CANNOT_THROW_BALL;
    BooleanCheckBoxOption USE_FIRST_MOVE_BEFORE_THROWING;
    BooleanCheckBoxOption VIDEO_ON_SHINY;

    EventNotificationOption NOTIFICATION_NONSHINY;
    EventNotificationOption NOTIFICATION_SHINY;
    EventNotificationOption NOTIFICATION_CATCH_SUCCESS;
    EventNotificationOption NOTIFICATION_CATCH_FAILED;
};




}
}
}
#endif
