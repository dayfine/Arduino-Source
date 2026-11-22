/*  Home Tile Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which game tile the HOME menu's cursor is on. HOME draws a cyan frame
 *  around the selected tile of the game row and writes that game's name
 *  above the row, also in cyan. The frame says that a tile is selected and
 *  where; the name says which game it is. Neither depends on the order of
 *  the row, which changes as games are played (the last one played moves
 *  to the first tile).
 *
 *  Measured on the Switch 1 HOME menu at 1920x1080; positions are parts of
 *  the screen, so other capture sizes read the same.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_HomeTileReader_H
#define PokemonAutomation_NintendoSwitch_HomeTileReader_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  The selected tile's cyan frame on `screen`; nothing when no tile of the
//  game row is selected (the cursor on the icons below it, a dialog or the
//  user select over HOME).
std::optional<ImageFloatBox> find_selected_home_tile(const ImageViewRGB32& screen);

//  The name HOME writes above the selected tile, as OCR reads it in
//  `language` (the console's, not the game's); nothing when no tile is
//  selected.
std::optional<std::string> read_selected_home_tile_name(Language language, const ImageViewRGB32& screen);

//  Whether a tile is selected and its name contains `name` ("Pokémon
//  Legends: Z-A", or a distinctive part of it), compared as OCR text:
//  case, accents, spaces and punctuation ignored.
bool selected_home_tile_is(Language language, const ImageViewRGB32& screen, std::string_view name);


//  The screenshot tests in CommandLineTests/NintendoSwitch/HomeTileReader/.
void add_tests_HomeTileReader(UnitTestDatabase& database);



} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
