/*  Game Languages
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_GameLanguages.h"

namespace PokemonAutomation{
namespace Pokemon{


const EnumDropdownDatabase<Language>& Gen8ToGen9Languages_Database(){
    static const EnumDropdownDatabase<Language> database({
        {Language::English,            "en",      "English"},
        {Language::Japanese,           "ja",      "Japanese"},
        {Language::Spanish,            "es",      "Spanish"},
        {Language::French,             "fr",      "French"},
        {Language::German,             "de",      "German"},
        {Language::Italian,            "it",      "Italian"},
        {Language::Korean,             "ko",      "Korean"},
        {Language::ChineseSimplified,  "zh-Hans", "Chinese (Simplified)"},
        {Language::ChineseTraditional, "zh-Hant", "Chinese (Traditional)"},
    });
    return database;
}

const EnumDropdownDatabase<Language>& FireRedLeafGreenLanguages_Database(){
    static const EnumDropdownDatabase<Language> database({
        {Language::English,            "en",      "English"},
        {Language::Japanese,           "ja",      "Japanese"},
        {Language::Spanish,            "es",      "Spanish"},
        {Language::French,             "fr",      "French"},
        {Language::German,             "de",      "German"},
        {Language::Italian,            "it",      "Italian"},
    });
    return database;
}



} // namespace Pokemon
} // namespace PokemonAutomation
