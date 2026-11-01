/*  BDSP AutoStory - Poke Mart
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Mart_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Mart_H

#include <vector>
#include "CommonFramework/Language.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


enum class ShopItem{
    FullRestore,
    MaxPotion,
    HyperPotion,
    SuperPotion,
    Potion,
    Antidote,
    ParalyzHeal,
    Awakening,
    Repel,
    SuperRepel,
    MaxRepel,
    EscapeRope,
};

//  Pass as the `quantity` field to buy the maximum available amount.
inline constexpr int BUY_MAX = -1;

struct ItemToBuy{
    ShopItem item = ShopItem::Potion;
    int quantity = 0;   //  positive count to buy, or BUY_MAX
};

//  Walk up to a vendor (already talking to them), navigate through the Buy menu,
//  and purchase each item in `items` in order. The function presses DPAD_DOWN
//  until the matching item name is found via OCR, so items may appear in any
//  list order. Returns true on success.
bool buy_items(
    VideoStream& stream,
    ProControllerContext& context,
    const std::vector<ItemToBuy>& items,
    Language language = Language::English
);



}
}
}
#endif
