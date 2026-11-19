/*  PokemonLZA AutoStory - New game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <format>
#include <functional>
#include <map>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardEntryMappings.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_SystemKeyboardTyping.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryConfirmChoicesReader.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryIntroScreens.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStoryLaunch.h"
#include "PokemonLZA_AutoStoryNewGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The Switch keyboard typer types the first page only (letters by their capital).
bool on_keyboard(std::string_view name){
    const std::map<char, KeyboardEntryPosition>& keys = KEYBOARD_POSITIONS(KeyboardLayout::QWERTY);
    for (const char ch : name){
        if (!keys.contains((char)std::toupper((unsigned char)ch))){
            return false;
        }
    }
    return true;
}

std::optional<std::string> name_typing_problem(const NewGameChoices& choices){
    const NewGameLanguageInfo& info = new_game_language_info(choices.language);
    if (!info.keyboard_supported){
        return std::format(
            "AutoStory: the program cannot type a name on the {} keyboard (Hangul).",
            info.tag
        );
    }
    if (!on_keyboard(choices.name)){
        return std::format(
            "AutoStory: the player name \"{}\" has a character that is not on the name keyboard's first page "
            "(letters, digits and - / : ' , . ? !).",
            choices.name
        );
    }
    return std::nullopt;
}


using FrameCheck = std::function<bool(const ImageViewRGB32& frame)>;

constexpr std::string_view DUMP_FOLDER = "PokemonLZA/AutoStory/Intro";

//  The logos before the language list take about 15 s, with no press needed.
constexpr Milliseconds LAUNCH_LIMIT = 60000ms;

//  Faster taps on the language list are dropped.
constexpr Milliseconds LANGUAGE_TAP_RELEASE = 600ms;

//  One portrait per tilt.
constexpr Milliseconds LOOK_TILT_HOLD = 150ms;
constexpr Milliseconds LOOK_TILT_RELEASE = 500ms;

//  One + submits the name; Chinese keyboards first take one to "Accept" the typed text.
constexpr size_t SUBMIT_PRESSES = 3;
constexpr Milliseconds SUBMIT_WAIT = 5000ms;

VideoSnapshot snapshot(ConsoleHandle& console, ProControllerContext& context){
    context.wait_for_all_requests();
    return console.video().snapshot();
}

bool wait_for_frame(ConsoleHandle& console, ProControllerContext& context, const FrameCheck& check, Milliseconds timeout){
    const WallClock deadline = current_time() + timeout;
    while (true){
        const VideoSnapshot frame = snapshot(console, context);
        if (frame && check(frame)){
            return true;
        }
        if (current_time() >= deadline){
            return false;
        }
        context.wait_for(250ms);
    }
}

[[noreturn]] void fail(ConsoleHandle& console, const std::string& message){
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, std::format("AutoStory intro: {}", message), console);
}

void press(ProControllerContext& context, IntroKey key, Milliseconds release){
    switch (key){
    case IntroKey::UP:
        pbf_press_dpad(context, DPAD_UP, /*hold=*/80ms, release);
        break;
    case IntroKey::DOWN:
        pbf_press_dpad(context, DPAD_DOWN, /*hold=*/80ms, release);
        break;
    case IntroKey::LEFT:
        pbf_press_dpad(context, DPAD_LEFT, /*hold=*/80ms, release);
        break;
    case IntroKey::RIGHT:
        pbf_press_dpad(context, DPAD_RIGHT, /*hold=*/80ms, release);
        break;
    case IntroKey::A:
        pbf_press_button(context, BUTTON_A, /*hold=*/80ms, release);
        break;
    }
}

//  The D-pad does not move the look picker.
void tilt(ProControllerContext& context, IntroKey key){
    switch (key){
    case IntroKey::UP:
        pbf_move_left_joystick(context, /*position=*/{0, +1}, LOOK_TILT_HOLD, LOOK_TILT_RELEASE);
        break;
    case IntroKey::DOWN:
        pbf_move_left_joystick(context, /*position=*/{0, -1}, LOOK_TILT_HOLD, LOOK_TILT_RELEASE);
        break;
    case IntroKey::LEFT:
        pbf_move_left_joystick(context, /*position=*/{-1, 0}, LOOK_TILT_HOLD, LOOK_TILT_RELEASE);
        break;
    case IntroKey::RIGHT:
        pbf_move_left_joystick(context, /*position=*/{+1, 0}, LOOK_TILT_HOLD, LOOK_TILT_RELEASE);
        break;
    default:
        break;
    }
}

void wait_for_language_list(ConsoleHandle& console, ProControllerContext& context){
    const WallClock start = current_time();
    while (current_time() - start < LAUNCH_LIMIT){
        const VideoSnapshot frame = snapshot(console, context);
        if (frame && read_language_list_cursor(frame)){
            console.log("AutoStory intro: the language list shows.");
            dump_debug_image(console, std::string(DUMP_FOLDER), "language-list", frame);
            return;
        }
        context.wait_for(250ms);
    }
    fail(console, std::format("the language list did not show within {} s of the launch.", LAUNCH_LIMIT.count() / 1000));
}

void pick_language(ConsoleHandle& console, ProControllerContext& context, NewGameLanguage language){
    const NewGameLanguageInfo& info = new_game_language_info(language);
    for (size_t attempt = 0; attempt < 20; attempt++){
        const VideoSnapshot frame = snapshot(console, context);
        const std::optional<size_t> cursor = frame ? read_language_list_cursor(frame) : std::nullopt;
        if (!cursor){
            context.wait_for(250ms);
            continue;
        }
        if (*cursor == info.list_entry){
            console.log(std::format("AutoStory intro: language {} (entry {}): A.", info.tag, info.list_entry));
            press(context, IntroKey::A, 500ms);
            return;
        }
        const std::optional<std::vector<IntroKey>> moves = language_list_moves(*cursor, info.list_entry);
        if (!moves || moves->empty()){
            break;
        }
        press(context, moves->front(), LANGUAGE_TAP_RELEASE);
    }
    fail(console, std::format("the language list's highlight did not reach {}.", info.tag));
}

//  Settled = two reads in a row agree.
std::optional<size_t> read_settled_look(ConsoleHandle& console, ProControllerContext& context){
    std::optional<size_t> previous;
    for (size_t read = 0; read < 14; read++){
        const VideoSnapshot frame = snapshot(console, context);
        const std::optional<size_t> cursor = frame ? read_look_picker_cursor(frame) : std::nullopt;
        if (cursor && cursor == previous){
            return cursor;
        }
        previous = cursor;
        context.wait_for(150ms);
    }
    return std::nullopt;
}

//  The picker opens with no highlight, and the first tilt highlights a
//  portrait that varies from launch to launch, so tilt until one reads.
void pick_look(ConsoleHandle& console, ProControllerContext& context, size_t look){
    if (!wait_for_frame(console, context, is_look_picker, 20000ms)){
        fail(console, "the look picker did not show within 20 s of the language's A.");
    }
    const VideoSnapshot picker = snapshot(console, context);
    if (picker){
        dump_debug_image(console, std::string(DUMP_FOLDER), "look-picker", picker);
    }
    size_t unread = 0;
    for (size_t attempt = 0; attempt < 24 && unread <= 6; attempt++){
        const std::optional<size_t> cursor = read_settled_look(console, context);
        if (!cursor){
            unread++;
            tilt(context, IntroKey::LEFT);
            continue;
        }
        if (*cursor == look){
            console.log(std::format("AutoStory intro: look {}: A.", look));
            press(context, IntroKey::A, 500ms);
            return;
        }
        const std::optional<std::vector<IntroKey>> moves = look_picker_moves(*cursor, look);
        if (!moves || moves->empty()){
            break;
        }
        tilt(context, moves->front());
    }
    fail(console, std::format("the look picker's highlight was not read on look {}.", look));
}

//  Presses while the keyboard slides in are dropped.
void wait_for_keyboard(ConsoleHandle& console, ProControllerContext& context){
    if (!wait_for_frame(console, context, is_name_keyboard, 20000ms)){
        fail(console, "the name keyboard did not show within 20 s of the look's A.");
    }
    context.wait_for(1000ms);
}

//  Throws UserSetupError, nothing typed, if the "0/N" counter is below the name's length.
void check_name_limit(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    const VideoSnapshot frame = snapshot(console, context);
    if (frame){
        dump_debug_image(console, std::string(DUMP_FOLDER), "keyboard", frame);
    }
    const std::optional<size_t> limit = frame ? read_name_keyboard_limit(frame) : std::nullopt;
    if (!limit){
        console.log("AutoStory intro: keyboard: the 0/N counter did not read; the language's limit holds.");
    }else if (*limit < choices.name.size()){
        throw UserSetupError(console, std::format(
            "AutoStory: the name keyboard takes {} characters; the player name \"{}\" has {}. Nothing was typed.",
            *limit, choices.name, choices.name.size()
        ));
    }else{
        console.log(std::format("AutoStory intro: keyboard: takes {} characters.", *limit));
    }
}

//  Simplified Chinese's keyboard opens in Pinyin; its English key switches to letters.
void switch_to_letters(ConsoleHandle& console, ProControllerContext& context){
    constexpr SystemKeyboardKey LETTERS_KEY{.row = 4, .col = 3};
    for (size_t attempt = 0; attempt < 16; attempt++){
        const VideoSnapshot frame = snapshot(console, context);
        const std::optional<SystemKeyboardKey> cursor = frame ? read_system_keyboard_cursor(frame) : std::nullopt;
        if (!cursor){
            context.wait_for(300ms);
            continue;
        }
        if (*cursor == LETTERS_KEY){
            console.log("AutoStory intro: keyboard: English (letters) key: A.");
            press(context, IntroKey::A, 400ms);
            return;
        }
        if (cursor->col == 11 || cursor->col > LETTERS_KEY.col){
            press(context, IntroKey::LEFT, 300ms);
        }else if (cursor->row < LETTERS_KEY.row){
            press(context, IntroKey::DOWN, 300ms);
        }else{
            press(context, IntroKey::RIGHT, 300ms);
        }
    }
    fail(console, "the keyboard's cursor did not reach the English (letters) key.");
}

void submit_name(ConsoleHandle& console, ProControllerContext& context){
    const FrameCheck confirm_choices = [](const ImageViewRGB32& frame){
        return read_confirm_choices_cursor(frame).has_value();
    };
    for (size_t pressed = 1; pressed <= SUBMIT_PRESSES; pressed++){
        console.log(std::format("AutoStory intro: keyboard: + ({} of at most {}).", pressed, SUBMIT_PRESSES));
        pbf_press_button(context, BUTTON_PLUS, /*hold=*/80ms, /*release=*/500ms);
        if (wait_for_frame(console, context, confirm_choices, SUBMIT_WAIT)){
            console.log("AutoStory intro: Confirm Choices shows.");
            return;
        }
        const VideoSnapshot frame = snapshot(console, context);
        if (!frame || !is_name_keyboard(frame)){
            break;
        }
        console.log("AutoStory intro: keyboard: still open after +.");
    }
    if (!wait_for_frame(console, context, confirm_choices, 15000ms)){
        fail(console, "Confirm Choices did not show after the name's +.");
    }
}

void type_name(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    wait_for_keyboard(console, context);
    check_name_limit(console, context, choices);
    if (choices.language == NewGameLanguage::ChineseSimplified){
        switch_to_letters(console, context);
    }
    console.log(std::format("AutoStory intro: typing \"{}\".", choices.name));
    FastCodeEntry::type_on_system_keyboard(console, context, choices.name);
    submit_name(console, context);
}

} // namespace


void require_new_game_home(ConsoleHandle& console, ProControllerContext& context){
    HomeMenuDetector home(console, COLOR_RED);
    const FrameCheck on_home = [&home](const ImageViewRGB32& frame){
        return home.detect_only(frame);
    };
    if (!wait_for_frame(console, context, on_home, 2000ms)){
        //  HomeMenuDetector misses a dimmed (idle) HOME; B wakes it and does nothing else there.
        console.log("AutoStory intro: HOME not detected; pressing B once to wake a dimmed screen.");
        pbf_press_button(context, BUTTON_B, /*hold=*/80ms, /*release=*/1000ms);
    }
    if (!wait_for_frame(console, context, on_home, 2000ms)){
        throw UserSetupError(
            console,
            std::format(
                "AutoStory: 01.00a starts a new game (no save) from the Switch HOME (PLZA's tile among the first {}). "
                "It is not on screen; pressed B once to wake the screen, nothing else was pressed. {}",
                HOME_TILES_SEARCHED, NEW_GAME_BY_HAND_HINT
            )
        );
    }
    console.log("AutoStory intro: starting on the Switch HOME.");
}

std::optional<std::string> new_game_run_problem(std::span<const Pokemon::StoryCheckpoint> rows, const NewGameChoices& choices){
    for (const Pokemon::StoryCheckpoint& row : rows){
        if (!story_row_makes_new_game_choices(row)){
            continue;
        }
        if (const std::optional<std::string> problem = name_typing_problem(choices)){
            return std::format("{} {} types the name. {}", *problem, row.id, NEW_GAME_BY_HAND_HINT);
        }
    }
    return std::nullopt;
}

void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    if (const std::optional<std::string> problem = new_game_problem(choices)){
        throw UserSetupError(console, *problem);
    }
    if (const std::optional<std::string> problem = name_typing_problem(choices)){
        throw UserSetupError(console, *problem);
    }
    require_new_game_home(console, context);
    launch_plza_from_home(console, context);
    wait_for_language_list(console, context);
    pick_language(console, context, choices.language);
    pick_look(console, context, choices.look);
    type_name(console, context, choices);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
