/*  AutoStory Pocket List
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonFRLG_AutoStoryPocketList.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using namespace std::chrono_literals;


//  The Items pocket's capacity (pokefirered BAG_ITEMS_COUNT).
constexpr size_t MAX_POCKET_ROWS = 42;

namespace{

bool read_initial_pocket_page(PocketList& list, std::vector<BagRow>& reads, PocketRead& read){
    for (int shown = 0; shown < ITEM_LIST_ROWS; shown++){
        std::optional<BagRow> row = list.read_shown(shown);
        if (row && row->item == "cancel"){
            read.saw_cancel = true;
        }
        if (!row || row->item == "cancel"){
            return true;
        }
        reads.push_back(std::move(*row));
    }
    return false;
}

} // namespace

PocketRead read_whole_pocket(PocketList& list){
    PocketRead read;
    std::vector<BagRow> reads;
    bool end = read_initial_pocket_page(list, reads, read);
    //  The rows below the first six: DOWN to the fourth row shown, then
    //  each DOWN scrolls the next row into the last. Reading one page only
    //  missed the X Specials under a MOON STONE, a NUGGET and four more,
    //  and the Tower rival was fought with none (console B, 2026-11-12).
    for (int c = 0; !end && c < ITEM_LIST_ROWS / 2; c++){
        list.cursor_down();
        read.downs++;
    }
    int same_in_a_row = 0;
    while (!end && reads.size() < MAX_POCKET_ROWS){
        list.cursor_down();
        read.downs++;
        std::optional<BagRow> row = list.read_shown(ITEM_LIST_ROWS - 1);
        //  Two consecutive identical bottom rows: the list stopped scrolling,
        //  its CANCEL unread. Requiring two tolerates a single dropped DOWN
        //  (e.g. Lorelei turn 11 re-read, 2026-11-22) without truncating the
        //  pocket and wiping the remaining items.
        const bool same = row && row->item == reads.back().item && row->count == reads.back().count;
        if (row && row->item == "cancel"){
            read.saw_cancel = true;
        }
        if (same){
            same_in_a_row++;
            end = same_in_a_row >= 2;
            continue;
        }
        same_in_a_row = 0;
        end = !row || row->item == "cancel";
        if (!end){
            reads.push_back(std::move(*row));
        }
    }
    read.rows = pocket_rows(std::move(reads));
    for (size_t c = 0; c < read.downs; c++){
        list.cursor_up();
    }
    return read;
}

namespace{

//  A DOWN that scrolls the list, press and wait before its rows are read
//  (read_items_pocket()'s, console B 2026-11-12).
constexpr Milliseconds SCROLL_TAP = 100ms;
constexpr Milliseconds SCROLL_WAIT = 400ms;
//  An UP back to the top, with no read after it (the field bag's).
constexpr Milliseconds CURSOR_UP_WAIT = 150ms;
//  The UP presses that bring the battle bag's cursor to the top, at
//  least: what the battle sent before the pocket's rows were counted.
constexpr size_t MIN_HOME_UPS = 12;

//  The open pocket on the console's screen.
class ConsolePocketList : public PocketList{
public:
    ConsolePocketList(ConsoleHandle& console, ProControllerContext& context, Language language)
        : m_console(console)
        , m_context(context)
        , m_language(language)
    {}
    virtual std::optional<BagRow> read_shown(int shown) override{
        if (!m_screen){
            m_context.wait_for_all_requests();
            m_screen = m_console.video().snapshot();
        }
        if (!m_screen){
            return std::nullopt;
        }
        return read_bag_row(m_console.logger(), m_language, m_screen, shown);
    }
    virtual void cursor_down() override{
        pbf_press_dpad(m_context, DPAD_DOWN, SCROLL_TAP, SCROLL_WAIT);
        m_screen = VideoSnapshot();
    }
    virtual void cursor_up() override{
        pbf_press_dpad(m_context, DPAD_UP, SCROLL_TAP, CURSOR_UP_WAIT);
        m_screen = VideoSnapshot();
    }

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Language m_language;
    VideoSnapshot m_screen;
};

} // namespace

PocketRead read_open_pocket(ConsoleHandle& console, ProControllerContext& context, Language language){
    ConsolePocketList list(console, context, language);
    return read_whole_pocket(list);
}

void open_battle_items_pocket(ProControllerContext& context){
    for (DpadPosition position : {DPAD_LEFT, DPAD_UP, DPAD_RIGHT}){
        pbf_press_dpad(context, position, /*hold=*/160ms, /*release=*/240ms);
    }
    pbf_press_button(context, BUTTON_A, /*hold=*/160ms, /*release=*/1500ms);
    for (int c = 0; c < 2; c++){
        pbf_press_dpad(context, DPAD_LEFT, /*hold=*/160ms, /*release=*/500ms);
    }
}

void open_battle_bag(ProControllerContext& context, size_t rows){
    open_battle_items_pocket(context);
    for (size_t c = 0; c < std::max(rows, MIN_HOME_UPS); c++){
        pbf_press_dpad(context, DPAD_UP, /*hold=*/100ms, /*release=*/150ms);
    }
}

std::optional<std::vector<BagRow>> read_battle_pocket(ConsoleHandle& console, ProControllerContext& context, Language language, size_t rows){
    open_battle_bag(context, rows);
    PocketRead read = read_open_pocket(console, context, language);
    pbf_press_button(context, BUTTON_B, /*hold=*/160ms, /*release=*/1500ms);
    if (read.rows.empty() && !read.saw_cancel){
        return std::nullopt;
    }
    return std::move(read.rows);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
