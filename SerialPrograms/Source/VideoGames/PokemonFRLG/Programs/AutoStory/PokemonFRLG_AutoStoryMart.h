/*  AutoStory Mart
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story step at any Poke Mart: a buy list in priority order, within a
 *  budget, as pokefirered's shop.c runs the BUY menu:
 *
 *  - Talking to the clerk: BUY, SELL, SEE YA!, on BUY. A on BUY opens the
 *    Mart's list (its stock in the map's scripts.inc order, then CANCEL),
 *    the cursor on the top row, and the money box.
 *  - A on an item the player can't pay one of: "You don't have enough
 *    money." and back to the list. Otherwise "How many?" with the
 *    quantity box on 1. Its most is the money over the price, at most 99.
 *    UP adds one and wraps from the most to 1; DOWN takes one and wraps
 *    from 1 to the most; RIGHT adds ten and LEFT takes ten, both stopping
 *    at the ends (menu_helpers.c AdjustQuantityAccordingToDPadInput).
 *  - A on the quantity: "... And you wanted N? That will be $X." YES/NO on
 *    YES; YES: "Here you are! Thank you!", the money taken, back to the
 *    list with the cursor on the row bought.
 *
 *  The step never opens SELL. Each line buys as many as its quantity,
 *  the budget left and the money left allow, in the order listed; a line
 *  that can't buy one is skipped. The money is read from the money box
 *  when a reader exists; without one the step counts on the table's
 *  floor (the least the ledger says the player has) and, with neither,
 *  buys nothing: a quantity typed past the most wraps, and buys the wrong
 *  number.
 *
 *  The decisions take their game steps from a MartActions, so they run in
 *  tests against a fake Mart; ConsoleMartActions does them on the console
 *  with the item list reader, whose names cover the items a new game meets
 *  before the Boulder Badge (a row it can't read is bought by position).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMart_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMart_H

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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  One item a Mart sells: its slug (as read_item_list_name() names it,
//  e.g. "parlyz-heal") and its price (pokefirered items.json).
struct MartStockItem{
    std::string_view item;
    size_t price;
};

//  The BUY list of the Mart on map `map` (e.g. "PewterCity_Mart"), in row
//  order, CANCEL not included; empty for a map whose stock isn't known.
const std::vector<MartStockItem>& mart_stock(std::string_view map);

//  One line of a buy list: an item and how many.
struct MartLine{
    std::string item = {};
    size_t quantity = 1;
};

//  What a Mart step buys.
struct MartPlan{
    //  The Mart's map, for its stock.
    std::string map = {};
    //  In priority order.
    std::vector<MartLine> lines = {};
    //  The most the step spends.
    size_t budget = 0;
    //  The least money the player has here, from the money plan: used
    //  when the money box can't be read.
    std::optional<size_t> money_floor = std::nullopt;
};

//  The GAME action of a Mart step: map WORD (required), the Mart's map;
//  budget INTEGER (required); floor INTEGER (optional), the money floor;
//  items ITEMS (required), the lines in order.
inline const std::string MART_ACTION = "mart";
//  `plan` as its action's args.
Pokemon::StoryActionArgs mart_args(const MartPlan& plan);
//  The plan a MART_ACTION step's args give; nothing if it has no line or
//  names an item its Mart doesn't sell.
std::optional<MartPlan> mart_plan_of(const Pokemon::StoryActionArgs& args);

//  The table's Mart step, once per save (StoryStep::once_per_save): a
//  retry from the same save skips it when it had nothing to buy.
Pokemon::StoryStep mart_step(std::string name, std::string why, const MartPlan& plan);


//  One purchase the step makes.
struct MartBuy{
    //  The list's row, from 0.
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
    //  On the list: the cursor from row `from` to row `to`.
    virtual void move_cursor(size_t from, size_t to) = 0;
    //  On the list: row `row`'s item slug; empty if it can't be read.
    virtual std::string read_row(size_t row) = 0;
    //  On the list, the cursor on `buy`'s row: A, its quantity typed as
    //  quantity_presses() says, A, YES. The quantity box is read before
    //  the A, if it can be: another number backs out.
    virtual PurchaseEnd buy(const MartBuy& buy) = 0;
    //  From the list: CANCEL, SEE YA!, to the overworld. False if it didn't
    //  come back.
    virtual bool leave() = 0;
};

//  How a Mart shopping trip concluded.
enum class MartEnd{
    DONE,
    //  The plan bought nothing: the money (or its floor) doesn't cover one
    //  of anything. The Mart isn't touched.
    NOTHING_TO_BUY,
    //  Neither the money box nor the table's floor gave the money.
    NO_MONEY_READ,
    FAILED,
};
//  The human-readable name of a Mart outcome.
const char* mart_end_name(MartEnd end);

//  The summary result of a Mart shopping session.
struct MartResult{
    MartEnd end = MartEnd::FAILED;
    std::vector<MartBuy> bought = {};
    size_t spent = 0;
    //  FAILED: what failed.
    std::string detail = {};
};

//  Facing the clerk: buys as `plan` says, then back to the overworld.
MartResult run_mart(Logger& logger, MartActions& actions, const MartPlan& plan);

//  Formats a log message summarizing the Mart shopping result.
std::string mart_message(const MartPlan& plan, const MartResult& result);


//  Counts `quantity` of `item` into `supplies`, if it is one the battles
//  use (Potions, Antidotes, Parlyz Heals, Awakenings); a new one goes to the end of the
//  pocket, where the bag adds it.
void count_supplies(Supplies& supplies, std::string_view item, size_t quantity);


//  The console's Mart. The money box and the quantity box have no reader
//  yet, so the money is the table's floor and the quantity is typed by
//  presses. Potions, Antidotes, Parlyz Heals and Awakenings bought are
//  counted into `supplies`.
class ConsoleMartActions : public MartActions{
public:
    ConsoleMartActions(ConsoleHandle& console, ProControllerContext& context, Language language, Supplies& supplies);

    virtual bool open_buy_list() override;
    virtual std::optional<size_t> read_money() override;
    virtual void move_cursor(size_t from, size_t to) override;
    virtual std::string read_row(size_t row) override;
    virtual PurchaseEnd buy(const MartBuy& buy) override;
    virtual bool leave() override;

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Language m_language;
    Supplies& m_supplies;
    size_t m_row = 0;
    //  The lowest row the cursor has reached: past the window's rows the
    //  list scrolls, and a screen row is no longer the list's.
    size_t m_lowest_row = 0;
};


//  Register auto-story Mart unit tests.
void add_tests_AutoStoryMart(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
