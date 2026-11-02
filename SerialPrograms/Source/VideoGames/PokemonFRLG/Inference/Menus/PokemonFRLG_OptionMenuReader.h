/*  Option Menu Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads the values the OPTION menu shows. pokefirered option_menu.c draws
 *  them in the window at tile (2, 7), from x 130 of it, one row every 13
 *  pixels from y 2 (FONT_NORMAL's 14-pixel letters overlap by one), in
 *  LIGHT_RED with a RED shadow: peach letters, red edges. The rows but the
 *  cursor's are darkened by 2/16. English only so far: in another language
 *  a value is never read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OptionMenuReader_H
#define PokemonAutomation_PokemonFRLG_OptionMenuReader_H

#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The options AutoStory sets.
enum class OptionMenuItem{
    TEXT_SPEED,
    BATTLE_STYLE,
};

//  "TEXT SPEED", "BATTLE STYLE", for logs.
const char* option_menu_name(OptionMenuItem item);

//  The item's row in the menu, from 0. The cursor opens on row 0.
size_t option_menu_row(OptionMenuItem item);

//  The item's values as slugs, in the order RIGHT steps through them:
//  "slow", "mid", "fast"; "shift", "set" (option_menu.c sTextSpeedOptions,
//  sBattleStyleOptions).
const std::vector<std::string>& option_menu_values(OptionMenuItem item);

//  Where row `row` shows its value, as fractions of the game screen.
ImageFloatBox option_value_box(size_t row);

//  The value the item's row shows, one of option_menu_values(item), in a
//  game in `language`; empty if it can't be read.
std::string read_option_value(Logger& logger, Language language, const ImageViewRGB32& screen, OptionMenuItem item);



}
}
}
#endif
