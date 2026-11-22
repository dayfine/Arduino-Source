/*  Legendary Catch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_LegendaryCatch_H
#define PokemonAutomation_PokemonFRLG_LegendaryCatch_H

#include <vector>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonTools/Options/LanguageOCROption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonFRLG_LegendaryCatchRequirements.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class LegendaryCatch_Descriptor : public SingleSwitchProgramDescriptor{
public:
    LegendaryCatch_Descriptor();
};


class LegendaryCatch : public SingleSwitchProgramInstance{
public:
    using Descriptor = LegendaryCatch_Descriptor;
    LegendaryCatch();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

private:
    std::vector<LegendaryTarget> selected_targets() const;

    void run_preconditions(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        const LegendaryCatchRequirements& requirements
    );

    std::vector<ObservedPartyMember> read_party_members(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context
    );

    std::map<std::string, unsigned> read_bag_items(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        const LegendaryCatchRequirements& requirements
    );

private:
    OCR::LanguageOCROption LANGUAGE;

    SectionDividerOption STATIC_HEADER;
    BooleanCheckBoxOption CATCH_ARTICUNO;
    BooleanCheckBoxOption CATCH_ZAPDOS;
    BooleanCheckBoxOption CATCH_MOLTRES;
    BooleanCheckBoxOption CATCH_MEWTWO;
    BooleanCheckBoxOption CATCH_DEOXYS;
    BooleanCheckBoxOption CATCH_LUGIA;
    BooleanCheckBoxOption CATCH_HO_OH;

    SectionDividerOption ROAMER_HEADER;
    BooleanCheckBoxOption CATCH_RAIKOU;
    BooleanCheckBoxOption CATCH_ENTEI;
    BooleanCheckBoxOption CATCH_SUICUNE;

    SectionDividerOption PRECONDITION_HEADER;
    SimpleIntegerOption<uint16_t> LUXURY_BALLS_PER_LEGENDARY;
    BooleanCheckBoxOption REQUIRE_TYPE_WALLS;
    BooleanCheckBoxOption CHECK_BAG_ITEMS;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif
