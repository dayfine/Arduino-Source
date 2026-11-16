/*  AutoStory Mart Sell
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Mart's SELL and its money box on the console, and what the clerk
 *  pays.
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_MoneyBoxReader.h"
#include "PokemonFRLG_AutoStoryPresses.h"
#include "PokemonFRLG_AutoStoryBag.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryPocketList.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

//  The bag slides in after SELL and ignores presses until it is done, as
//  from the START menu (PokemonFRLG_AutoStoryItemUse.cpp).
constexpr Milliseconds BAG_SLIDE_IN = 1500ms;
constexpr Milliseconds POCKET_TAP = 100ms;
constexpr Milliseconds POCKET_SWITCH_WAIT = 600ms;
constexpr Milliseconds CURSOR_TAP = 100ms;
constexpr Milliseconds CURSOR_WAIT = 150ms;
//  Out of the bag, "Is there anything else I may do?", SEE YA!.
constexpr Milliseconds MENU_CLOSE_TIMEOUT = 20000ms;

//  Half of pokefirered items.json's price, for the items the story sells.
struct SaleItem{
    std::string_view item;
    size_t price;
};
constexpr SaleItem SALE_ITEMS[] = {
    {"nugget", 10000 / 2},
};

} // namespace


std::optional<size_t> sale_price(std::string_view item){
    for (const SaleItem& sale : SALE_ITEMS){
        if (sale.item == item){
            return sale.price;
        }
    }
    return std::nullopt;
}


std::optional<size_t> ConsoleMartActions::read_money(){
    m_context.wait_for_all_requests();
    const VideoSnapshot screen = m_console.video().snapshot();
    if (!screen){
        return std::nullopt;
    }
    return read_money_box(m_console.logger(), screen);
}

SaleEnd ConsoleMartActions::sell(const MartLine& sale){
    //  "May I help you?" BUY, SELL, SEE YA!: SELL, and the bag slides in.
    //  Not timed on the console: the BUY waits.
    pbf_press_button(m_context, BUTTON_A, 160ms, 2000ms);
    pbf_press_dpad(m_context, DPAD_DOWN, 160ms, 300ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, BAG_SLIDE_IN);
    //  Left to the Items pocket, which doesn't wrap, and up to its top.
    for (int c = 0; c < 3; c++){
        pbf_press_dpad(m_context, DPAD_LEFT, POCKET_TAP, POCKET_SWITCH_WAIT);
    }
    for (int c = 0; c < 20; c++){
        pbf_press_dpad(m_context, DPAD_UP, CURSOR_TAP, CURSOR_WAIT);
    }
    const PocketRead read = read_open_pocket(m_console, m_context, m_language);
    for (size_t c = 0; c < read.downs; c++){
        pbf_press_dpad(m_context, DPAD_UP, CURSOR_TAP, CURSOR_WAIT);
    }
    const std::optional<size_t> row = bag_row_of(read.rows, sale.item);
    if (!row){
        return back_to_clerk(SaleEnd::NONE_IN_BAG);
    }
    const int held = read.rows[*row].count;
    if (held < 1){
        m_console.log(std::format("Mart: the {} count wasn't read; not sold.", sale.item), COLOR_RED);
        return back_to_clerk(SaleEnd::FAILED);
    }
    for (size_t c = 0; c < *row; c++){
        pbf_press_dpad(m_context, DPAD_DOWN, CURSOR_TAP, CURSOR_WAIT);
    }
    //  One held: straight to "I can pay $X." YES/NO. More: "How many would
    //  you like to sell?" on 1 first, typed as a purchase's.
    pbf_press_button(m_context, BUTTON_A, 160ms, 2500ms);
    if (held > 1){
        const QuantityPresses presses = quantity_presses(std::min<size_t>(sale.quantity, (size_t)held));
        for (size_t c = 0; c < presses.right; c++){
            pbf_press_dpad(m_context, DPAD_RIGHT, 160ms, 340ms);
        }
        for (size_t c = 0; c < presses.up; c++){
            pbf_press_dpad(m_context, DPAD_UP, 160ms, 340ms);
        }
        pbf_press_button(m_context, BUTTON_A, 160ms, 2500ms);
    }
    //  YES, then "Turned over the <item> and received $X.", back to the bag.
    pbf_press_button(m_context, BUTTON_A, 160ms, 2500ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 1500ms);
    return back_to_clerk(SaleEnd::SOLD);
}

SaleEnd ConsoleMartActions::back_to_clerk(SaleEnd end){
    //  B out of the bag to the clerk's menu, B (SEE YA!) and through
    //  "Please come again!".
    pbf_press_button(m_context, BUTTON_B, 160ms, 1500ms);
    try{
        mash_until_overworld(m_console, m_context, MENU_CLOSE_TIMEOUT);
        return end;
    }catch (OperationFailedException& e){
        m_console.log("Mart: " + e.message(), COLOR_RED);
        return SaleEnd::FAILED;
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
