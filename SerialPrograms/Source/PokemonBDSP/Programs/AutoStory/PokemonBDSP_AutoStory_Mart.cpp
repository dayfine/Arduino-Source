/*  BDSP AutoStory - Poke Mart
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_SelectionArrow.h"
#include "PokemonBDSP/Inference/PokemonBDSP_ShopDetectors.h"
#include "PokemonBDSP_AutoStory_Mart.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

const char* shop_item_name(ShopItem item){
    switch (item){
    case ShopItem::FullRestore:
        return "Full restore";
    case ShopItem::MaxPotion:
        return "Max potion";
    case ShopItem::HyperPotion:
        return "Hyper potion";
    case ShopItem::SuperPotion:
        return "Super potion";
    case ShopItem::Potion:
        return "Potion";
    case ShopItem::Antidote:
        return "Antidote";
    case ShopItem::ParalyzHeal:
        return "Paralyz heal";
    case ShopItem::Awakening:
        return "Awakening";
    case ShopItem::Repel:
        return "Repel";
    case ShopItem::SuperRepel:
        return "Super repel";
    case ShopItem::MaxRepel:
        return "Max repel";
    case ShopItem::EscapeRope:
        return "Escape rope";
    default:
        return "Unknown";
    }
}

// Returns the OCR slug used as the dictionary key in ShopItemNameOCR.json.
const char* shop_item_slug(ShopItem item){
    switch (item){
    case ShopItem::FullRestore:
        return "full-restore";
    case ShopItem::MaxPotion:
        return "max-potion";
    case ShopItem::HyperPotion:
        return "hyper-potion";
    case ShopItem::SuperPotion:
        return "super-potion";
    case ShopItem::Potion:
        return "potion";
    case ShopItem::Antidote:
        return "antidote";
    case ShopItem::ParalyzHeal:
        return "paralyze-heal";
    case ShopItem::Awakening:
        return "awakening";
    case ShopItem::Repel:
        return "repel";
    case ShopItem::SuperRepel:
        return "super-repel";
    case ShopItem::MaxRepel:
        return "max-repel";
    case ShopItem::EscapeRope:
        return "escape-rope";
    default:
        return "unknown";
    }
}

// {MaxRepel, 2, 4}, {FullRestore, 0, 3} → "4 Max repel, 3 Full restore"
std::string make_buy_label(const std::vector<ItemToBuy>& items){
    std::string label;
    for (size_t i = 0; i < items.size(); i++){
        if (i > 0){
            label += ", ";
        }
        if (items[i].quantity == BUY_MAX){
            label += "max ";
        }else{
            label += std::to_string(items[i].quantity) + " ";
        }
        label += shop_item_name(items[i].item);
    }
    return label;
}

}

bool buy_items(VideoStream& stream,
    ProControllerContext& context,
    const std::vector<ItemToBuy>& items,
    Language language){
    if (items.empty()){
        return true;
    }

    const std::string label = make_buy_label(items);
    context.wait_for_all_requests();
    pbf_wait(context, 500ms);
    pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
    pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
    context.wait_for_all_requests();


    // ── Coordinate constants ─────────────────────────────────────────────────

    // Area where the selection arrow appears at the vendor greeting menu (Buy/Sell/Leave).
    static const ImageFloatBox VENDOR_ARROW_BOX{0.630000, 0.550000, 0.070000, 0.120000};

    // Area where the selection arrow appears at the YES/NO confirmation dialog.
    static const ImageFloatBox CONFIRM_ARROW_BOX{0.700000, 0.620000, 0.070000, 0.120000};

    // Full area containing all item rows in the shop list.
    // The hover detector scans this region to dynamically locate the red outline.
    static const ImageFloatBox ITEM_SCAN_AREA{0.417000, 0.100000, 0.540000, 0.625000};

    static constexpr size_t MAX_OUTER = 50;
    static constexpr size_t MAX_ITEM_SEARCH = 50; // max iterations per item before giving up

    // ── Outer state machine ──────────────────────────────────────────────────
    for (size_t outer = 0; outer < MAX_OUTER; outer++){
        SelectionArrowFinder arrow_vendor(stream.overlay(), VENDOR_ARROW_BOX, COLOR_GREEN);
        ShopBuyingWindowWatcher buying_window;
        ShortDialogWatcher dialog(COLOR_RED);

        // case 0: no detectors fired — press A in a for-loop (5 presses) while watching.
        stream.log("Starting to loop over items");
        int state = run_until<ProControllerContext>(stream,
            context,
            [](ProControllerContext& ctx){
                for (int i = 0; i < 5; i++){
                    pbf_press_button(ctx, BUTTON_A, 80ms, 1000ms);
                }
            },
            {{arrow_vendor}, {buying_window}, {dialog}});

        context.wait_for_all_requests();
        pbf_wait(context, 500ms);

        switch (state){

        case 0: // case 1: SelectionArrow at vendor menu → press A to choose "Buy"
            stream.log("Found the selection arrow, pressing A to enter the buying window...", COLOR_GREEN);
            pbf_press_button(context, BUTTON_A, 80ms, 500ms);
            break;

        case 1: // case 2: buying window open → buy each requested item
            stream.log("Found the buying window, searching for the right item...", COLOR_GREEN);
            {
                bool buying_failed = false;

                for (const ItemToBuy& item : items){
                    stream.log("Buying " + std::to_string(item.quantity) + "x " + shop_item_slug(item.item));
                    if (buying_failed){
                        break;
                    }

                    // Detector fires only when the red outline is found AND both
                    // name and price OCR match the expected item slug.
                    ShopItemHoverDetector hover(stream.logger(),
                        ITEM_SCAN_AREA,
                        shop_item_slug(item.item),
                        language,
                        COLOR_CYAN);
                    ShopQuantityWindowWatcher qty_window;
                    SelectionArrowFinder confirm_arrow(stream.overlay(), CONFIRM_ARROW_BOX, COLOR_GREEN);

                    bool item_bought = false;
                    bool hover_confirmed = false;
                    for (size_t inner = 0; inner < MAX_ITEM_SEARCH && !item_bought; inner++){
                        context.wait_for_all_requests();

                        int sub = run_until<ProControllerContext>(stream,
                            context,
                            [](ProControllerContext& ctx){
                                ctx.wait_for(1000ms);
                            },
                            {{confirm_arrow}, {qty_window}, {hover}});

                        context.wait_for_all_requests();

                        stream.log("buy_items: sub=" + std::to_string(sub), COLOR_WHITE);

                        switch (sub){

                        case 0: // YES/NO confirmation arrow → confirm purchase
                            stream.log("Found the confirmation to buy arrow, pressing A to buy", COLOR_GREEN);
                            {
                                context.wait_for_all_requests();
                                ShopBuyingWindowWatcher back_in_shop;
                                run_until<ProControllerContext>(stream,
                                    context,
                                    [](ProControllerContext& ctx){
                                        for (int i = 0; i < 10; i++){
                                            pbf_press_button(ctx, BUTTON_A, 80ms, 800ms);
                                        }
                                    },
                                    {{back_in_shop}});
                                item_bought = true;
                                break;
                            }

                        case 1: // quantity window
                            if (!hover_confirmed){
                                // Quantity window appeared before we confirmed the right item —
                                // an earlier A press accidentally opened the wrong item's selector.
                                // Press B to go back to the list and continue searching.
                                stream.log("Unexpected quantity window (wrong item), pressing B...", COLOR_YELLOW);
                                pbf_press_button(context, BUTTON_B, 80ms, 800ms);
                                break;
                            }
                            stream.log("Found the quantity window", COLOR_GREEN);
                            if (item.quantity == BUY_MAX){
                                stream.log("Buying max quantity");
                                pbf_press_dpad(context, DPAD_DOWN, 80ms, 300ms);
                            }else{
                                stream.log("Buying " + std::to_string(item.quantity) + " of the item");
                                for (int q = 1; q < item.quantity; q++){
                                    pbf_press_dpad(context, DPAD_UP, 80ms, 300ms);
                                }
                            }
                            pbf_press_button(context, BUTTON_A, 80ms, 300ms);
                            pbf_wait(context, 1000ms);
                            break;

                        case 2: // correct item highlighted → press A to open quantity selector
                            stream.log("Found the right item, pressing A to buy a certain quantity...", COLOR_GREEN);
                            hover_confirmed = true;
                            pbf_press_button(context, BUTTON_A, 80ms, 300ms);
                            pbf_wait(context, 1000ms);
                            break;

                        default: // outline not found or wrong item → advance to next row
                            stream.log("Not on the right item, moving...", COLOR_GREEN);
                            pbf_press_dpad(context, DPAD_DOWN, 80ms, 300ms);
                            pbf_wait(context, 300ms);
                            break;
                        }
                    }

                    if (!item_bought){
                        stream.log(label + ": buy_items: could not locate " + shop_item_name(item.item) +
                            " in shop list.",
                            COLOR_RED);
                        buying_failed = true;
                    }
                }

                if (!buying_failed){
                    stream.log("All items bought, exiting shop...", COLOR_GREEN);
                    pbf_mash_button(context, BUTTON_B, 5000ms);
                    return true;
                }
                break;
            }

        case 2: // case 3: unexpected ShortDialog → press B until back in buying window
        {
            ShopBuyingWindowWatcher back_in_shop;
            run_until<ProControllerContext>(stream,
                context,
                [](ProControllerContext& ctx){
                    pbf_mash_button(ctx, BUTTON_B, 5000ms);
                },
                {{back_in_shop}});
            break;
        }

        default: // outer default: unrecognised state → press B to recover
            pbf_press_button(context, BUTTON_B, 80ms, 3000ms);
            break;
        }
    }

    stream.log(label + ": buy_items: failed to complete after max iterations.", COLOR_RED);
    return false;
}



}
}
}
