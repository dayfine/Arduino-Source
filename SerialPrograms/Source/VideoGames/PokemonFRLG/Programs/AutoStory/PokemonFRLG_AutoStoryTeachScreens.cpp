/*  AutoStory Teach Screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryTeachScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


const char* teach_press_name(TeachPress press){
    switch (press){
    case TeachPress::A:     return "A";
    case TeachPress::B:     return "B";
    case TeachPress::UP:    return "UP";
    case TeachPress::DOWN:  return "DOWN";
    case TeachPress::LEFT:  return "LEFT";
    case TeachPress::RIGHT: return "RIGHT";
    }
    return "?";
}


namespace{

//  Looks a step makes before it gives up on a screen that doesn't change.
constexpr size_t MAX_LOOKS = 40;
//  Looks at the same moves page after A on a move: the game refused it.
constexpr size_t MOVES_PAGE_STILL = 8;

std::string rows_text(const std::vector<std::string>& rows){
    std::string text;
    for (const std::string& row : rows){
        text += (text.empty() ? "" : ", ") + row;
    }
    return text.empty() ? "none read" : text;
}

}


ScreenTeachActions::ScreenTeachActions(Logger& logger, TeachScreens& screens)
    : m_logger(logger)
    , m_screens(screens)
{}

void ScreenTeachActions::press(TeachPress press){
    m_screens.press(press);
}

std::optional<TeachView> ScreenTeachActions::wait_for(TeachScreen screen, size_t looks){
    for (size_t c = 0; c < looks; c++){
        TeachView view = m_screens.look();
        if (view.screen == screen){
            return view;
        }
    }
    m_logger.log(std::format("Teach: {} didn't show.", teach_screen_name(screen)), COLOR_RED);
    return std::nullopt;
}

bool ScreenTeachActions::move_cursor_to(TeachScreen screen, size_t row){
    for (size_t c = 0; c < TEACH_LIST_ROWS * 2; c++){
        const TeachView view = m_screens.look();
        if (view.screen != screen || !view.cursor){
            m_logger.log(std::format("Teach: lost the cursor on {}.", teach_screen_name(screen)), COLOR_RED);
            return false;
        }
        if (*view.cursor == row){
            return true;
        }
        press(*view.cursor < row ? TeachPress::DOWN : TeachPress::UP);
    }
    return false;
}

bool ScreenTeachActions::to_key_items(){
    for (size_t c = 0; c < 4; c++){
        const std::optional<TeachView> view = wait_for(TeachScreen::BAG);
        if (!view){
            return false;
        }
        if (view->pocket == "key-items"){
            return true;
        }
        if (view->pocket == "items"){
            press(TeachPress::RIGHT);
        }else if (view->pocket == "poke-balls"){
            press(TeachPress::LEFT);
        }else{
            m_logger.log("Teach: the bag's pocket wasn't read.", COLOR_RED);
            return false;
        }
    }
    return false;
}

ItemOpen ScreenTeachActions::open_row(TeachScreen screen, std::string_view name){
    const std::optional<TeachView> view = wait_for(screen);
    if (!view){
        return ItemOpen::FAILED;
    }
    const auto found = std::find(view->rows.begin(), view->rows.end(), name);
    if (found == view->rows.end()){
        m_logger.log(std::format("Teach: no {} in {} ({}).", name, teach_screen_name(screen), rows_text(view->rows)), COLOR_RED);
        //  No rows: nothing was read.
        return view->rows.empty() ? ItemOpen::FAILED : ItemOpen::NOT_IN_CASE;
    }
    if (!move_cursor_to(screen, size_t(found - view->rows.begin()))){
        return ItemOpen::FAILED;
    }
    press(TeachPress::A);
    if (!wait_for(TeachScreen::ITEM_MENU) || !move_cursor_to(TeachScreen::ITEM_MENU, 0)){
        return ItemOpen::FAILED;
    }
    press(TeachPress::A);
    return ItemOpen::OPENED;
}

ItemOpen ScreenTeachActions::open_item(std::string_view item, std::string_view move){
    m_tutor = item == TEACH_TUTOR;
    if (m_tutor){
        if (!m_screens.talk_to_tutor()){
            m_logger.log(std::format("Teach: the tutor's questions for {} didn't lead to the party menu.", move), COLOR_RED);
            return ItemOpen::FAILED;
        }
        return wait_for(TeachScreen::PARTY_PROMPT).has_value() ? ItemOpen::OPENED : ItemOpen::FAILED;
    }
    if (!m_screens.open_bag()){
        m_logger.log("Teach: the bag didn't open.", COLOR_RED);
        return ItemOpen::FAILED;
    }
    if (!to_key_items() || open_row(TeachScreen::BAG, "tm-case") != ItemOpen::OPENED){
        return ItemOpen::FAILED;
    }
    const ItemOpen opened = open_row(TeachScreen::TM_CASE, move);
    if (opened != ItemOpen::OPENED){
        m_logger.log(std::format("Teach: couldn't use {} ({}) from the TM CASE.", item, move), COLOR_RED);
        return opened;
    }
    return wait_for(TeachScreen::PARTY_PROMPT).has_value() ? ItemOpen::OPENED : ItemOpen::FAILED;
}

TeachPrompt ScreenTeachActions::choose_member(size_t slot){
    if (!wait_for(TeachScreen::PARTY_PROMPT)){
        return TeachPrompt::FAILED;
    }
    //  The menu opens on the lead; DOWN steps through the slots.
    for (size_t c = 0; c < slot; c++){
        press(TeachPress::DOWN);
    }
    press(TeachPress::A);
    return member_answer();
}

TeachPrompt ScreenTeachActions::member_answer(){
    bool incompatible = false;
    bool texts = false;
    size_t prompts = 0;
    for (size_t c = 0; c < MAX_LOOKS; c++){
        const TeachView view = m_screens.look();
        switch (view.screen){
        case TeachScreen::PARTY_TEXT:
            if (view.text == TeachText::WHICH_MOVE){
                return TeachPrompt::DELETE_PROMPT;
            }
            incompatible |= view.text == TeachText::NOT_COMPATIBLE;
            texts = true;
            press(TeachPress::A);
            break;
        case TeachScreen::OTHER:
            //  A tutor's party menu closes onto the overworld once learned.
            if (m_tutor && texts){
                return TeachPrompt::LEARNED;
            }
            break;
        case TeachScreen::PARTY_YES_NO:
            return view.text == TeachText::STOP_QUESTION ? TeachPrompt::FAILED : TeachPrompt::DELETE_PROMPT;
        case TeachScreen::KNOWN_MOVES:
            return TeachPrompt::DELETE_PROMPT;
        case TeachScreen::TM_CASE:
            return TeachPrompt::LEARNED;
        case TeachScreen::PARTY_PROMPT:
            //  Back on the prompt after a message, or not left yet.
            if (incompatible){
                return TeachPrompt::NOT_COMPATIBLE;
            }
            if (++prompts > MOVES_PAGE_STILL){
                return TeachPrompt::FAILED;
            }
            break;
        default:
            break;
        }
    }
    return TeachPrompt::FAILED;
}

void ScreenTeachActions::answer_to_forget(const TeachView& view){
    //  YES to delete a move; NO to stop trying, back to the moves.
    if (view.text == TeachText::STOP_QUESTION){
        press(TeachPress::B);
    }else{
        press(view.cursor == 1u ? TeachPress::UP : TeachPress::A);
    }
}

ForgetEnd ScreenTeachActions::forget(size_t slot){
    bool picked = false;
    size_t still = 0;
    //  Looks at a screen not placed since the move was picked.
    size_t unplaced = 0;
    for (size_t c = 0; c < MAX_LOOKS; c++){
        const TeachView view = m_screens.look();
        switch (view.screen){
        case TeachScreen::PARTY_YES_NO:
            answer_to_forget(view);
            break;
        case TeachScreen::PARTY_TEXT:
            press(TeachPress::A);
            break;
        case TeachScreen::KNOWN_MOVES:
            if (picked){
                if (++still > MOVES_PAGE_STILL){
                    return ForgetEnd::HM_REFUSED;
                }
            }else if (view.cursor && *view.cursor != slot){
                press(*view.cursor < slot ? TeachPress::DOWN : TeachPress::UP);
            }else if (view.cursor){
                press(TeachPress::A);
                picked = true;
            }
            break;
        case TeachScreen::TM_CASE:
        case TeachScreen::BAG:
        case TeachScreen::ITEM_MENU:
            return picked ? ForgetEnd::LEARNED : ForgetEnd::FAILED;
        default:
            //  A tutor's: leave() pages through them; the moves read decide.
            if (picked && m_tutor){
                return ForgetEnd::LEARNED;
            }
            //  After the pick, its messages: B pages through them, once the
            //  moves page has closed (the first look).
            if (picked && ++unplaced > 1){
                press(TeachPress::B);
            }
            break;
        }
    }
    if (picked && unplaced > 0){
        //  The moves page closed on the pick: the moves read after decide.
        m_logger.log("Teach: the TM CASE didn't show after the forget; reading the moves will tell.", COLOR_ORANGE);
        return ForgetEnd::LEARNED;
    }
    return ForgetEnd::FAILED;
}

void ScreenTeachActions::stop_teaching(){
    for (size_t c = 0; c < MAX_LOOKS; c++){
        const TeachView view = m_screens.look();
        switch (view.screen){
        case TeachScreen::KNOWN_MOVES:
        case TeachScreen::PARTY_PROMPT:
            press(TeachPress::B);
            break;
        case TeachScreen::PARTY_YES_NO:
            //  NO to deleting a move, then YES to stop trying.
            if (view.text == TeachText::DELETE_QUESTION){
                press(TeachPress::B);
            }else{
                press(view.cursor == 1u ? TeachPress::UP : TeachPress::A);
            }
            break;
        case TeachScreen::PARTY_TEXT:
            press(TeachPress::A);
            break;
        default:
            return;
        }
    }
}

void ScreenTeachActions::dump_screen(std::string_view label){
    m_screens.dump_screen(label);
}

bool ScreenTeachActions::leave(){
    for (size_t c = 0; c < MAX_LOOKS; c++){
        const TeachView view = m_screens.look();
        switch (view.screen){
        case TeachScreen::BAG:
        case TeachScreen::ITEM_MENU:
        case TeachScreen::TM_CASE:
        case TeachScreen::PARTY_PROMPT:
            press(TeachPress::B);
            break;
        case TeachScreen::PARTY_TEXT:
        case TeachScreen::PARTY_YES_NO:
        case TeachScreen::KNOWN_MOVES:
            stop_teaching();
            break;
        case TeachScreen::OTHER:
        case TeachScreen::TM_SCENE:
            return m_screens.to_overworld();
        }
    }
    return false;
}


ConsoleTeachScreens::ConsoleTeachScreens(ConsoleHandle& console, ProControllerContext& context, Language language)
    : m_console(console)
    , m_context(context)
    , m_language(language)
{}

bool ConsoleTeachScreens::open_bag(){
    try{
        open_bag_from_overworld(m_console, m_context);
        m_context.wait_for_all_requests();
        return true;
    }catch (OperationFailedException& e){
        m_console.log("Teach: " + e.message(), COLOR_RED);
        return false;
    }
}

bool ConsoleTeachScreens::talk_to_tutor(){
    try{
        //  A talks, pages through its lines and says YES, the top row.
        answer_yes_no(m_console, m_context, true, BUTTON_A, 30000ms);
        answer_yes_no(m_console, m_context, true, BUTTON_A, 15000ms);
    }catch (OperationFailedException& e){
        m_console.log("Teach: " + e.message(), COLOR_RED);
        return false;
    }
    //  "Which POKEMON wants to learn it?": A only on its advance arrow, as
    //  A on the party menu would choose the lead.
    for (size_t c = 0; c < 50; c++){
        m_context.wait_for_all_requests();
        VideoSnapshot snapshot = m_console.video().snapshot();
        if (snapshot && AdvanceWhiteDialogDetector(COLOR_RED).detect(snapshot)){
            pbf_press_button(m_context, BUTTON_A, 160ms, 240ms);
        }else if (snapshot && read_teach_screen(snapshot) == TeachScreen::PARTY_PROMPT){
            return true;
        }else{
            pbf_wait(m_context, 300ms);
        }
    }
    return false;
}

TeachView ConsoleTeachScreens::look(){
    //  Two looks 150 ms apart that agree: the screen has stopped changing.
    std::optional<std::pair<TeachScreen, std::optional<size_t>>> last;
    for (size_t c = 0; c < 10; c++){
        m_context.wait_for_all_requests();
        VideoSnapshot snapshot = m_console.video().snapshot();
        if (snapshot){
            const TeachScreen screen = read_teach_screen(snapshot);
            const std::optional<size_t> cursor = read_teach_cursor(screen, snapshot);
            if (last && last->first == screen && last->second == cursor){
                TeachView view = read_view(snapshot, screen, cursor);
                log_view(view);
                return view;
            }
            last.emplace(screen, cursor);
        }
        pbf_wait(m_context, 150ms);
    }
    return {};
}

TeachView ConsoleTeachScreens::read_view(const ImageViewRGB32& image, TeachScreen screen, std::optional<size_t> cursor){
    Logger& logger = m_console.logger();
    TeachView view{.screen = screen, .cursor = cursor};
    switch (screen){
    case TeachScreen::BAG:
        view.pocket = read_pocket_name(logger, m_language, image);
        if (view.pocket == "key-items"){
            view.rows = read_teach_rows(logger, m_language, image, screen);
        }
        break;
    case TeachScreen::TM_CASE:
        view.rows = read_teach_rows(logger, m_language, image, screen);
        break;
    case TeachScreen::PARTY_PROMPT:
    case TeachScreen::PARTY_TEXT:
    case TeachScreen::PARTY_YES_NO:
        view.text = read_teach_text(logger, m_language, image);
        break;
    default:
        break;
    }
    return view;
}

void ConsoleTeachScreens::log_view(const TeachView& view){
    std::string line = std::format(
        "Teach: {}, cursor {}", teach_screen_name(view.screen), view.cursor ? std::to_string(*view.cursor + 1) : "none"
    );
    if (!view.pocket.empty()){
        line += ", " + view.pocket;
    }
    if (!view.rows.empty()){
        line += " (" + rows_text(view.rows) + ")";
    }
    if (view.text != TeachText::UNKNOWN){
        line += std::string(", \"") + teach_text_name(view.text) + "\"";
    }
    if (line != m_logged){
        m_console.log(line + ".");
        m_logged = line;
    }
}

void ConsoleTeachScreens::press(TeachPress press){
    switch (press){
    case TeachPress::A:     pbf_press_button(m_context, BUTTON_A, 160ms, 240ms); break;
    case TeachPress::B:     pbf_press_button(m_context, BUTTON_B, 160ms, 240ms); break;
    case TeachPress::UP:    pbf_press_dpad(m_context, DPAD_UP, 160ms, 240ms); break;
    case TeachPress::DOWN:  pbf_press_dpad(m_context, DPAD_DOWN, 160ms, 240ms); break;
    case TeachPress::LEFT:  pbf_press_dpad(m_context, DPAD_LEFT, 160ms, 240ms); break;
    case TeachPress::RIGHT: pbf_press_dpad(m_context, DPAD_RIGHT, 160ms, 240ms); break;
    }
    m_context.wait_for_all_requests();
}

void ConsoleTeachScreens::dump_screen(std::string_view label){
    m_context.wait_for_all_requests();
    dump_debug_image(m_console, "AutoStory/teach", std::string(label), m_console.video().snapshot());
}

bool ConsoleTeachScreens::to_overworld(){
    try{
        mash_until_overworld(m_console, m_context, 20000ms);
        return true;
    }catch (OperationFailedException& e){
        m_console.log("Teach: " + e.message(), COLOR_RED);
        return false;
    }
}



}
}
}
