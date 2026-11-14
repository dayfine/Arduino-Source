/*  Legacy FRLG Checkpoint Export Format
 *  From: https://github.com/PokemonAutomation/
 */

#ifndef PokemonAutomation_PokemonFRLG_LegacyJsonFormat_H
#define PokemonAutomation_PokemonFRLG_LegacyJsonFormat_H

#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace StoryJsonInternal{

//  The envelope constants the export writes and the reader requires.
constexpr std::string_view FORMAT = "pokesim-native-checkpoint-export/1";
//  Game identifier in the legacy envelope.
constexpr std::string_view GAME = "frlg";
//  Marks a captured table row rather than an authored plan.
constexpr std::string_view STATUS = "native-table-export";
//  Describes the native bookkeeping snapshot, not observed game state.
constexpr std::string_view STATE_BASIS = "native-table-bookkeeping";
//  Fields deliberately left outside the captured native table state.
inline const std::vector<std::string>& unbound_fields(){
    static const std::vector<std::string> UNBOUND{
        "runtime_configuration", "map_pins", "executor_bindings", "entry_evidence", "effective_save_retry_policy",
    };
    return UNBOUND;
}

//  Stable legacy spellings of the native START menu variants.
inline std::string_view menu_name(StartMenu menu){
    switch (menu){
    case StartMenu::BEFORE_POKEMON: return "before_pokemon";
    case StartMenu::BEFORE_POKEDEX: return "before_pokedex";
    case StartMenu::FULL: return "full";
    case StartMenu::SAFARI: break;
    }
    throw ParseException("Unsupported FRLG START menu.");
}

} // namespace StoryJsonInternal

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

#endif
