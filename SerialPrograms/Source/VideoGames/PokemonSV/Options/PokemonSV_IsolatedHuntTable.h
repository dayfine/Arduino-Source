#ifndef PokemonAutomation_PokemonSV_IsolatedHuntTable_H
#define PokemonAutomation_PokemonSV_IsolatedHuntTable_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EditableTableOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonTools/Options/StringSelectOption.h"
#include "VideoGames/PokemonSwSh/Options/PokemonSwSh_BallSelectOption.h"
#include "VideoGames/PokemonSV/Programs/ShinyHunting/PokemonSV_IsolatedHuntPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

class IsolatedHuntRow : public EditableTableRow{
public:
    IsolatedHuntRow(EditableTableOption& parent);
    virtual std::unique_ptr<EditableTableRow> clone() const override;
    IsolatedHuntRequest snapshot() const;
private:
    BooleanCheckBoxCell enabled;
    StringSelectCell pokemon;
    EnumDropdownCell<IsolatedEncounterType> type;
    EnumDropdownCell<IsolatedHuntLocation> location;
    EnumDropdownCell<IsolatedShinyAction> shiny_action;
    PokemonSwSh::PokemonBallSelectCell ball;
    SimpleIntegerCell<uint16_t> ball_limit;
};

class IsolatedHuntTable : public EditableTableOption_t<IsolatedHuntRow>{
public:
    IsolatedHuntTable();
    virtual std::vector<std::string> make_header() const override;
    std::vector<IsolatedHuntRequest> snapshot() const;
private:
    std::vector<std::unique_ptr<EditableTableRow>> make_defaults();
};

}
}
}
#endif
