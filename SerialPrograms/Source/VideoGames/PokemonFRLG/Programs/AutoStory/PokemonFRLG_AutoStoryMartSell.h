/*  AutoStory Mart Sell
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a MART step sells at the counter before it buys: what the clerk
 *  pays, and the SELL game step, injected so the step's decisions run in
 *  tests.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMartSell_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMartSell_H

#include <cstddef>
#include <optional>
#include <string_view>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What the clerk pays for one `item` (half its pokefirered items.json
//  price): the items the story sells (the NUGGET); nothing for another,
//  or for one priced 0, which the clerk won't buy ("OH, no. I can't buy
//  that.", as the MOON STONE).
std::optional<size_t> sale_price(std::string_view item);

//  How selling one line ended.
enum class SaleEnd{
    //  "Turned over the <item> and received $N.": back facing the clerk.
    SOLD,
    //  The pocket has none of the item: back facing the clerk.
    NONE_IN_BAG,
    FAILED,
};

//  The SELL game step a Mart step takes for each of its sales.
class MartSellActions{
public:
    virtual ~MartSellActions() = default;

    //  Facing the clerk: talk, SELL, `sale.quantity` of `sale.item` from
    //  its pocket, YES, out of the bag and the menu, facing the clerk again.
    virtual SaleEnd sell(const MartLine& sale) = 0;
};

//  The console's SELL: the bag on its Items pocket, read with
//  read_open_pocket(), the quantity typed as a purchase's.
class ConsoleMartSellActions : public MartSellActions{
public:
    ConsoleMartSellActions(ConsoleHandle& console, ProControllerContext& context, Language language);

    virtual SaleEnd sell(const MartLine& sale) override;

private:
    //  From the bag: out to the overworld facing the clerk; `end`, or
    //  FAILED if the overworld didn't come back.
    SaleEnd back_to_clerk(SaleEnd end);

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Language m_language;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
