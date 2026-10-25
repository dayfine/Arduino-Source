/*  Isolated Hunt Flight Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#include "Common/Cpp/Strings/Unicode.h"
#include "PokemonSV_IsolatedHuntFlight.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
std::u32string normalize_destination(const std::string& text){
    std::u32string result;
    for (char32_t ch : utf8_to_utf32(text)){
        if (ch >= U'０' && ch <= U'９'){ ch = ch - U'０' + U'0'; }
        if (ch >= U'A' && ch <= U'Z'){ ch = ch - U'A' + U'a'; }
        if ((ch >= U'a' && ch <= U'z') || (ch >= U'0' && ch <= U'9') ||
            (ch >= 0x4e00 && ch <= 0x9fff)){
            result += ch;
        }
    }
    return result;
}
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
        if (normalize_destination(actions.read_destination()) != expected){
            return false;
        }
    }
    return actions.confirmation_visible() && actions.confirm_and_wait();
}
}
}
}
