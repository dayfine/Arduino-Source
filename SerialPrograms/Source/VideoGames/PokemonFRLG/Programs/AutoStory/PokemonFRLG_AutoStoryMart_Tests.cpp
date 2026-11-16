/*  AutoStory Mart Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Mart step against a fake Mart that runs the BUY menu as
 *  pokefirered's shop.c does (see PokemonFRLG_AutoStoryMart.h): the list
 *  keeps its cursor on the row bought, the quantity box's most is the
 *  money over the price and UP wraps from it to 1, and an item the player
 *  can't pay one of is refused.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "PokemonFRLG_AutoStoryCaptureStep.h"
#include "PokemonFRLG_AutoStoryGrindStep.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

class FakeMart : public MartActions{
public:
    FakeMart(std::string_view map, size_t money)
        : stock(mart_stock(map))
        , money(money)
    {}

    std::vector<MartStockItem> stock;
    size_t money;
    bool rows_readable = true;
    bool quantity_readable = true;
    //  The money box: unread by default, as before it had a reader.
    bool money_readable = false;
    //  A sale says SOLD but the clerk pays nothing.
    bool sales_unpaid = false;
    //  Rows that read as another item.
    std::map<size_t, std::string> misread = {};
    std::map<std::string, size_t> bag = {};
    size_t list_opened = 0;
    size_t dumps = 0;
    bool in_list = false;
    std::string error = {};

    virtual bool open_buy_list() override{
        in_list = true;
        m_cursor = 0;
        m_scroll = 0;
        m_row = 0;
        list_opened++;
        return true;
    }
    virtual void press_cursor(bool down) override{
        press(down);
    }
    //  `row` is a row of the window, as the console reads the screen.
    virtual std::string read_row(size_t row) override{
        if (row != m_row){
            error = std::format("read window row {}, the cursor on window row {}", row, m_row);
        }
        const size_t index = m_scroll + row;
        const auto found = misread.find(index);
        if (found != misread.end()){
            return found->second;
        }
        return rows_readable && index < stock.size() ? std::string(stock[index].item) : "";
    }
    virtual void dump_screen(std::string_view) override{
        dumps++;
    }
    virtual PurchaseEnd buy(const MartBuy& buy) override{
        if (m_cursor >= stock.size()){
            error = "A on CANCEL";
            in_list = false;
            return PurchaseEnd::FAILED;
        }
        const size_t price = stock[m_cursor].price;
        if (price > money){
            return PurchaseEnd::NOT_ENOUGH_MONEY;
        }
        const size_t quantity = typed_quantity(buy.quantity, std::min<size_t>(99, money / price));
        if (quantity_readable && quantity != buy.quantity){
            return PurchaseEnd::WRONG_QUANTITY;
        }
        money -= quantity * price;
        bag[std::string(stock[m_cursor].item)] += quantity;
        return PurchaseEnd::BOUGHT;
    }
    virtual bool leave() override{
        in_list = false;
        return true;
    }
    virtual std::optional<size_t> read_money() override{
        if (!in_list){
            error = "read the money off the list";
        }
        return money_readable ? std::optional<size_t>(money) : std::nullopt;
    }

private:
    //  One UP or DOWN on the list (the stock, then CANCEL), as pokefirered's
    //  list_menu.c ListMenuUpdateSelectedRowIndexAndScrollOffset moves it,
    //  six rows shown (shop.c BuyMenuInitListMenu): going down the cursor
    //  stops on the window's fourth row and the list scrolls under it until
    //  its end shows; going up it stops on the third until its top shows.
    void press(bool down){
        const size_t total = stock.size() + 1;
        const size_t shown = std::min<size_t>(total, 6);
        if (down){
            if (m_scroll + m_row + 1 >= total){
                return;
            }
            if (m_row < shown / 2 + shown % 2 || m_scroll == total - shown){
                m_row++;
            }else{
                m_scroll++;
            }
        }else{
            if (m_scroll + m_row == 0){
                return;
            }
            if (m_row > shown - (shown / 2 + shown % 2) - 1 || m_scroll == 0){
                m_row--;
            }else{
                m_scroll--;
            }
        }
        m_cursor = m_scroll + m_row;
    }

    //  The box after quantity_presses(wanted), from 1, with this most.
    static size_t typed_quantity(size_t wanted, size_t most){
        const QuantityPresses presses = quantity_presses(wanted);
        size_t quantity = 1;
        for (size_t c = 0; c < presses.right; c++){
            quantity = std::min(quantity + 10, most);
        }
        for (size_t c = 0; c < presses.up; c++){
            quantity = quantity + 1 > most ? 1 : quantity + 1;
        }
        return quantity;
    }

    size_t m_cursor = 0;
    //  The window: the list's row on its top, and the cursor's row in it.
    size_t m_scroll = 0;
    size_t m_row = 0;
};

//  The same Mart's SELL: the clerk buys from its `bag` at sale_price(),
//  from facing the clerk.
class FakeMartSell : public MartSellActions{
public:
    explicit FakeMartSell(FakeMart& mart)
        : m_mart(mart)
    {}

    virtual SaleEnd sell(const MartLine& sale) override{
        const auto held = m_mart.bag.find(sale.item);
        const std::optional<size_t> price = sale_price(sale.item);
        if (m_mart.in_list || !price){
            m_mart.error = m_mart.in_list ? "SELL from the BUY list" : "sold an item the clerk won't buy";
            return SaleEnd::FAILED;
        }
        if (held == m_mart.bag.end()){
            return SaleEnd::NONE_IN_BAG;
        }
        const size_t quantity = std::min(sale.quantity, held->second);
        held->second -= quantity;
        if (held->second == 0){
            m_mart.bag.erase(held);
        }
        m_mart.money += m_mart.sales_unpaid ? 0 : quantity * *price;
        return SaleEnd::SOLD;
    }

private:
    FakeMart& m_mart;
};


struct AutoStoryMartInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryMartExpectation{
};

struct AutoStoryMartCase{
    AutoStoryMartInput input;
    AutoStoryMartExpectation expectation;
};

class Test_AutoStoryMart : public UnitTest{
public:
    explicit Test_AutoStoryMart(AutoStoryMartCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryMart - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryMartCase m_case;
};


MartPlan pewter_plan(std::vector<MartLine> lines){
    return {.map = "PewterCity_Mart", .lines = std::move(lines)};
}

std::string bag_string(const std::map<std::string, size_t>& bag){
    std::string text;
    for (const auto& item : bag){
        text += std::format("{}{} {}", text.empty() ? "" : ", ", item.second, item.first);
    }
    return text.empty() ? "nothing" : text;
}

struct MartCase{
    std::string name = {};
    MartPlan plan = {};
    size_t money = 0;
    std::function<void(FakeMart&)> setup = {};
    MartEnd end = MartEnd::DONE;
    std::map<std::string, size_t> bag = {};
    size_t money_after = 0;
    //  Screens saved for a row read as another item.
    size_t dumps = 0;
};

std::vector<MartCase> mart_cases_1(){
    return {
        {
            .name = "every line its listed quantity, in order",
            .plan = pewter_plan({{"potion", 5}, {"repel", 3}, {"antidote", 3}}),
            .money = 3000,
            .bag = {{"potion", 5}, {"repel", 3}, {"antidote", 3}},
            .money_after = 150,
        },
        {
            .name = "a line the money can't pay one of fails the step, the rest not bought",
            .plan = pewter_plan({{"potion", 1}, {"escape-rope", 1}, {"antidote", 2}}),
            .money = 800,
            .end = MartEnd::FAILED,
            .bag = {{"potion", 1}},
            .money_after = 500,
        },
        {
            .name = "twenty-five by RIGHT and UP",
            .plan = pewter_plan({{"poke-ball", 25}}),
            .money = 6000,
            .bag = {{"poke-ball", 25}},
            .money_after = 1000,
        },
    };
}

std::vector<MartCase> mart_cases_2(){
    return {
        {
            .name = "more than the money allows: the quantity box wraps, and its read catches it",
            .plan = pewter_plan({{"potion", 3}}),
            .money = 500,
            .end = MartEnd::FAILED,
            .money_after = 500,
        },
        {
            .name = "too poor for any of it: the dialog fails the step",
            .plan = pewter_plan({{"potion", 5}}),
            .money = 250,
            .end = MartEnd::FAILED,
            .money_after = 250,
        },
    };
}

std::vector<MartCase> mart_cases_3(){
    return {
        {
            .name = "a row that reads as another item stops it",
            .plan = pewter_plan({{"potion", 1}, {"antidote", 1}}),
            .money = 3000,
            .setup = [](FakeMart& mart){ mart.misread[2] = "parlyz-heal"; },
            .end = MartEnd::FAILED,
            .bag = {{"potion", 1}},
            .money_after = 2700,
            .dumps = 1,
        },
        {
            .name = "rows that can't be read are bought by position",
            .plan = pewter_plan({{"repel", 2}, {"potion", 1}}),
            .money = 3000,
            .setup = [](FakeMart& mart){ mart.rows_readable = false; },
            .bag = {{"repel", 2}, {"potion", 1}},
            .money_after = 2000,
        },
    };
}

//  The X items clerk's list (seven, then CANCEL) scrolls under the cursor:
//  after X SPECIAL and X SPEED, X ACCURACY sits on the window's fourth row,
//  not its fifth, and the fifth was read as GUARD SPEC. (console B,
//  2026-11-12, 10.01).
std::vector<MartCase> mart_cases_4(){
    const std::vector<MartLine> lines{{"x-special", 21}, {"x-speed", 6}, {"x-accuracy", 5}, {"guard-spec", 2}};
    return {
        {
            .name = "a list scrolled under the cursor",
            .plan = {.map = "CeladonCity_DepartmentStore_5F", .lines = lines},
            .money = 19862,
            .bag = {{"x-special", 21}, {"x-speed", 6}, {"x-accuracy", 5}, {"guard-spec", 2}},
            .money_after = 4262,
        },
    };
}

//  20.01a's Full Heals after the Full Restores, only if the money left
//  covers them (user, 2026-11-15): a short purse skips the line, the step
//  done, where without if_affordable it fails.
std::vector<MartCase> mart_cases_5(){
    const std::vector<MartLine> two_full_heals{{"full-heal", 2}};
    const std::vector<MartLine> restore_then_heal{{"full-restore", 1}, {"full-heal", 1}};
    const MartPlan full_heals{.map = "IndigoPlateau_PokemonCenter_1F", .lines = two_full_heals, .if_affordable = true};
    const MartPlan restore_first{.map = "IndigoPlateau_PokemonCenter_1F", .lines = restore_then_heal, .if_affordable = true};
    return {
        {
            .name = "if_affordable: both bought when the money covers them",
            .plan = full_heals,
            .money = 1500,
            .bag = {{"full-heal", 2}},
            .money_after = 300,
        },
        {
            .name = "if_affordable: money for one, the quantity box wraps: skipped, done",
            .plan = full_heals,
            .money = 900,
            .money_after = 900,
        },
        {
            .name = "if_affordable: money for none: skipped, done",
            .plan = full_heals,
            .money = 400,
            .money_after = 400,
        },
        {
            .name = "if_affordable: a line skipped, the cheaper next one still bought",
            .plan = restore_first,
            .money = 700,
            .bag = {{"full-heal", 1}},
            .money_after = 100,
        },
    };
}

//  19.01a sells the Nugget ($5,000, half its 10,000) and buys Super
//  Potions with it at Lavender's Mart: the money box, read on the BUY
//  list, checks the sale and cuts the if_affordable line to what it
//  covers; unread, the line is bought blind.
std::vector<MartCase> mart_cases_6(){
    const auto nugget = [](FakeMart& mart){ mart.bag["nugget"] = 1; };
    const auto nugget_read = [](FakeMart& mart){ mart.bag["nugget"] = 1; mart.money_readable = true; };
    const auto unpaid = [](FakeMart& mart){ mart.bag["nugget"] = 1; mart.money_readable = true; mart.sales_unpaid = true; };
    const auto read_only = [](FakeMart& mart){ mart.money_readable = true; };
    MartPlan six{.map = "LavenderTown_Mart", .lines = {{"super-potion", 6}}, .if_affordable = true, .sells = {{"nugget", 1}}};
    MartPlan nine = six;
    nine.lines[0].quantity = 9;
    return {
        {
            .name = "sell the Nugget, then 6 Super Potions from its $5000",
            .plan = six, .money = 0, .setup = nugget_read,
            .bag = {{"super-potion", 6}}, .money_after = 800,
        },
        {
            .name = "the money unread: the 6 bought blind",
            .plan = six, .money = 0, .setup = nugget,
            .bag = {{"super-potion", 6}}, .money_after = 800,
        },
        {
            .name = "9 wanted, the money read covers 7",
            .plan = nine, .money = 0, .setup = nugget_read,
            .bag = {{"super-potion", 7}}, .money_after = 100,
        },
        {
            .name = "no Nugget in the bag: the sale skipped, the money read covers 1",
            .plan = six, .money = 1000, .setup = read_only,
            .bag = {{"super-potion", 1}}, .money_after = 300,
        },
        {
            .name = "a sale the money doesn't show fails before buying",
            .plan = six, .money = 0, .setup = unpaid,
            .end = MartEnd::FAILED, .bag = {}, .money_after = 0,
        },
    };
}

std::vector<MartCase> mart_cases(){
    std::vector<MartCase> cases;
    for (std::vector<MartCase> part : {mart_cases_1(), mart_cases_2(), mart_cases_3(), mart_cases_4(), mart_cases_5(), mart_cases_6()}){
        cases.insert(cases.end(), part.begin(), part.end());
    }
    return cases;
}

UnitTestResult check_mart_case(const MartCase& item){
    QuietLogger logger;
    FakeMart mart(item.plan.map, item.money);
    if (item.setup){
        item.setup(mart);
    }
    FakeMartSell sell(mart);
    const MartResult result = run_mart(logger, mart, sell, item.plan);
    if (!mart.error.empty()){
        return item.name + ": " + mart.error;
    }
    if (result.end != item.end){
        return std::format("{}: ended {}, expected {} ({}).", item.name,
            mart_end_name(result.end), mart_end_name(item.end), result.detail);
    }
    if (mart.bag != item.bag || mart.money != item.money_after || mart.dumps != item.dumps){
        return std::format("{}: bag {}, ${}, {} screens saved; expected {}, ${}, {}.", item.name,
            bag_string(mart.bag), mart.money, mart.dumps, bag_string(item.bag), item.money_after, item.dumps);
    }
    if (mart.in_list){
        return item.name + ": left in the Mart's list.";
    }
    return true;
}

UnitTestResult test_mart_buys(){
    for (const MartCase& item : mart_cases()){
        const UnitTestResult result = check_mart_case(item);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    return true;
}


//  The presses for each quantity reach it from 1 in a box whose most is
//  just that quantity, so RIGHT never stops at the most early.
UnitTestResult test_quantity_presses(){
    for (size_t quantity = 1; quantity <= 99; quantity++){
        const QuantityPresses presses = quantity_presses(quantity);
        const size_t reached = 1 + presses.right * 10 + presses.up;
        if (reached != quantity || presses.up > 9){
            return std::format("{}: {} RIGHT and {} UP reach {}.", quantity, presses.right, presses.up, reached);
        }
    }
    return true;
}


UnitTestResult test_mart_args(){
    const MartPlan plan = pewter_plan({{"potion", 5}, {"repel", 3}});
    const Pokemon::StoryActionArgs args = mart_args(plan);
    const std::optional<MartPlan> back = mart_plan_of(args);
    if (!back || back->map != plan.map
        || back->lines.size() != 2 || back->lines[1].item != "repel" || back->lines[1].quantity != 3
    ){
        return "The Pewter plan didn't read back from its args.";
    }
    MartPlan if_affordable = plan;
    if_affordable.if_affordable = true;
    const std::optional<MartPlan> affordable_back = mart_plan_of(mart_args(if_affordable));
    if (!affordable_back || !affordable_back->if_affordable || back->if_affordable || args.contains("if_affordable")){
        return "if_affordable didn't read back from its args, or was written when false.";
    }
    MartPlan selling = plan;
    selling.sells = {{"nugget", 1}};
    const std::optional<MartPlan> selling_back = mart_plan_of(mart_args(selling));
    if (!selling_back || selling_back->sells.size() != 1 || selling_back->sells[0].item != "nugget" || args.contains("sell")){
        return "The sales didn't read back from their args, or were written when none.";
    }
    const std::vector<std::pair<std::string, Pokemon::StoryArgValue>> bad_args{
        {"items", std::vector<Pokemon::StoryItem>{}},
        {"map", std::string("Nowhere_Mart")},
        {"map", std::string("ViridianCity_Mart")},
        {"items", std::vector<Pokemon::StoryItem>{{"potion", 0}}},
        {"items", std::vector<Pokemon::StoryItem>{{"potion", 100}}},
        {"sell", std::vector<Pokemon::StoryItem>{{"moon-stone", 1}}},
        {"sell", std::vector<Pokemon::StoryItem>{{"nugget", 0}}},
    };
    for (const auto& [key, value] : bad_args){
        Pokemon::StoryActionArgs bad = args;
        bad[key] = value;
        if (mart_plan_of(bad)){
            return "Expected a bad " + key + " refused.";
        }
    }
    return true;
}


//  Each Mart's stock is its scripts.inc list, with known prices.
UnitTestResult test_mart_stock(){
    const std::vector<MartStockItem>& viridian = mart_stock("ViridianCity_Mart");
    if (viridian.size() != 4 || viridian[3].item != "parlyz-heal" || viridian[1].price != 300){
        return "Viridian's stock isn't POKE BALL, POTION (300), ANTIDOTE, PARLYZ HEAL.";
    }
    const std::vector<MartStockItem>& cerulean = mart_stock("CeruleanCity_Mart");
    if (cerulean.size() != 9 || cerulean[1].item != "super-potion" || cerulean[2].item != "potion"){
        return "Cerulean sells SUPER POTION above POTION.";
    }
    if (!mart_stock("Route4_PokemonCenter_1F").empty()){
        return "Expected no stock for a map without a Mart.";
    }
    return true;
}


//  12.01's Fuchsia Mart read its ULTRA BALL row as PARLYZ HEAL and bought
//  nothing (console B, 2026-11-12): the item list reader had no name for it.
//  Every item any Mart sells has one.
UnitTestResult test_mart_stock_readable(){
    std::string missing;
    for (const std::string_view map : {
        "ViridianCity_Mart", "PewterCity_Mart", "CeruleanCity_Mart", "VermilionCity_Mart",
        "LavenderTown_Mart", "FuchsiaCity_Mart", "SaffronCity_Mart", "CinnabarIsland_Mart",
        "CeladonCity_DepartmentStore_5F",
    }){
        for (const MartStockItem& stock : mart_stock(map)){
            if (!item_list_reader_knows(stock.item)){
                missing += std::format("\n    {}: {}", map, stock.item);
            }
        }
    }
    if (!missing.empty()){
        return std::format("The item list reader has no name for:{}", missing);
    }
    return true;
}


//  Potions, Antidotes, Parlyz Heals and Awakenings bought go into the
//  battles' supplies.
UnitTestResult test_count_supplies(){
    Supplies supplies;
    supplies.pocket = {"potion"};
    supplies.potions = 1;
    count_supplies(supplies, "antidote", 3);
    count_supplies(supplies, "potion", 2);
    count_supplies(supplies, "repel", 4);
    count_supplies(supplies, "poke-ball", 5);
    count_supplies(supplies, "awakening", 3);
    const std::vector<std::string> pocket{"potion", "antidote", "awakening"};
    if (supplies.potions != 3 || supplies.antidotes != 3 || supplies.awakenings != 3 || supplies.pocket != pocket){
        return std::format("Supplies {} potions, {} antidotes, {} awakenings, {} pocket rows.",
            supplies.potions, supplies.antidotes, supplies.awakenings, supplies.pocket.size());
    }
    return true;
}

//  A Mart step buys blind: the money can't be read, so a Mart that can't
//  be paid fails the step. That is cheap only early in a checkpoint: no
//  battle (a BATTLE, TRAIN, GRIND or CAPTURE step) and at most
//  MAX_WALK_BEFORE_MART ROUTE or LEGS steps since the checkpoint's start or
//  its last Mart step.
constexpr size_t MAX_WALK_BEFORE_MART = 4;

//  Checkpoints whose Mart step is late, known: not reordered yet. A
//  checkpoint listed here that no longer is late must come off the list.
const std::set<std::string_view> LATE_MART_CHECKPOINTS = {"07.01", "12.01"};

//  Whether a step is a battle that can cost the money (a prize or a loss).
bool fights(const StoryStep& step){
    return step.kind == Pokemon::StoryStepKind::BATTLE
        || step.kind == Pokemon::StoryStepKind::TRAIN
        || (step.kind == Pokemon::StoryStepKind::GAME && (step.action == GRIND_ACTION || step.action == CAPTURE_ACTION));
}

//  Whether `data` has a Mart step after a battle or a long walk.
bool has_late_mart(const StoryCheckpoint& data){
    size_t walk = 0;
    bool fought = false;
    for (const StoryStep& step : data.steps){
        if (step.kind == Pokemon::StoryStepKind::GAME && step.action == MART_ACTION){
            if (fought || walk > MAX_WALK_BEFORE_MART){
                return true;
            }
            walk = 0;
            fought = false;
        }else if (step.kind == Pokemon::StoryStepKind::ROUTE || step.kind == Pokemon::StoryStepKind::LEGS){
            walk++;
        }else if (fights(step)){
            fought = true;
        }
    }
    return false;
}

UnitTestResult test_mart_steps_early(){
    std::string late;
    std::string stale;
    for (const StoryCheckpoint& data : story_checkpoints()){
        const bool listed = LATE_MART_CHECKPOINTS.contains(data.id);
        if (has_late_mart(data) && !listed){
            late += " " + data.id;
        }else if (!has_late_mart(data) && listed){
            stale += " " + data.id;
        }
    }
    if (!late.empty()){
        return "Mart steps after a battle or a long walk:" + late;
    }
    if (!stale.empty()){
        return "Checkpoints listed as late, no longer:" + stale;
    }
    return true;
}

//  One Mart step of the shopping the user approved on 2026-11-15, with
//  the story row it must come before and the money it is run against.
struct ApprovedShopping{
    std::string_view checkpoint;
    std::string_view step;
    std::string_view before;
    MartPlan plan;
    size_t money;
    std::map<std::string, size_t> bag;
    size_t money_after;
    //  In the bag before the step, for its sales.
    std::map<std::string, size_t> held = {};
};

//  A plan of one line: `quantity` of `item` at `map`'s Mart.
MartPlan one_line(std::string map, std::string item, size_t quantity){
    return {.map = std::move(map), .lines = {MartLine{std::move(item), quantity}}};
}

const std::vector<ApprovedShopping>& approved_shopping(){
    MartPlan status_heals = one_line("ViridianCity_Mart", "antidote", 1);
    status_heals.lines.push_back({"parlyz-heal", 2});
    MartPlan full_heals = one_line("IndigoPlateau_PokemonCenter_1F", "full-heal", 2);
    full_heals.if_affordable = true;
    MartPlan nugget_for_potions = one_line("LavenderTown_Mart", "super-potion", 6);
    nugget_for_potions.if_affordable = true;
    nugget_for_potions.sells = {{"nugget", 1}};
    static const std::vector<ApprovedShopping> shopping{
        {
            .checkpoint = "12.03", .step = "buy_super_potions_for_silph", .before = "16.01",
            .plan = one_line("FuchsiaCity_Mart", "super-potion", 4),
            .money = 2800, .bag = {{"super-potion", 4}}, .money_after = 0,
        },
        {
            .checkpoint = "18.03", .step = "buy_x_accuracies", .before = "19.01",
            .plan = one_line("CeladonCity_DepartmentStore_5F", "x-accuracy", 2),
            .money = 1900, .bag = {{"x-accuracy", 2}}, .money_after = 0,
        },
        {
            //  The Nugget pays for all 6 with no money before.
            .checkpoint = "19.01a", .step = "sell_nugget_buy_super_potions", .before = "19.01",
            .plan = nugget_for_potions,
            .money = 0, .bag = {{"super-potion", 6}}, .money_after = 800, .held = {{"nugget", 1}},
        },
        {
            .checkpoint = "19.01a", .step = "buy_status_heals", .before = "19.01",
            .plan = status_heals,
            .money = 500, .bag = {{"antidote", 1}, {"parlyz-heal", 2}}, .money_after = 0,
        },
        {
            //  After 13 Full Restores, money for one Full Heal: skipped.
            .checkpoint = "20.01a", .step = "buy_full_heals", .before = "20.01",
            .plan = full_heals,
            .money = 1100, .bag = {}, .money_after = 1100,
        },
    };
    return shopping;
}

bool same_plan(const MartPlan& a, const MartPlan& b){
    if (a.map != b.map || a.if_affordable != b.if_affordable || a.lines.size() != b.lines.size() || a.sells.size() != b.sells.size()){
        return false;
    }
    for (size_t c = 0; c < a.sells.size(); c++){
        if (a.sells[c].item != b.sells[c].item || a.sells[c].quantity != b.sells[c].quantity){
            return false;
        }
    }
    for (size_t c = 0; c < a.lines.size(); c++){
        if (a.lines[c].item != b.lines[c].item || a.lines[c].quantity != b.lines[c].quantity){
            return false;
        }
    }
    return true;
}

//  Each approved Mart step is in its row, before the row it shops for,
//  with the approved lines; on a fake Mart with the money given it ends
//  done with that bag.
UnitTestResult test_approved_shopping(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    for (const ApprovedShopping& shopping : approved_shopping()){
        std::optional<size_t> row_index;
        std::optional<size_t> before_index;
        const StoryStep* found = nullptr;
        for (size_t c = 0; c < rows.size(); c++){
            if (rows[c].id == shopping.before){
                before_index = c;
            }
            if (rows[c].id != shopping.checkpoint){
                continue;
            }
            row_index = c;
            for (const StoryStep& step : rows[c].steps){
                found = step.name == shopping.step ? &step : found;
            }
        }
        if (found == nullptr || !row_index || !before_index || *row_index >= *before_index){
            return std::format("{}/{}: missing, or not before {}.", shopping.checkpoint, shopping.step, shopping.before);
        }
        const std::optional<MartPlan> plan = mart_plan_of(found->args);
        if (!plan || !same_plan(*plan, shopping.plan)){
            return std::format("{}/{}: not the approved lines.", shopping.checkpoint, shopping.step);
        }
        const UnitTestResult result = check_mart_case({
            .name = std::format("{}/{}", shopping.checkpoint, shopping.step),
            .plan = *plan,
            .money = shopping.money,
            .setup = [&](FakeMart& mart){ mart.bag.insert(shopping.held.begin(), shopping.held.end()); },
            .bag = shopping.bag,
            .money_after = shopping.money_after,
        });
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    return true;
}

} // namespace


const AutoStoryMartCase AUTO_STORY_MART_CASES[] = {
    {
        .input = {"buy lists on a fake Mart", test_mart_buys},
        .expectation = {},
    },
    {
        .input = {"quantity presses", test_quantity_presses},
        .expectation = {},
    },
    {
        .input = {"the step's args", test_mart_args},
        .expectation = {},
    },
    {
        .input = {"Mart stocks", test_mart_stock},
        .expectation = {},
    },
    {
        .input = {"every Mart item has a reader name", test_mart_stock_readable},
        .expectation = {},
    },
    {
        .input = {"bought supplies counted", test_count_supplies},
        .expectation = {},
    },
    {
        .input = {"every Mart step is early in its checkpoint", test_mart_steps_early},
        .expectation = {},
    },
    {
        .input = {"the shopping approved on 2026-11-15", test_approved_shopping},
        .expectation = {},
    },
};

void add_tests_AutoStoryMart(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_MART_CASES){
        database.add<Test_AutoStoryMart>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
