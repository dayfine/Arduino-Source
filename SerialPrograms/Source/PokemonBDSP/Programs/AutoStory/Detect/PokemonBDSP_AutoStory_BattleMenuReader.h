/*  BDSP AutoStory - Battle Menu Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which battle command and which move the cursor is on. Adapted from
 *  PokemonAutomation/Arduino-Source#1717 by Developer-Butters, which added
 *  them to PokemonBDSP_BattleMenuDetector; here they sit beside it, so the
 *  upstream detector stays as it is.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_BattleMenuReader_H
#define PokemonAutomation_PokemonBDSP_AutoStory_BattleMenuReader_H

#include <array>
#include <cstddef>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Vertical command order in the BDSP battle menu.
enum class BattleMenuOption{
    UNKNOWN = -1,
    BATTLE = 0,
    POKEMON = 1,
    BAG = 2,
    RUN = 3,
};

//  The command under the cursor: `menu` must see its battle menu, and white
//  selection brackets must sit at all four corners of exactly one command.
//  Missing or ambiguous brackets read UNKNOWN; the cursor is never guessed.
BattleMenuOption battle_menu_selected_option(BattleMenuDetector& menu, const ImageViewRGB32& screen);

//  Logical move slots are ordered top to bottom among the populated move
//  buttons. `selected_slot` is 1 to 4, or 0 when missing or ambiguous.
struct BattleMoveMenuState{
    size_t move_count = 0;
    size_t selected_slot = 0;
    std::array<ImageFloatBox, 4> buttons;
};
BattleMoveMenuState detect_battle_move_menu(const ImageViewRGB32& screen);



}
}
}
#endif
