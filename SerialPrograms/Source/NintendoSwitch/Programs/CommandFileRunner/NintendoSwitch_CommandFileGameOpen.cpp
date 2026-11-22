/*  Command File "game_open"
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "NintendoSwitch_CommandFileGameOpen.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


std::optional<std::string_view> game_closed_reason(const GameOpenScreen& screen){
    if (!screen.has_frame){
        return "no_video";
    }
    if (screen.home){
        return "home";
    }
    if (screen.user_select){
        return "user_select";
    }
    if (screen.black){
        return "black";
    }
    return std::nullopt;
}



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
