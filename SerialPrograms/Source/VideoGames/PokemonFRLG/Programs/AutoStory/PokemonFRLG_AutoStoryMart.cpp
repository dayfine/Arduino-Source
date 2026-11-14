/*  AutoStory Mart
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryMart.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

//  The quantity box's most (shop.c Task_BuyHowManyDialogueInit).
constexpr size_t MAX_QUANTITY = 99;
//  The rows the BUY list shows at once (shop.c BuyMenuInitListMenu).
constexpr size_t LIST_ROWS_SHOWN = 6;

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
constexpr MartStockItem X_ATTACK{"x-attack", 500};
constexpr MartStockItem X_DEFEND{"x-defend", 550};
constexpr MartStockItem X_SPEED{"x-speed", 350};
constexpr MartStockItem X_SPECIAL{"x-special", 350};
constexpr MartStockItem X_ACCURACY{"x-accuracy", 950};
constexpr MartStockItem GUARD_SPEC{"guard-spec", 700};
constexpr MartStockItem DIRE_HIT{"dire-hit", 650};

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
        //  The X items clerk (CeladonCity_DepartmentStore_5F_XItems).
        {"CeladonCity_DepartmentStore_5F", {X_ATTACK, X_DEFEND, X_SPEED, X_SPECIAL, X_ACCURACY, GUARD_SPEC, DIRE_HIT}},
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

//  The item on row `index` of a Mart's list: its stock, then CANCEL.
std::string_view list_item(const std::vector<MartStockItem>& stock, size_t index){
    return index < stock.size() ? stock[index].item : "cancel";
}

//  The BUY list's window over its rows, moved one UP or DOWN at a time as
//  pokefirered's list_menu.c ListMenuUpdateSelectedRowIndexAndScrollOffset
//  moves it (see PokemonFRLG_AutoStoryMart.h).
class ListWindow{
public:
    explicit ListWindow(size_t rows)
        : m_rows(rows)
        , m_shown(std::min(rows, LIST_ROWS_SHOWN))
    {}

    //  The cursor's row in the list.
    size_t index() const{
        return m_top + m_row;
    }
    //  The cursor's row in the window.
    size_t row() const{
        return m_row;
    }

    void move(bool down){
        if (down){
            move_down();
        }else{
            move_up();
        }
    }

private:
    void move_down(){
        if (index() + 1 >= m_rows){
            return;
        }
        if (m_row < m_shown / 2 + m_shown % 2 || m_top == m_rows - m_shown){
            m_row++;
        }else{
            m_top++;
        }
    }
    void move_up(){
        if (index() == 0){
            return;
        }
        if (m_top == 0 || m_row + 1 > m_shown - (m_shown / 2 + m_shown % 2)){
            m_row--;
        }else{
            m_top--;
        }
    }

    size_t m_rows;
    size_t m_shown;
    //  The list's row on the window's top.
    size_t m_top = 0;
    size_t m_row = 0;
};

} // namespace


const std::vector<MartStockItem>& mart_stock(std::string_view map){
    static const std::vector<MartStockItem> NONE;
    const auto found = mart_stocks().find(map);
    return found == mart_stocks().end() ? NONE : found->second;
}

std::string_view mart_end_name(MartEnd end){
    switch (end){
    case MartEnd::DONE:             return "done";
    case MartEnd::FAILED:           return "failed";
    }
    return "?";
}


Pokemon::StoryActionArgs mart_args(const MartPlan& plan){
    std::vector<Pokemon::StoryItem> items;
    for (const MartLine& line : plan.lines){
        items.push_back({line.item, line.quantity});
    }
    return {{"map", plan.map}, {"items", std::move(items)}};
}

std::optional<MartPlan> mart_plan_of(const Pokemon::StoryActionArgs& args){
    const std::string* map = game_arg<std::string>(args, "map");
    const std::vector<Pokemon::StoryItem>* items = game_arg<std::vector<Pokemon::StoryItem>>(args, "items");
    if (map == nullptr || mart_stock(*map).empty() || items == nullptr || items->empty()){
        return std::nullopt;
    }
    MartPlan plan{.map = *map};
    for (const Pokemon::StoryItem& item : *items){
        if (item.quantity < 1 || item.quantity > MAX_QUANTITY || !stock_row(mart_stock(plan.map), item.item)){
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
    step.action = MART_ACTION;
    step.args = mart_args(plan);
    step.once_per_save = true;
    return step;
}


std::optional<std::vector<MartBuy>> plan_mart_buys(const std::vector<MartStockItem>& stock, const MartPlan& plan){
    std::vector<MartBuy> buys;
    for (const MartLine& line : plan.lines){
        const std::optional<size_t> row = stock_row(stock, line.item);
        if (!row){
            return std::nullopt;
        }
        const size_t price = stock[*row].price;
        const size_t quantity = std::min(line.quantity, MAX_QUANTITY);
        buys.push_back({.row = *row, .item = line.item, .quantity = quantity, .cost = quantity * price});
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
        , m_stock(mart_stock(plan.map))
        , m_window(m_stock.size() + 1)
    {}

    MartResult run(){
        if (!m_actions.open_buy_list()){
            return end(MartEnd::FAILED, "the BUY list didn't show");
        }
        const std::optional<std::vector<MartBuy>> buys = plan_mart_buys(m_stock, m_plan);
        if (!buys){
            return leave(MartEnd::FAILED, "an item the Mart doesn't sell");
        }
        for (const MartBuy& buy : *buys){
            if (!buy_one(buy)){
                return leave(MartEnd::FAILED, m_result.detail);
            }
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

    //  Moves the cursor to `buy`'s row one press at a time, reading the
    //  cursor's row after each: a name that isn't the row's stops it, the
    //  screen saved. A row that can't be read is passed, or bought, by its
    //  position.
    bool reach(const MartBuy& buy){
        for (size_t presses = 0; presses <= m_stock.size(); presses++){
            const size_t index = m_window.index();
            const std::string item = m_actions.read_row(m_window.row());
            if (!item.empty() && item != list_item(m_stock, index)){
                m_result.detail = std::format("row {} is {}, not {}", index + 1, item, list_item(m_stock, index));
                m_actions.dump_screen(std::format("row{}-{}-not-{}", index + 1, item, list_item(m_stock, index)));
                return false;
            }
            if (index == buy.row){
                if (item.empty()){
                    m_logger.log(std::format("Mart: couldn't read row {}; buying it by position.", index + 1), COLOR_ORANGE);
                }
                return true;
            }
            const bool down = index < buy.row;
            m_actions.press_cursor(down);
            m_window.move(down);
        }
        m_result.detail = std::format("the cursor didn't reach row {}", buy.row + 1);
        return false;
    }

    bool buy_one(const MartBuy& buy){
        if (!reach(buy)){
            return false;
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

    Logger& m_logger;
    MartActions& m_actions;
    const MartPlan& m_plan;
    const std::vector<MartStockItem>& m_stock;
    //  Where the cursor is, from the list's top row as open_buy_list()
    //  leaves it; a purchase leaves it on the row bought.
    ListWindow m_window;
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
    return true;
}

void ConsoleMartActions::press_cursor(bool down){
    pbf_press_dpad(m_context, down ? DPAD_DOWN : DPAD_UP, 160ms, 300ms);
    m_context.wait_for_all_requests();
}

std::string ConsoleMartActions::read_row(size_t row){
    if (row >= (size_t)ITEM_LIST_ROWS){
        return "";
    }
    VideoSnapshot screen = m_console.video().snapshot();
    if (!screen){
        return "";
    }
    return read_item_list_name(m_console.logger(), m_language, screen, (int)row);
}

void ConsoleMartActions::dump_screen(std::string_view name){
    VideoSnapshot screen = m_console.video().snapshot();
    if (screen){
        dump_debug_image(m_console, "AutoStory/mart", std::string(name), screen);
    }
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
    }else if (item == "super-potion"){
        count = &Supplies::super_potions;
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
