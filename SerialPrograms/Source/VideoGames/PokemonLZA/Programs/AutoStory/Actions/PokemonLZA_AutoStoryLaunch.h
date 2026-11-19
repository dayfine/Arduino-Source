/*  PokemonLZA AutoStory - Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The one way AutoStory starts or restarts PLZA from the Switch HOME: for
 *  a new game and for the latest save. HOME's tiles are
 *  in last-played order, so the first tile is not always PLZA's.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryLaunch_H
#define PokemonAutomation_PokemonLZA_AutoStoryLaunch_H

#include <cstddef>
#include <functional>
#include <optional>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  How many of HOME's tiles are searched for PLZA's.
constexpr size_t HOME_TILES_SEARCHED = 8;

//  How many `move_right` calls put PLZA's tile under the cursor; nullopt if
//  none of HOME_TILES_SEARCHED tiles is PLZA's.
std::optional<size_t> find_plza_tile(
    const std::function<bool()>& highlighted_is_plza,
    const std::function<void()>& move_right
);

//  From the game or HOME: closes the running game, whichever it is, and
//  launches PLZA up to its black screen. Throws UserSetupError, having
//  launched nothing, if PLZA's tile is not among HOME_TILES_SEARCHED.
void launch_plza_from_home(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
