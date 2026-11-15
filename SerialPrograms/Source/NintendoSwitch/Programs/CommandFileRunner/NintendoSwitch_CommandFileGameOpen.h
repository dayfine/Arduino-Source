/*  Command File "game_open"
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the "game_open" command makes of its detectors: whether a game is
 *  in front, or why not.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_CommandFileGameOpen_H
#define PokemonAutomation_NintendoSwitch_CommandFileGameOpen_H

#include <optional>
#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{

//  What the detectors of "game_open" saw on one frame.
struct GameOpenScreen{
    bool has_frame = false;
    bool home = false;          //  The Switch Home menu.
    bool user_select = false;   //  "Who is using the software?"
    bool black = false;         //  A black screen: a loading screen or no signal, so no game seen.
};

//  Why no game is in front on `screen` ("no_video", "home", "user_select" or
//  "black"), or nullopt when a game is. It does not tell which game: that needs a
//  game's own detector.
std::optional<std::string_view> game_closed_reason(const GameOpenScreen& screen);



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif
