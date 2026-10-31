#include "PokemonSV_IsolatedHuntText.h"
#include "PokemonSV_IsolatedHuntPower.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
std::u32string expected_power(Language language, IsolatedEncounterType type, uint8_t level){
    if ((type != IsolatedEncounterType::FIRE && type != IsolatedEncounterType::GRASS) || (level != 2 && level != 3)){
        return {};
    }
    const bool fire = type == IsolatedEncounterType::FIRE;
    std::u32string name;
    switch (language){
    case Language::English: name = fire ? U"encounterpowerfirelv" : U"encounterpowergrasslv"; break;
    case Language::ChineseTraditional: name = fire ? U"遭遇力火lv" : U"遭遇力草lv"; break;
    default: return {};
    }
    return name + utf8_to_utf32(std::to_string(level));
}
}
bool verify_isolated_power(IsolatedPowerActions& actions, Language language, IsolatedEncounterType type, uint8_t level){
    const auto expected = expected_power(language, type, level);
    if (expected.empty() || !actions.open_panel()){ return false; }
    for (size_t read = 0; read < 2; read++){
        if (normalize_isolated_text(actions.read_encounter_power()) != expected){ return false; }
    }
    return actions.close_panel();
}
}
}
}
