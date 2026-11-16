/*  PokemonLZA AutoStory - Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The one way AutoStory starts or restarts PLZA from the Switch HOME: for
 *  a new game, for the latest save and for capture mode. HOME's tiles are
 *  in last-played order, so the first tile is not always PLZA's (console B,
 *  2026-11-15: FireRed's, played last, launched instead).
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


//  How many of HOME's tiles, from the first, are searched for PLZA's.
constexpr size_t HOME_TILES_SEARCHED = 8;

//  HOME's tile search from the highlighted tile rightward: reads each tile
//  with `highlighted_is_plza`, moving one tile right with `move_right`
//  between reads, and returns how many moves put PLZA's tile under the
//  cursor; nullopt if none of HOME_TILES_SEARCHED tiles is PLZA's.
std::optional<size_t> find_plza_tile(
    const std::function<bool()>& highlighted_is_plza,
    const std::function<void()>& move_right
);

//  From the game or HOME: goes HOME, closes the running game whichever it
//  is (the first tile, the last played), selects PLZA's tile by its title
//  and launches it through the user select, until the game's black screen.
//  Logs whether the closed game was PLZA. Throws UserSetupError, having
//  launched nothing, if PLZA's tile is not among HOME's first
//  HOME_TILES_SEARCHED.
void launch_plza_from_home(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
