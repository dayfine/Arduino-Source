#ifndef PokemonAutomation_PokemonSV_IsolatedHuntText_H
#define PokemonAutomation_PokemonSV_IsolatedHuntText_H
#include "Common/Cpp/Strings/Unicode.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
inline std::u32string normalize_isolated_text(const std::string& text){
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
}
}
}
#endif
