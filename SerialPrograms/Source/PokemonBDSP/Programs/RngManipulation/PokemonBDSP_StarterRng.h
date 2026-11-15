/*  BDSP Starter RNG
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_StarterRng_H
#define PokemonAutomation_PokemonBDSP_StarterRng_H

#include <stdint.h>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonTools/Options/LanguageOCROption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "PokemonBDSP/Options/PokemonBDSP_PlayerModelOption.h"
#include "PokemonBDSP/Options/PokemonBDSP_RngFilter.h"
#include "PokemonBDSP_RngAim.h"
#include "PokemonBDSP_RngDisplays.h"
#include "PokemonBDSP_StarterNavigation.h"
#include "PokemonBDSP_StarterRngAttempts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


class StarterRng_Descriptor : public SingleSwitchProgramDescriptor{
public:
    StarterRng_Descriptor();

    using Stats = StarterRngStats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class StarterRng : public SingleSwitchProgramInstance, public ConfigOption::Listener{
public:
    using Descriptor = StarterRng_Descriptor;
    StarterRng();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    virtual void on_config_value_changed(void* object) override;

private:
    RngAim m_aim;

private:
    OCR::LanguageOCROption LANGUAGE;
    PlayerModelOption PLAYER_MODEL;
    EnumDropdownOption<BdspStartPoint> START_POINT;
    EnumDropdownOption<BdspStarter> STARTER;

    BdspRngFilterTable FILTERS;

    BlinkCollectionDisplay COLLECTION_DISPLAY;
    RngStateDisplay STATE_DISPLAY;
    RngTargetDisplay TARGET_DISPLAY;

    SimpleIntegerOption<uint16_t> MAX_RESETS;
    SimpleIntegerOption<uint16_t> MAX_TARGET_WAIT_MINUTES;
    SimpleIntegerOption<uint16_t> MAX_SKIP_MINUTES;

    BooleanCheckBoxOption AUTO_CALIBRATE;

    BooleanCheckBoxOption USE_SOUND_DETECTION;
    BooleanCheckBoxOption TAKE_VIDEO;
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;

    EventNotificationOption NOTIFICATION_SHINY;
    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif
