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
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
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
    bool money_readable = true;
    bool rows_readable = true;
    bool quantity_readable = true;
    //  Rows that read as another item.
    std::map<size_t, std::string> misread = {};
    //  Each purchase takes this much more than its price: a bug to catch.
    size_t overcharge = 0;
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
    virtual std::optional<size_t> read_money() override{
        if (!in_list){
            error = "read the money off the list";
        }
        return money_readable ? std::optional<size_t>(money) : std::nullopt;
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
        money -= quantity * price + overcharge;
        bag[std::string(stock[m_cursor].item)] += quantity;
        return PurchaseEnd::BOUGHT;
    }
    virtual bool leave() override{
        in_list = false;
        return true;
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


MartPlan pewter_plan(size_t budget, std::vector<MartLine> lines, std::optional<size_t> floor = std::nullopt){
    return {.map = "PewterCity_Mart", .lines = std::move(lines), .budget = budget, .money_floor = floor};
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
            .name = "priority order within the budget, a line cut to what fits",
            .plan = pewter_plan(2000, {{"potion", 5}, {"repel", 3}, {"antidote", 3}}),
            .money = 3000,
            .bag = {{"potion", 5}, {"repel", 1}, {"antidote", 1}},
            .money_after = 1050,
        },
        {
            .name = "the money, below the budget, limits it",
            .plan = pewter_plan(2000, {{"potion", 5}}),
            .money = 700,
            .bag = {{"potion", 2}},
            .money_after = 100,
        },
        {
            .name = "a line that can't buy one is skipped",
            .plan = pewter_plan(400, {{"escape-rope", 1}, {"antidote", 2}}),
            .money = 3000,
            .bag = {{"antidote", 2}},
            .money_after = 2800,
        },
        {
            .name = "twenty-five by RIGHT and UP",
            .plan = pewter_plan(10000, {{"poke-ball", 25}}),
            .money = 6000,
            .bag = {{"poke-ball", 25}},
            .money_after = 1000,
        },
    };
}

std::vector<MartCase> mart_cases_2(){
    return {
        {
            .name = "unread money: the table's floor",
            .plan = pewter_plan(2000, {{"potion", 5}}, 1000),
            .money = 1200,
            .setup = [](FakeMart& mart){ mart.money_readable = false; },
            .bag = {{"potion", 3}},
            .money_after = 300,
        },
        {
            .name = "unread money and no floor: nothing bought",
            .plan = pewter_plan(2000, {{"potion", 5}}),
            .money = 3000,
            .setup = [](FakeMart& mart){ mart.money_readable = false; },
            .end = MartEnd::NO_MONEY_READ,
            .money_after = 3000,
        },
        {
            .name = "a floor above the money: the quantity box wraps, and its read catches it",
            .plan = pewter_plan(2000, {{"potion", 3}}, 1000),
            .money = 500,
            .setup = [](FakeMart& mart){ mart.money_readable = false; },
            .end = MartEnd::FAILED,
            .money_after = 500,
        },
        {
            .name = "too poor for any of it",
            .plan = pewter_plan(2000, {{"potion", 5}}),
            .money = 250,
            .end = MartEnd::NOTHING_TO_BUY,
            .money_after = 250,
        },
    };
}

std::vector<MartCase> mart_cases_3(){
    return {
        {
            .name = "a row that reads as another item stops it",
            .plan = pewter_plan(2000, {{"potion", 1}, {"antidote", 1}}),
            .money = 3000,
            .setup = [](FakeMart& mart){ mart.misread[2] = "parlyz-heal"; },
            .end = MartEnd::FAILED,
            .bag = {{"potion", 1}},
            .money_after = 2700,
            .dumps = 1,
        },
        {
            .name = "rows that can't be read are bought by position",
            .plan = pewter_plan(2000, {{"repel", 2}, {"potion", 1}}),
            .money = 3000,
            .setup = [](FakeMart& mart){ mart.rows_readable = false; },
            .bag = {{"repel", 2}, {"potion", 1}},
            .money_after = 2000,
        },
        {
            .name = "more money gone than bought fails",
            .plan = pewter_plan(2000, {{"potion", 1}}),
            .money = 3000,
            .setup = [](FakeMart& mart){ mart.overcharge = 10; },
            .end = MartEnd::FAILED,
            .bag = {{"potion", 1}},
            .money_after = 2690,
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
            .plan = {.map = "CeladonCity_DepartmentStore_5F", .lines = lines, .budget = 15600},
            .money = 19862,
            .bag = {{"x-special", 21}, {"x-speed", 6}, {"x-accuracy", 5}, {"guard-spec", 2}},
            .money_after = 4262,
        },
    };
}

std::vector<MartCase> mart_cases(){
    std::vector<MartCase> cases;
    for (std::vector<MartCase> part : {mart_cases_1(), mart_cases_2(), mart_cases_3(), mart_cases_4()}){
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
    const MartResult result = run_mart(logger, mart, item.plan);
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
    const MartPlan plan = pewter_plan(2000, {{"potion", 5}, {"repel", 3}}, 2400);
    const Pokemon::StoryActionArgs args = mart_args(plan);
    const std::optional<MartPlan> back = mart_plan_of(args);
    if (!back || back->map != plan.map || back->budget != 2000 || back->money_floor != plan.money_floor
        || back->lines.size() != 2 || back->lines[1].item != "repel" || back->lines[1].quantity != 3
    ){
        return "The Pewter plan didn't read back from its args.";
    }
    const std::vector<std::pair<std::string, Pokemon::StoryArgValue>> bad_args{
        {"items", std::vector<Pokemon::StoryItem>{}},
        {"map", std::string("Nowhere_Mart")},
        {"map", std::string("ViridianCity_Mart")},
        {"items", std::vector<Pokemon::StoryItem>{{"potion", 0}}},
        {"items", std::vector<Pokemon::StoryItem>{{"potion", 100}}},
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
    for (const char* map : {
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

//  ConsoleMartActions::read_money() reads nothing yet, so a Mart step with no
//  "floor" buys nothing (MartEnd::NO_MONEY_READ): every one carries a floor.
UnitTestResult test_mart_steps_have_floor(){
    std::string missing;
    for (const StoryCheckpoint& data : story_checkpoints()){
        for (const StoryStep& step : data.steps){
            if (step.kind == Pokemon::StoryStepKind::GAME && step.action == MART_ACTION && !step.args.contains("floor")){
                missing += " " + data.id + "/" + step.name;
            }
        }
    }
    if (!missing.empty()){
        return "Mart steps with no floor:" + missing;
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
        .input = {"every Mart step has a floor", test_mart_steps_have_floor},
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
