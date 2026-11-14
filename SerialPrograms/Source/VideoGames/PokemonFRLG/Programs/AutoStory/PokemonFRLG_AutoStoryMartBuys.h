/*  AutoStory Mart Buys
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a MART step buys, the presses that set a quantity, and the game
 *  steps it takes, injected so its decisions run in tests.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMartBuys_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMartBuys_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  One purchase the step makes.
struct MartBuy{
    //  The list's row, from 0 at the list's top.
    size_t row = 0;
    std::string item = {};
    size_t quantity = 0;
    size_t cost = 0;
};

//  What `plan` buys at a Mart with `stock` and `money`: each line as many
//  as fit in the budget and the money left, in order, at most 99; lines
//  that buy none are left out. Nothing if a line's item isn't stocked.
std::optional<std::vector<MartBuy>> plan_mart_buys(
    const std::vector<MartStockItem>& stock, const MartPlan& plan, size_t money
);

//  The presses that turn the quantity box from 1 into `quantity` without
//  passing its most: RIGHT adds ten, then UP one at a time.
struct QuantityPresses{
    size_t right = 0;
    size_t up = 0;
};
//  Calculates D-pad presses needed to select a purchase quantity.
QuantityPresses quantity_presses(size_t quantity);


//  The outcome of attempting to buy an item at the Mart.
enum class PurchaseEnd{
    //  "Here you are!": bought, back at the list.
    BOUGHT,
    //  "You don't have enough money.": back at the list.
    NOT_ENOUGH_MONEY,
    //  The quantity box showed another number: backed out with B.
    WRONG_QUANTITY,
    FAILED,
};

//  The game steps a Mart step takes, injected so its decisions run in
//  tests.
class MartActions{
public:
    virtual ~MartActions() = default;

    //  Facing the clerk: talk, BUY, to the list with the cursor on its top
    //  row. False if the list didn't show.
    virtual bool open_buy_list() = 0;
    //  On the list: the money box; nothing if it can't be read.
    virtual std::optional<size_t> read_money() = 0;
    //  On the list: one DOWN (`down`) or UP.
    virtual void press_cursor(bool down) = 0;
    //  On the list: the item slug on row `row` of its window, from the
    //  window's top (not the list's: the list scrolls); empty if it can't
    //  be read.
    virtual std::string read_row(size_t row) = 0;
    //  On the list: saves the screen to look at, as `name`.
    virtual void dump_screen(std::string_view name) = 0;
    //  On the list, the cursor on `buy`'s row: A, its quantity typed as
    //  quantity_presses() says, A, YES. The quantity box is read before
    //  the A, if it can be: another number backs out.
    virtual PurchaseEnd buy(const MartBuy& buy) = 0;
    //  From the list: CANCEL, SEE YA!, to the overworld. False if it didn't
    //  come back.
    virtual bool leave() = 0;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
