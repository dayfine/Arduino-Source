/*  Isolated Hunt Prototype
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_IsolatedHuntPrototype_H
#define PokemonAutomation_PokemonSV_IsolatedHuntPrototype_H

#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonTools/Options/LanguageOCROption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_EncounterBotCommon.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_IsolatedHuntTable.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_IsolatedHuntSandwichOption.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldSensors.h"
#include "PokemonSV_LetsGoTools.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

class IsolatedHuntPrototype_Descriptor : public SingleSwitchProgramDescriptor{
public:
    IsolatedHuntPrototype_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

class IsolatedHuntPrototype : public SingleSwitchProgramInstance{
public:
    using Descriptor = IsolatedHuntPrototype_Descriptor;
    IsolatedHuntPrototype();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    void add_options();
    void run_hunts(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    bool run_one_hunt(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const IsolatedHuntRequest& request);
    bool approach(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    bool clear_pass(SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        OverworldSensors& sensors, uint8_t& empty);
    std::string clear_spot(SingleSwitchProgramEnvironment& env, ProControllerContext& context, OverworldSensors& sensors);
    bool process_encounter(SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        OverworldSensors& sensors, OverworldBattleTracker& battle_tracker, LetsGoEncounterBotTracker& encounters);

    enum class Start{
        HUNT_FROM_ZERO_GATE,
        PREPARED_SPOT,
        CENTRAL_PLAZA,
        RETURN_TO_ZERO_GATE,
    };

    OCR::LanguageOCROption LANGUAGE;
    EnumDropdownOption<Start> START;
    IsolatedHuntTable HUNTS;
    IsolatedHuntSandwichOption SANDWICH_OPTIONS;
    SimpleIntegerOption<uint16_t> RUN_SECONDS;
    SimpleIntegerOption<uint16_t> MAX_ROUNDS;
    SimpleIntegerOption<uint8_t> EMPTY_PASSES;
    EncounterBotCommonOptions ENCOUNTER_OPTIONS;
    EventNotificationsOption NOTIFICATIONS;
};

}
}
}
#endif
