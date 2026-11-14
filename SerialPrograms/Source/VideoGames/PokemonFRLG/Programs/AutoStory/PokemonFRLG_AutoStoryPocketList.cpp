/*  AutoStory Pocket List
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonFRLG_AutoStoryPocketList.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using namespace std::chrono_literals;


//  The Items pocket's capacity (pokefirered BAG_ITEMS_COUNT).
constexpr size_t MAX_POCKET_ROWS = 42;

PocketRead read_whole_pocket(PocketList& list){
    PocketRead read;
    std::vector<BagRow> reads;
    bool end = false;
    for (int shown = 0; !end && shown < ITEM_LIST_ROWS; shown++){
        std::optional<BagRow> row = list.read_shown(shown);
        end = !row || row->item == "cancel";
        if (!end){
            reads.push_back(std::move(*row));
        }
    }
    //  The rows below the first six: DOWN to the fourth row shown, then
    //  each DOWN scrolls the next row into the last. Reading one page only
    //  missed the X Specials under a MOON STONE, a NUGGET and four more,
    //  and the Tower rival was fought with none (console B, 2026-11-12).
    for (int c = 0; !end && c < ITEM_LIST_ROWS / 2; c++){
        list.cursor_down();
        read.downs++;
    }
    while (!end && reads.size() < MAX_POCKET_ROWS){
        list.cursor_down();
        read.downs++;
        std::optional<BagRow> row = list.read_shown(ITEM_LIST_ROWS - 1);
        //  The same row again: the list stopped scrolling, its CANCEL unread.
        const bool same = row && row->item == reads.back().item && row->count == reads.back().count;
        end = !row || row->item == "cancel" || same;
        if (!end){
            reads.push_back(std::move(*row));
        }
    }
    read.rows = pocket_rows(std::move(reads));
    return read;
}

namespace{

//  A DOWN that scrolls the list, press and wait before its rows are read
//  (read_items_pocket()'s, console B 2026-11-12).
constexpr Milliseconds SCROLL_TAP = 100ms;
constexpr Milliseconds SCROLL_WAIT = 400ms;

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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
