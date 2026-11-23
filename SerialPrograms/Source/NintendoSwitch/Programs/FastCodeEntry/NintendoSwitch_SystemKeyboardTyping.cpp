/*  System Keyboard Typing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <cstddef>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_CodeEntryTools.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardCodeEntry.h"
#include "NintendoSwitch_SystemKeyboardTyping.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace FastCodeEntry{

using namespace std::chrono_literals;


namespace{

//  Longer than the code entry delays: the next step reads the screen, so the cursor
//  animation and the capture latency must be over.
constexpr Milliseconds MOVE_RELEASE = 200ms;
constexpr Milliseconds WRAP_RELEASE = 400ms;    //  Wrapping around a side animates for longer.
constexpr Milliseconds SETTLE = 300ms;          //  Before reading the cursor after moving it.
constexpr Milliseconds SHIFT_RELEASE = 250ms;
constexpr Milliseconds KEY_RELEASE = 300ms;
constexpr Milliseconds PAGE_SWITCH_RELEASE = 400ms;
constexpr Milliseconds CONFIRM_RELEASE = 400ms;
constexpr Milliseconds CONFIRM_POLL = 100ms;

//  Presses while the keyboard slides in are ignored, so the cursor is re-read each try.
constexpr size_t MAX_MOVE_TRIES = 10;
constexpr size_t MAX_LETTERS_TRIES = 20;

//  Reads of the text field after a key press, SETTLE apart, before giving up.
constexpr size_t MAX_FIELD_READS = 3;

//  Consecutive closed readings before confirm_system_keyboard() stops retrying PLUS.
constexpr size_t CONFIRM_CLOSED_READS = 2;

//  The fewest white pixels a typed character adds to the field ("." or "'").
constexpr size_t MIN_CHARACTER_PIXELS = 10;

//  Bottom-row English/Latin key on CJK IME keyboards (e.g. Simplified Chinese Pinyin).
constexpr SystemKeyboardKey LETTERS_KEY{4, 3};


std::vector<KeyboardEntryPosition> text_keys(Logger& logger, const std::string& text, KeyboardLayout layout){
    const std::map<char, KeyboardEntryPosition>& positions = KEYBOARD_POSITIONS(layout);
    std::vector<KeyboardEntryPosition> keys;
    std::string missing;
    for (char ch : text){
        //  Letters are listed under their uppercase key.
        auto iter = positions.find((char)std::toupper((unsigned char)ch));
        if (iter == positions.end()){
            missing += ch;
        }else{
            keys.emplace_back(iter->second);
        }
    }
    if (!missing.empty()){
        throw UserSetupError(
            logger,
            std::format("System keyboard: \"{}\" has characters with no key on the keyboard: \"{}\".", text, missing)
        );
    }
    return keys;
}

Milliseconds button_hold(ProControllerContext& context){
    const ConsoleSettings& settings = ConsoleSettings::instance();
    const CodeEntryDelays delays = context->performance_class() == ControllerPerformanceClass::SerialPABotBase_Wired
        ? CodeEntryDelays(settings.CODEBOARD_ENTRY_SWITCH1_WIRED)
        : CodeEntryDelays(settings.CODEBOARD_ENTRY_SWITCH1_WIRELESS);
    return delays.hold;
}

DpadPosition move_direction(CodeEntryAction action){
    switch (action){
    case CodeEntryAction::NORM_MOVE_UP:
    case CodeEntryAction::WRAP_MOVE_UP:
        return DpadPosition::DPAD_UP;
    case CodeEntryAction::NORM_MOVE_RIGHT:
    case CodeEntryAction::WRAP_MOVE_RIGHT:
        return DpadPosition::DPAD_RIGHT;
    case CodeEntryAction::NORM_MOVE_DOWN:
    case CodeEntryAction::WRAP_MOVE_DOWN:
        return DpadPosition::DPAD_DOWN;
    case CodeEntryAction::NORM_MOVE_LEFT:
    case CodeEntryAction::WRAP_MOVE_LEFT:
        return DpadPosition::DPAD_LEFT;
    default:
        return DpadPosition::DPAD_NONE;
    }
}

//  Off the character grid (the right column or bottom row), one step back onto it,
//  then the cursor is read again.
std::vector<CodeEntryAction> path_toward(SystemKeyboardKey cursor, KeyboardEntryPosition target){
    if (cursor.col == 11){
        return {CodeEntryAction::NORM_MOVE_LEFT};
    }
    if (cursor.row == 4){
        return {CodeEntryAction::NORM_MOVE_UP};
    }
    std::vector<CodeEntryAction> path = keyboard_get_path({cursor.row, cursor.col}, target);
    std::erase_if(path, [](CodeEntryAction action){ return !is_move(action); });
    return path;
}

char dpad_to_char(DpadPosition direction){
    switch (direction){
    case DpadPosition::DPAD_UP:
        return 'U';
    case DpadPosition::DPAD_RIGHT:
        return 'R';
    case DpadPosition::DPAD_DOWN:
        return 'D';
    case DpadPosition::DPAD_LEFT:
        return 'L';
    default:
        return '?';
    }
}

//  Single-step navigation toward an arbitrary key (including row 4's variable-width
//  keys). Moves left off column 11 and past target.col before descending so it never
//  gets stuck on OK (rows 3-4) or detours across the wide Spacebar.
std::optional<DpadPosition> step_toward_key(SystemKeyboardKey cursor, SystemKeyboardKey target){
    if (cursor == target){
        return std::nullopt;
    }
    if (cursor.col > target.col){
        return DpadPosition::DPAD_LEFT;
    }
    if (cursor.row < target.row){
        return DpadPosition::DPAD_DOWN;
    }
    if (cursor.row > target.row){
        return DpadPosition::DPAD_UP;
    }
    return DpadPosition::DPAD_RIGHT;
}

//  Tracks consecutive frames reporting the keyboard closed. A missing or open frame
//  resets the streak so a single-frame animation blink (e.g. IME commit) does not
//  falsely end the PLUS retry loop.
bool confirm_closed_reached(
    std::optional<bool> frame_closed,
    size_t required_reads,
    size_t& consecutive_closed
){
    if (!frame_closed || !*frame_closed){
        consecutive_closed = 0;
        return false;
    }
    consecutive_closed++;
    return consecutive_closed >= required_reads;
}

//  The moves as letters for the log, e.g. "DLL" or "R*" for a wrap to the right.
std::string path_to_string(const std::vector<CodeEntryAction>& path){
    std::string ret;
    for (CodeEntryAction action : path){
        ret += dpad_to_char(move_direction(action));
        if (is_wrap(action)){
            ret += '*';
        }
    }
    return ret;
}

std::optional<SystemKeyboardKey> read_cursor(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot screen = stream.video().snapshot();
    if (!screen){
        return std::nullopt;
    }
    return read_system_keyboard_cursor(screen);
}

[[noreturn]] void throw_cursor_failure(
    VideoStream& stream,
    std::optional<SystemKeyboardKey> last, KeyboardEntryPosition target, char ch
){
    std::string message = last
        ? std::format("the cursor is at ({}, {}), not on '{}' at ({}, {})", last->row, last->col, ch, target.row, target.col)
        : std::format("the cursor could not be read while moving to '{}' at ({}, {})", ch, target.row, target.col);
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("System keyboard: {}, after {} tries.", message, MAX_MOVE_TRIES),
        stream
    );
}

void move_cursor(
    VideoStream& stream, ProControllerContext& context, Milliseconds hold,
    KeyboardEntryPosition target, char ch
){
    std::optional<SystemKeyboardKey> last;
    for (size_t attempt = 0; attempt < MAX_MOVE_TRIES; attempt++){
        const std::optional<SystemKeyboardKey> cursor = read_cursor(stream, context);
        if (!cursor){
            stream.logger().log(std::format("System keyboard: no cursor seen, try {}.", attempt + 1));
            pbf_wait(context, SETTLE);
            continue;
        }
        last = cursor;
        if (cursor->row == target.row && cursor->col == target.col){
            return;
        }
        const std::vector<CodeEntryAction> path = path_toward(*cursor, target);
        stream.logger().log(std::format(
            "System keyboard: cursor ({}, {}) -> '{}' ({}, {}): {}, try {}.",
            cursor->row, cursor->col, ch, target.row, target.col, path_to_string(path), attempt + 1
        ));
        for (CodeEntryAction action : path){
            pbf_press_dpad(context, move_direction(action), hold, is_wrap(action) ? WRAP_RELEASE : MOVE_RELEASE);
        }
        pbf_wait(context, SETTLE);
    }
    throw_cursor_failure(stream, last, target, ch);
}

size_t read_text_pixels(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot screen = stream.video().snapshot();
    return screen ? system_keyboard_text_pixels(screen) : 0;
}

//  Types `ch` with the cursor on its key, and checks that the text field grew.
void press_key(VideoStream& stream, ProControllerContext& context, Milliseconds hold, char ch){
    const size_t before = read_text_pixels(stream, context);
    if (std::isupper((unsigned char)ch)){
        pbf_press_button(context, BUTTON_LCLICK, hold, SHIFT_RELEASE);
    }
    pbf_press_button(context, BUTTON_A, hold, KEY_RELEASE);
    size_t after = 0;
    for (size_t read = 0; read < MAX_FIELD_READS; read++){
        after = read_text_pixels(stream, context);
        if (after >= before + MIN_CHARACTER_PIXELS){
            return;
        }
        pbf_wait(context, SETTLE);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("System keyboard: pressed '{}', but the text field did not change ({} -> {} white pixels).", ch, before, after),
        stream
    );
}

[[noreturn]] void throw_letters_failure(
    VideoStream& stream, std::optional<SystemKeyboardKey> last
){
    std::string message = last
        ? std::format("the cursor is at ({}, {}), not on the letters key ({}, {})", last->row, last->col, LETTERS_KEY.row, LETTERS_KEY.col)
        : std::format("the cursor could not be read while moving to the letters key ({}, {})", LETTERS_KEY.row, LETTERS_KEY.col);
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("System keyboard: {}, after {} tries.", message, MAX_LETTERS_TRIES),
        stream
    );
}

bool wait_for_keyboard_close(
    VideoStream& stream, ProControllerContext& context,
    const std::function<bool(const ImageViewRGB32&)>& is_closed,
    Milliseconds timeout
){
    context.wait_for_all_requests();
    const WallClock deadline = current_time() + timeout;
    size_t consecutive_closed = 0;
    while (true){
        const VideoSnapshot screen = stream.video().snapshot();
        const std::optional<bool> closed = screen ? std::optional<bool>(is_closed(screen)) : std::nullopt;
        if (confirm_closed_reached(closed, CONFIRM_CLOSED_READS, consecutive_closed)){
            return true;
        }
        if (current_time() >= deadline){
            return false;
        }
        context.wait_for(CONFIRM_POLL);
    }
}

}  // namespace



void switch_system_keyboard_to_letters(VideoStream& stream, ProControllerContext& context){
    const Milliseconds hold = button_hold(context);
    std::optional<SystemKeyboardKey> last;
    for (size_t attempt = 0; attempt < MAX_LETTERS_TRIES; attempt++){
        const std::optional<SystemKeyboardKey> cursor = read_cursor(stream, context);
        if (!cursor){
            stream.logger().log(std::format("System keyboard: no cursor seen while switching to letters, try {}.", attempt + 1));
            pbf_wait(context, SETTLE);
            continue;
        }
        last = cursor;
        const std::optional<DpadPosition> step = step_toward_key(*cursor, LETTERS_KEY);
        if (!step){
            stream.logger().log("System keyboard: switching to Latin letters key (4, 3).");
            pbf_press_button(context, BUTTON_A, hold, PAGE_SWITCH_RELEASE);
            context.wait_for_all_requests();
            return;
        }
        stream.logger().log(std::format("System keyboard: cursor ({}, {}) -> letters ({}, {}): {}, try {}.", cursor->row, cursor->col, LETTERS_KEY.row, LETTERS_KEY.col, dpad_to_char(*step), attempt + 1));
        pbf_press_dpad(context, *step, hold, MOVE_RELEASE);
        pbf_wait(context, SETTLE);
    }
    throw_letters_failure(stream, last);
}

void type_on_system_keyboard(
    VideoStream& stream, ProControllerContext& context,
    const std::string& text,
    KeyboardLayout layout
){
    const std::vector<KeyboardEntryPosition> keys = text_keys(stream.logger(), text, layout);
    const Milliseconds hold = button_hold(context);
    for (size_t c = 0; c < text.size(); c++){
        move_cursor(stream, context, hold, keys[c], text[c]);
        press_key(stream, context, hold, text[c]);
    }
    context.wait_for_all_requests();
}

void confirm_system_keyboard(
    VideoStream& stream, ProControllerContext& context,
    const std::function<bool(const ImageViewRGB32&)>& is_closed,
    size_t max_presses,
    Milliseconds wait_per_press
){
    const auto closed_check = [&](const ImageViewRGB32& screen){
        return is_closed ? is_closed(screen) : !read_system_keyboard_cursor(screen).has_value();
    };
    const Milliseconds hold = button_hold(context);
    for (size_t pressed = 1; pressed <= max_presses; pressed++){
        stream.logger().log(std::format("System keyboard: pressing PLUS ({} of at most {}).", pressed, max_presses));
        pbf_press_button(context, BUTTON_PLUS, hold, CONFIRM_RELEASE);
        if (wait_for_keyboard_close(stream, context, closed_check, wait_per_press)){
            return;
        }
        stream.logger().log("System keyboard: still open after PLUS.");
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("System keyboard: stayed open after {} PLUS {}.", max_presses, max_presses == 1 ? "press" : "presses"),
        stream
    );
}



namespace{

SystemKeyboardKey apply_step(SystemKeyboardKey cursor, DpadPosition step){
    switch (step){
    case DpadPosition::DPAD_UP:
        return SystemKeyboardKey{(uint8_t)(cursor.row - 1), cursor.col};
    case DpadPosition::DPAD_DOWN:
        return SystemKeyboardKey{(uint8_t)(cursor.row + 1), cursor.col};
    case DpadPosition::DPAD_LEFT:
        return SystemKeyboardKey{cursor.row, (uint8_t)(cursor.col - 1)};
    case DpadPosition::DPAD_RIGHT:
        return SystemKeyboardKey{cursor.row, (uint8_t)(cursor.col + 1)};
    default:
        return cursor;
    }
}

class Test_StepTowardKey : public UnitTest{
public:
    Test_StepTowardKey()
        : UnitTest("NintendoSwitch::SystemKeyboardTyping - step_toward_key")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        struct Case{ SystemKeyboardKey cursor; SystemKeyboardKey target; std::optional<DpadPosition> expected; };
        const Case cases[] = {
            {{4, 3},  LETTERS_KEY, std::nullopt},
            {{0, 0},  LETTERS_KEY, DpadPosition::DPAD_DOWN},
            {{3, 3},  LETTERS_KEY, DpadPosition::DPAD_DOWN},
            {{4, 0},  LETTERS_KEY, DpadPosition::DPAD_RIGHT},
            {{4, 2},  LETTERS_KEY, DpadPosition::DPAD_RIGHT},
            {{4, 4},  LETTERS_KEY, DpadPosition::DPAD_LEFT},
            {{4, 8},  LETTERS_KEY, DpadPosition::DPAD_LEFT},
            {{2, 8},  LETTERS_KEY, DpadPosition::DPAD_LEFT},
            {{0, 11}, LETTERS_KEY, DpadPosition::DPAD_LEFT},
            {{1, 11}, LETTERS_KEY, DpadPosition::DPAD_LEFT},
            {{3, 11}, LETTERS_KEY, DpadPosition::DPAD_LEFT},
            {{4, 5},  {1, 5},      DpadPosition::DPAD_UP},
            {{3, 11}, {0, 11},     DpadPosition::DPAD_UP},
        };
        for (const Case& c : cases){
            if (step_toward_key(c.cursor, c.target) != c.expected){
                return std::format("step_toward_key(({}, {}), ({}, {})) mismatch.", c.cursor.row, c.cursor.col, c.target.row, c.target.col);
            }
        }
        return true;
    }
};

class Test_StepTowardKeyConvergence : public UnitTest{
public:
    Test_StepTowardKeyConvergence()
        : UnitTest("NintendoSwitch::SystemKeyboardTyping - step_toward_key convergence")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        for (uint8_t row = 0; row < 5; row++){
            for (uint8_t col = 0; col < 12; col++){
                SystemKeyboardKey cursor{row, col};
                size_t steps = 0;
                while (const std::optional<DpadPosition> step = step_toward_key(cursor, LETTERS_KEY)){
                    cursor = apply_step(cursor, *step);
                    steps++;
                    if (steps >= MAX_LETTERS_TRIES){
                        return std::format("Did not reach letters key from ({}, {}) within {} steps.", row, col, MAX_LETTERS_TRIES);
                    }
                }
            }
        }
        return true;
    }
};

bool run_debounce_sequence(const std::vector<std::optional<bool>>& readings){
    size_t consecutive = 0;
    for (const std::optional<bool>& reading : readings){
        if (confirm_closed_reached(reading, CONFIRM_CLOSED_READS, consecutive)){
            return true;
        }
    }
    return false;
}

class Test_ConfirmClosedDebounce : public UnitTest{
public:
    Test_ConfirmClosedDebounce()
        : UnitTest("NintendoSwitch::SystemKeyboardTyping - confirm_closed_reached")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        struct Sequence{ std::string_view label; std::vector<std::optional<bool>> readings; bool expected_closed; };
        const Sequence sequences[] = {
            {"two consecutive closed frames", {true, true}, true},
            {"single closed blink resets on open", {true, false, true}, false},
            {"missing frame resets streak", {true, std::nullopt, true}, false},
            {"closes after blink and two consecutive closed", {false, true, false, true, true}, true},
            {"all open never closes", {false, false, false}, false},
            {"all missing never closes", {std::nullopt, std::nullopt, std::nullopt}, false},
        };
        for (const Sequence& seq : sequences){
            const bool closed = run_debounce_sequence(seq.readings);
            if (closed != seq.expected_closed){
                return std::format("Debounce sequence '{}' gave {}.", seq.label, closed);
            }
        }
        return true;
    }
};

}  // namespace


void add_tests_SystemKeyboardTyping(UnitTestDatabase& database){
    database.add<Test_StepTowardKey>();
    database.add<Test_StepTowardKeyConvergence>();
    database.add<Test_ConfirmClosedDebounce>();
}



}  // namespace FastCodeEntry
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
