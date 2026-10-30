#pragma once

#include <memory>
#include <vector>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonTools/Options/StringSelectOption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonBDSP_AutoStoryTools.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

class BDSPAutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    BDSPAutoStory_Descriptor();

    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class BDSPAutoStory : public SingleSwitchProgramInstance, public ConfigOption::Listener{
public:
    using Descriptor = BDSPAutoStory_Descriptor;
    ~BDSPAutoStory();
    BDSPAutoStory();

    virtual void program(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context
        ) override;


private:
    virtual void on_config_value_changed(void* object) override;
    void update_ui_state();
private:
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    BooleanCheckBoxOption DIAGNOSTIC_POST_CATCH_ONLY;
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    BooleanCheckBoxOption DIAGNOSTIC_FAKE_CHECKPOINT_SAVE;
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    BooleanCheckBoxOption DIAGNOSTIC_ROUTE202_TO_SANDGEM_CENTER;
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    BooleanCheckBoxOption DIAGNOSTIC_SANDGEM_MART_LADY;
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    BooleanCheckBoxOption DIAGNOSTIC_JUBILIFE_CENTER;
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    BooleanCheckBoxOption DIAGNOSTIC_JUBILIFE_SCHOOL;
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    BooleanCheckBoxOption DIAGNOSTIC_JUBILIFE_BARRY;
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    BooleanCheckBoxOption DIAGNOSTIC_JUBILIFE_PRESIDENT;
    // DIAGNOSTIC ONLY: Remove before pushing to production.
    BooleanCheckBoxOption DIAGNOSTIC_JUBILIFE_CLOWN_MART;
    BooleanCheckBoxOption ENABLE_ADVANCED_MODE;

    StringSelectOption START_CHECKPOINT_TUTORIAL;
    StringSelectOption END_CHECKPOINT_TUTORIAL;
    StaticTextOption START_DESCRIPTION_SEGMENT;
    StaticTextOption END_DESCRIPTION_SEGMENT;

    StaticTextOption START_DESCRIPTION_CHECKPOINT;
    StaticTextOption END_DESCRIPTION_CHECKPOINT;

    StringSelectOption START_SEGMENT;
    StringSelectOption END_SEGMENT;

    BooleanCheckBoxOption                  NEW_GAME_INTRO;
    StringOption                           PLAYER_NAME;
    EnumDropdownOption<StarterChoice>      STARTERCHOICE;
    BooleanCheckBoxOption                  STARTER_SHINY;
    BooleanCheckBoxOption                  CATCH_LEGENDARY;
    EnumDropdownOption<HelpMode>           HELP_MODE;
    EnumDropdownOption<OutsideHelpPokemon> OUTSIDE_HELP_POKEMON;
    EnumDropdownOption<TorterraMove>       OUTSIDE_HELP_ATTACK_1;
    EnumDropdownOption<TorterraMove>       OUTSIDE_HELP_ATTACK_2;
    EnumDropdownOption<TorterraMove>       OUTSIDE_HELP_ATTACK_3;
    EnumDropdownOption<TorterraMove>       OUTSIDE_HELP_ATTACK_4;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};

}
}
}
