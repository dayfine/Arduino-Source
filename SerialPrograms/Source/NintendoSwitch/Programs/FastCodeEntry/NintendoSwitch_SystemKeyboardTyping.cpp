/*  System Keyboard Typing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch_KeyboardCodeEntry.h"
#include "NintendoSwitch_SystemKeyboardTyping.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace FastCodeEntry{

using namespace std::chrono_literals;


namespace{

constexpr Milliseconds MOVE_HOLD = 100ms;       //  One key per press, without auto-repeat.
constexpr Milliseconds MOVE_RELEASE = 200ms;
constexpr Milliseconds WRAP_RELEASE = 400ms;    //  Wrapping around a side animates for longer.
constexpr Milliseconds SETTLE = 300ms;          //  Before reading the cursor after moving it.
constexpr Milliseconds KEY_HOLD = 80ms;
constexpr Milliseconds KEY_RELEASE = 300ms;

//  A press made while the keyboard is still sliding in is ignored, so the
//  cursor is read again after each try until it is on the target.
constexpr size_t MAX_MOVE_TRIES = 10;


bool read_cursor(VideoStream& stream, ProControllerContext& context, KeyboardEntryPosition& position){
    context.wait_for_all_requests();
    VideoSnapshot screen = stream.video().snapshot();
    int row = -1;
    int col = -1;
    if (!screen || !find_system_keyboard_cursor(screen, row, col)){
        return false;
    }
    position = {(uint8_t)row, (uint8_t)col};
    return true;
}

void move_cursor(VideoStream& stream, ProControllerContext& context, KeyboardEntryPosition target, char ch){
    for (size_t attempt = 0; attempt < MAX_MOVE_TRIES; attempt++){
        KeyboardEntryPosition cursor;
        if (!read_cursor(stream, context, cursor)){
            pbf_wait(context, SETTLE);
            continue;
        }
        if (cursor.row == target.row && cursor.col == target.col){
            return;
        }
        for (CodeEntryAction action : keyboard_get_path(cursor, target)){
            if (!is_move(action)){
                continue;
            }
            const bool wrap = ((uint8_t)action & 0x80) != 0;
            const DpadPosition direction = (DpadPosition)((uint8_t)action & 0x7f);
            pbf_press_dpad(context, direction, MOVE_HOLD, wrap ? WRAP_RELEASE : MOVE_RELEASE);
        }
        pbf_wait(context, SETTLE);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::string("System keyboard: the cursor did not reach '") + ch + "'.",
        stream
    );
}

}



void type_on_system_keyboard(
    VideoStream& stream, ProControllerContext& context,
    const std::string& text,
    KeyboardLayout layout
){
    const std::map<char, KeyboardEntryPosition>& positions = KEYBOARD_POSITIONS(layout);
    for (char ch : text){
        //  Letters are listed under their uppercase key.
        auto iter = positions.find((char)std::toupper((unsigned char)ch));
        if (iter == positions.end()){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::string("System keyboard: no key for '") + ch + "'.",
                stream
            );
        }
        move_cursor(stream, context, iter->second, ch);
        if (std::isupper((unsigned char)ch)){
            pbf_press_button(context, BUTTON_LCLICK, 100ms, 250ms);
        }
        pbf_press_button(context, BUTTON_A, KEY_HOLD, KEY_RELEASE);
    }
    context.wait_for_all_requests();
}



}
}
}
