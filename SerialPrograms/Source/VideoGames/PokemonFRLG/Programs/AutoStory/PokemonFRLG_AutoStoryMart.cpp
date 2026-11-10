/*  AutoStory Mart
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryVariantWords.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

//  The quantity box's most (shop.c Task_BuyHowManyDialogueInit).
constexpr size_t MAX_QUANTITY = 99;

//  Prices from pokefirered src/data/items.json.
constexpr MartStockItem POKE_BALL{"poke-ball", 200};
constexpr MartStockItem GREAT_BALL{"great-ball", 600};
constexpr MartStockItem ULTRA_BALL{"ultra-ball", 1200};
constexpr MartStockItem POTION{"potion", 300};
constexpr MartStockItem SUPER_POTION{"super-potion", 700};
constexpr MartStockItem HYPER_POTION{"hyper-potion", 1200};
constexpr MartStockItem MAX_POTION{"max-potion", 2500};
constexpr MartStockItem FULL_RESTORE{"full-restore", 3000};
constexpr MartStockItem REVIVE{"revive", 1500};
constexpr MartStockItem ANTIDOTE{"antidote", 100};
constexpr MartStockItem PARLYZ_HEAL{"parlyz-heal", 200};
constexpr MartStockItem AWAKENING{"awakening", 250};
constexpr MartStockItem BURN_HEAL{"burn-heal", 250};
constexpr MartStockItem ICE_HEAL{"ice-heal", 250};
constexpr MartStockItem FULL_HEAL{"full-heal", 600};
constexpr MartStockItem ESCAPE_ROPE{"escape-rope", 550};
constexpr MartStockItem REPEL{"repel", 350};
constexpr MartStockItem SUPER_REPEL{"super-repel", 500};
constexpr MartStockItem MAX_REPEL{"max-repel", 700};

//  Each Mart's `<map>_Items` list in pokefirered data/maps/<map>/scripts.inc.
const std::map<std::string, std::vector<MartStockItem>, std::less<>>& mart_stocks(){
    static const std::map<std::string, std::vector<MartStockItem>, std::less<>> stocks = {
        {"ViridianCity_Mart", {POKE_BALL, POTION, ANTIDOTE, PARLYZ_HEAL}},
        {"PewterCity_Mart", {POKE_BALL, POTION, ANTIDOTE, PARLYZ_HEAL, AWAKENING, BURN_HEAL, ESCAPE_ROPE, REPEL}},
        {"CeruleanCity_Mart", {
            POKE_BALL, SUPER_POTION, POTION, ANTIDOTE, PARLYZ_HEAL, AWAKENING, BURN_HEAL, ESCAPE_ROPE, REPEL,
        }},
        {"VermilionCity_Mart", {POKE_BALL, SUPER_POTION, ANTIDOTE, PARLYZ_HEAL, AWAKENING, ICE_HEAL, REPEL}},
        {"LavenderTown_Mart", {
            GREAT_BALL, SUPER_POTION, REVIVE, ANTIDOTE, PARLYZ_HEAL, BURN_HEAL, ICE_HEAL, ESCAPE_ROPE, SUPER_REPEL,
        }},
        {"FuchsiaCity_Mart", {ULTRA_BALL, GREAT_BALL, SUPER_POTION, REVIVE, FULL_HEAL, MAX_REPEL}},
        {"SaffronCity_Mart", {GREAT_BALL, HYPER_POTION, REVIVE, FULL_HEAL, ESCAPE_ROPE, MAX_REPEL}},
        {"CinnabarIsland_Mart", {ULTRA_BALL, GREAT_BALL, HYPER_POTION, REVIVE, FULL_HEAL, ESCAPE_ROPE, MAX_REPEL}},
        {"IndigoPlateau_PokemonCenter_1F", {
            ULTRA_BALL, GREAT_BALL, FULL_RESTORE, MAX_POTION, REVIVE, FULL_HEAL, MAX_REPEL,
        }},
    };
    return stocks;
}

std::optional<size_t> stock_row(const std::vector<MartStockItem>& stock, std::string_view item){
    for (size_t row = 0; row < stock.size(); row++){
        if (stock[row].item == item){
            return row;
        }
    }
    return std::nullopt;
}

//  The most money a budget or floor names: six digits.
constexpr int64_t MAX_MONEY = 999999;

} // namespace


const std::vector<MartStockItem>& mart_stock(std::string_view map){
    static const std::vector<MartStockItem> NONE;
    const auto found = mart_stocks().find(map);
    return found == mart_stocks().end() ? NONE : found->second;
}

const char* mart_end_name(MartEnd end){
    switch (end){
    case MartEnd::DONE:             return "done";
    case MartEnd::NOTHING_TO_BUY:   return "nothing_to_buy";
    case MartEnd::NO_MONEY_READ:    return "no_money_read";
    case MartEnd::FAILED:           return "failed";
    }
    return "?";
}


Pokemon::StoryArgs mart_args(const MartPlan& plan){
    Pokemon::StoryArgs args{
        {"map", plan.map},
        {"budget", static_cast<int64_t>(plan.budget)},
    };
    if (plan.money_floor){
        args["floor"] = static_cast<int64_t>(*plan.money_floor);
    }
    return args;
}

std::optional<MartPlan> parse_mart_step(const Pokemon::StoryStep& step){
    const Pokemon::StoryArgs* args = game_step_args(step, MART_VARIANT, MART_ARGS);
    if (args == nullptr || !arg_number_within(*args, "budget", 0, MAX_MONEY) || !arg_number_within(*args, "floor", 0, MAX_MONEY)){
        return std::nullopt;
    }
    MartPlan plan;
    plan.map = std::string(*Pokemon::story_arg_text(*args, "map"));
    const std::vector<MartStockItem>& stock = mart_stock(plan.map);
    if (stock.empty() || step.items.empty()){
        return std::nullopt;
    }
    plan.budget = static_cast<size_t>(*Pokemon::story_arg_number(*args, "budget"));
    if (const std::optional<int64_t> floor = Pokemon::story_arg_number(*args, "floor")){
        plan.money_floor = static_cast<size_t>(*floor);
    }
    for (const Pokemon::StoryItem& item : step.items){
        if (item.quantity < 1 || item.quantity > MAX_QUANTITY || !stock_row(stock, item.item)){
            return std::nullopt;
        }
        plan.lines.push_back({item.item, item.quantity});
    }
    return plan;
}

Pokemon::StoryStep mart_step(std::string name, std::string why, const MartPlan& plan){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = MART_VARIANT;
    step.args = mart_args(plan);
    for (const MartLine& line : plan.lines){
        step.items.push_back({line.item, line.quantity});
    }
    step.once_per_save = true;
    return step;
}


std::optional<std::vector<MartBuy>> plan_mart_buys(
    const std::vector<MartStockItem>& stock, const MartPlan& plan, size_t money
){
    std::vector<MartBuy> buys;
    size_t left = std::min(plan.budget, money);
    for (const MartLine& line : plan.lines){
        const std::optional<size_t> row = stock_row(stock, line.item);
        if (!row){
            return std::nullopt;
        }
        const size_t price = stock[*row].price;
        const size_t quantity = std::min({line.quantity, left / price, MAX_QUANTITY});
        if (quantity == 0){
            continue;
        }
        buys.push_back({.row = *row, .item = line.item, .quantity = quantity, .cost = quantity * price});
        left -= quantity * price;
    }
    return buys;
}

QuantityPresses quantity_presses(size_t quantity){
    if (quantity <= 1){
        return {};
    }
    //  RIGHT from 1 reaches 11, 21, ...: never past `quantity`, so never
    //  stopped at the most.
    const size_t right = (quantity - 1) / 10;
    return {.right = right, .up = quantity - 1 - right * 10};
}


namespace{

//  One Mart step under way.
class MartRun{
public:
    MartRun(Logger& logger, MartActions& actions, const MartPlan& plan)
        : m_logger(logger)
        , m_actions(actions)
        , m_plan(plan)
    {}

    MartResult run(){
        if (!m_actions.open_buy_list()){
            return end(MartEnd::FAILED, "the BUY list didn't show");
        }
        const std::optional<size_t> read = m_actions.read_money();
        const std::optional<size_t> money = read ? read : m_plan.money_floor;
        if (!money){
            return leave(MartEnd::NO_MONEY_READ, "");
        }
        m_logger.log(std::format("Mart: ${} ({}), budget ${}.", *money, read ? "read" : "the table's floor", m_plan.budget));
        const std::optional<std::vector<MartBuy>> buys = plan_mart_buys(mart_stock(m_plan.map), m_plan, *money);
        if (!buys){
            return leave(MartEnd::FAILED, "an item the Mart doesn't sell");
        }
        if (buys->empty()){
            return leave(MartEnd::NOTHING_TO_BUY, "");
        }
        for (const MartBuy& buy : *buys){
            if (!buy_one(buy)){
                return leave(MartEnd::FAILED, m_result.detail);
            }
        }
        if (!money_checks(read)){
            return leave(MartEnd::FAILED, m_result.detail);
        }
        return leave(MartEnd::DONE, "");
    }

private:
    MartResult end(MartEnd end, std::string detail){
        m_result.end = end;
        m_result.detail = std::move(detail);
        return m_result;
    }
    //  Out to the overworld, then ends as `end`, unless that fails.
    MartResult leave(MartEnd end, std::string detail){
        if (!m_actions.leave()){
            return this->end(MartEnd::FAILED, "the overworld didn't come back after the Mart");
        }
        return this->end(end, std::move(detail));
    }

    bool buy_one(const MartBuy& buy){
        m_actions.move_cursor(m_cursor, buy.row);
        m_cursor = buy.row;
        const std::string item = m_actions.read_row(buy.row);
        if (!item.empty() && item != buy.item){
            m_result.detail = std::format("row {} is {}, not {}", buy.row + 1, item, buy.item);
            return false;
        }
        if (item.empty()){
            m_logger.log(std::format("Mart: couldn't read row {}; buying it by position.", buy.row + 1), COLOR_ORANGE);
        }
        switch (m_actions.buy(buy)){
        case PurchaseEnd::BOUGHT:
            m_logger.log(std::format("Mart: bought {} {} for ${}.", buy.quantity, buy.item, buy.cost));
            m_result.bought.push_back(buy);
            m_result.spent += buy.cost;
            return true;
        case PurchaseEnd::NOT_ENOUGH_MONEY:
            m_result.detail = std::format("not enough money for {} {}", buy.quantity, buy.item);
            return false;
        case PurchaseEnd::WRONG_QUANTITY:
            m_result.detail = std::format("the quantity box didn't show {} for {}", buy.quantity, buy.item);
            return false;
        case PurchaseEnd::FAILED:
            break;
        }
        m_result.detail = std::format("buying {} {} didn't come back to the list", buy.quantity, buy.item);
        return false;
    }

    //  With the money read before, it is read again: the difference must
    //  be what was spent.
    bool money_checks(const std::optional<size_t>& before){
        if (!before){
            return true;
        }
        const std::optional<size_t> after = m_actions.read_money();
        if (!after || *before - *after == m_result.spent){
            return true;
        }
        m_result.detail = std::format("${} before and ${} after, but ${} bought", *before, *after, m_result.spent);
        return false;
    }

    Logger& m_logger;
    MartActions& m_actions;
    const MartPlan& m_plan;
    size_t m_cursor = 0;
    MartResult m_result;
};

} // namespace

MartResult run_mart(Logger& logger, MartActions& actions, const MartPlan& plan){
    return MartRun(logger, actions, plan).run();
}

std::string mart_message(const MartPlan& plan, const MartResult& result){
    std::string bought;
    for (const MartBuy& buy : result.bought){
        bought += std::format("{}{} {}", bought.empty() ? "" : ", ", buy.quantity, buy.item);
    }
    if (bought.empty()){
        bought = "nothing";
    }
    switch (result.end){
    case MartEnd::DONE:
        return std::format("Mart ({}): bought {} for ${}.", plan.map, bought, result.spent);
    case MartEnd::NOTHING_TO_BUY:
        return std::format("Mart ({}): the money or budget covers none of the list; bought nothing.", plan.map);
    case MartEnd::NO_MONEY_READ:
        return std::format(
            "Mart ({}): the money box can't be read and the table gives no floor; bought nothing.", plan.map
        );
    case MartEnd::FAILED:
        return std::format("Mart ({}): {}; bought {} for ${}.", plan.map, result.detail, bought, result.spent);
    }
    return "Mart: ?";
}


ConsoleMartActions::ConsoleMartActions(
    ConsoleHandle& console, ProControllerContext& context, Language language, Supplies& supplies
)
    : m_console(console)
    , m_context(context)
    , m_language(language)
    , m_supplies(supplies)
{}

bool ConsoleMartActions::open_buy_list(){
    //  "May I help you?" with BUY on top, then the list with the cursor on
    //  its top row, as buy_items() found them.
    pbf_press_button(m_context, BUTTON_A, 160ms, 2000ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 2000ms);
    m_context.wait_for_all_requests();
    m_row = 0;
    m_lowest_row = 0;
    return true;
}

std::optional<size_t> ConsoleMartActions::read_money(){
    //  No reader for the money box yet.
    return std::nullopt;
}

void ConsoleMartActions::move_cursor(size_t from, size_t to){
    for (size_t c = from; c < to; c++){
        pbf_press_dpad(m_context, DPAD_DOWN, 160ms, 300ms);
    }
    for (size_t c = to; c < from; c++){
        pbf_press_dpad(m_context, DPAD_UP, 160ms, 300ms);
    }
    m_context.wait_for_all_requests();
    m_row = to;
    m_lowest_row = std::max(m_lowest_row, to);
}

std::string ConsoleMartActions::read_row(size_t row){
    //  Rows past the window's last but one may have scrolled the list.
    if (m_lowest_row + 1 >= (size_t)ITEM_LIST_ROWS){
        return "";
    }
    VideoSnapshot screen = m_console.video().snapshot();
    if (!screen){
        return "";
    }
    return read_item_list_name(m_console.logger(), m_language, screen, (int)row);
}

PurchaseEnd ConsoleMartActions::buy(const MartBuy& buy){
    //  "How many?" drops presses while its text prints.
    pbf_press_button(m_context, BUTTON_A, 160ms, 2500ms);
    const QuantityPresses presses = quantity_presses(buy.quantity);
    for (size_t c = 0; c < presses.right; c++){
        pbf_press_dpad(m_context, DPAD_RIGHT, 160ms, 340ms);
    }
    for (size_t c = 0; c < presses.up; c++){
        pbf_press_dpad(m_context, DPAD_UP, 160ms, 340ms);
    }
    //  "That will be..." YES/NO, YES, "Here you are!"; back to the list.
    pbf_press_button(m_context, BUTTON_A, 160ms, 3500ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 2000ms);
    pbf_press_button(m_context, BUTTON_A, 160ms, 1500ms);
    m_context.wait_for_all_requests();
    count_supplies(m_supplies, buy.item, buy.quantity);
    return PurchaseEnd::BOUGHT;
}

bool ConsoleMartActions::leave(){
    pbf_press_button(m_context, BUTTON_B, 160ms, 1500ms);
    pbf_press_button(m_context, BUTTON_B, 160ms, 1500ms);
    try{
        mash_until_overworld(m_console, m_context, 20000ms);
        return true;
    }catch (OperationFailedException& e){
        m_console.log("Mart: " + e.message(), COLOR_RED);
        return false;
    }
}


void count_supplies(Supplies& supplies, std::string_view item, size_t quantity){
    int Supplies::* count = nullptr;
    if (item == "potion"){
        count = &Supplies::potions;
    }else if (item == "antidote"){
        count = &Supplies::antidotes;
    }else if (item == "parlyz-heal"){
        count = &Supplies::parlyz_heals;
    }else if (item == "awakening"){
        count = &Supplies::awakenings;
    }
    if (count == nullptr){
        return;
    }
    if (std::find(supplies.pocket.begin(), supplies.pocket.end(), item) == supplies.pocket.end()){
        supplies.pocket.emplace_back(item);
    }
    supplies.*count += (int)quantity;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
