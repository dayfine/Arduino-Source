/*  Isolated Hunt Flight Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#include "PokemonSV_IsolatedHuntText.h"
#include "PokemonSV_IsolatedHuntFlight.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
std::u32string central_name(Language language){
    switch (language){
    case Language::English: return U"westprovinceareaonecentral";
    case Language::ChineseTraditional: return U"西第1區中央";
    default: return {};
    }
}
}
bool fly_to_isolated_central(IsolatedFlightActions& actions, Language language){
    const auto expected = central_name(language);
    if (expected.empty() || !actions.open_confirmation()){
        return false;
    }
    // Both reads must contain the complete name; never fuzzy-match North to Central.
    for (size_t read = 0; read < 2; read++){
        if (normalize_isolated_text(actions.read_destination()) != expected){
            return false;
        }
    }
    return actions.confirmation_visible() && actions.confirm_and_wait();
}
}
}
}
