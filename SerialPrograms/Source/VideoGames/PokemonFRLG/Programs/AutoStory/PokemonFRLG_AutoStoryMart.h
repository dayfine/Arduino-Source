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
#include "PokemonFRLG_AutoStoryMartBuys.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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
std::string_view mart_end_name(MartEnd end);

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
