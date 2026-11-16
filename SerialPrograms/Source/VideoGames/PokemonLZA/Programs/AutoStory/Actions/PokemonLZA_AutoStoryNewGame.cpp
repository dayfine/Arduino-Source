/*  PokemonLZA AutoStory - New game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_SystemKeyboardTyping.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryConfirmChoicesReader.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryIntroScreens.h"
#include "PokemonLZA_AutoStoryNewGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using FrameCheck = std::function<bool(const ImageViewRGB32& frame)>;

constexpr std::string_view DUMP_FOLDER = "PokemonLZA/AutoStory/Intro";

//  From the user select to the language list: about 15 s of logos, with no
//  press needed (the console, 2026-11-15, all ten languages).
constexpr Milliseconds LAUNCH_LIMIT = 60000ms;

//  HOME's tiles are in last-played order, so PLZA's is not always the
//  first: it is searched for among this many.
constexpr size_t HOME_TILES_SEARCHED = 8;
constexpr Milliseconds HOME_TILE_RELEASE = 500ms;

//  The language list's D-pad taps: each registered at this pace (the
//  console, 2026-11-15).
constexpr Milliseconds LANGUAGE_TAP_RELEASE = 600ms;

//  The look picker's stick tilts: one portrait each.
constexpr Milliseconds LOOK_TILT_HOLD = 150ms;
constexpr Milliseconds LOOK_TILT_RELEASE = 500ms;

//  The + presses after the name. One submits the European languages' and
//  Japanese's keyboard; the Chinese ones first take one to accept the typed
//  text (the button reads "Accept"), then one to submit. Each is checked.
constexpr size_t SUBMIT_PRESSES = 3;
constexpr Milliseconds SUBMIT_WAIT = 5000ms;

//  Capture mode: a frame this far (pixel RMSD) from the last saved one is
//  a new screen.
constexpr double CAPTURE_CHANGE = 4;
constexpr Milliseconds CAPTURE_EVERY = 1000ms;

VideoSnapshot snapshot(ConsoleHandle& console, ProControllerContext& context){
    context.wait_for_all_requests();
    return console.video().snapshot();
}

//  Until `check` holds on a frame, up to `timeout`, reading every 250 ms.
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

//  The look picker's presses are left stick tilts (it says "Use the Left
//  Stick"; the D-pad does not move it).
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

//  On HOME: HOME puts the cursor on the first tile (as
//  start_game_from_home() does for a game slot), then right one tile at a
//  time until the highlighted tile's title is PLZA's.
void select_plza_tile(ConsoleHandle& console, ProControllerContext& context){
    pbf_press_button(context, BUTTON_HOME, /*hold=*/160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
    for (size_t tile = 0; tile < HOME_TILES_SEARCHED; tile++){
        const VideoSnapshot frame = snapshot(console, context);
        if (frame && is_home_tile_plza(frame)){
            console.log(std::format("AutoStory intro: PLZA is HOME's tile {}.", tile));
            return;
        }
        pbf_press_dpad(context, DPAD_RIGHT, /*hold=*/80ms, HOME_TILE_RELEASE);
    }
    throw UserSetupError(
        console,
        std::format("AutoStory: Pokemon Legends: Z-A is not on HOME's first {} tiles. Play it once so its tile moves to the front.", HOME_TILES_SEARCHED)
    );
}

//  From HOME with the game closed: selects PLZA's tile, then A on it,
//  through the user select (start_game_from_home(), slot 0: the cursor
//  does not move), until the game's black screen.
void launch_from_home(ConsoleHandle& console, ProControllerContext& context){
    select_plza_tile(console, context);
    console.log("AutoStory intro: launching PLZA's HOME tile.");
    start_game_from_home(console, context, /*tolerate_update_menu=*/true, /*game_slot=*/0, /*user_slot=*/0);
}

//  After the launch, until the language list, pressing nothing (see
//  LAUNCH_LIMIT). `recorder` and `language`: capture mode, save each new
//  screen on the way.
void wait_for_language_list(
    ConsoleHandle& console, ProControllerContext& context,
    IntroFrameRecorder* recorder, NewGameLanguage language
){
    const WallClock start = current_time();
    std::optional<ImageRGB32> last_saved;
    while (current_time() - start < LAUNCH_LIMIT){
        const VideoSnapshot frame = snapshot(console, context);
        if (frame && read_language_list_cursor(frame)){
            console.log("AutoStory intro: the language list shows.");
            dump_debug_image(console, std::string(DUMP_FOLDER), "language-list", frame);
            return;
        }
        if (frame && recorder){
            const Milliseconds at = std::chrono::duration_cast<Milliseconds>(current_time() - start);
            const std::optional<double> change = last_saved
                ? std::optional<double>(ImageMatch::pixel_RMSD(*last_saved, frame))
                : std::nullopt;
            if (recorder->see(at, change)){
                const std::string file = intro_capture_file(language, std::format("{:02}", recorder->saved() - 1));
                frame->save(SCREENSHOTS_PATH() + file);
                console.log(std::format("AutoStory capture: {} ({} ms after the launch).", file, at.count()));
                last_saved = frame->copy();
            }
        }
        context.wait_for(250ms);
    }
    fail(console, std::format("the language list did not show within {} s of the launch.", LAUNCH_LIMIT.count() / 1000));
}

//  On the language list (it opens on English): one tap at a time to
//  `language`, reading the highlight after each, then A, which opens the
//  look picker with no confirmation.
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

//  The look picker's highlight once settled: two reads in a row agree,
//  150 ms apart, within 2 s.
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

//  On the look picker: it opens with no portrait highlighted, and the first
//  tilt highlights one that differs from launch to launch (the console,
//  2026-11-15: looks 1, 2, 5, 6, 7 and 8 after a tilt left). So it tilts
//  left until a highlight reads, then toward `look`, reading the highlight
//  after each tilt, and presses A only once it reads `look` (A opens the
//  name keyboard).
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

//  Until the name keyboard has shown and finished sliding in (presses
//  during that are dropped).
void wait_for_keyboard(ConsoleHandle& console, ProControllerContext& context){
    if (!wait_for_frame(console, context, is_name_keyboard, 20000ms)){
        fail(console, "the name keyboard did not show within 20 s of the look's A.");
    }
    context.wait_for(1000ms);
}

//  The keyboard's "0/N" counter (12 in the European languages, 6 in
//  Japanese, Korean and Chinese): throws UserSetupError, nothing typed, if
//  the name is longer.
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

//  Simplified Chinese's keyboard opens in Pinyin, where letters compose
//  Chinese; the bottom row's English key, under column 3, switches it to
//  letters (Traditional Chinese's opens on letters). Moves the cursor
//  there one D-pad tap at a time, reading it after each, then A.
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

//  After the name: + until Confirm Choices shows, checking the screen after
//  each press (see SUBMIT_PRESSES).
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

//  On the keyboard: checks its counter, types the name with the Switch
//  keyboard typer (which reads the cursor before each key and checks each
//  press reached the field), then submits it.
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

//  Capture mode: the screen now as `language`'s `screen`.
void save_capture(ConsoleHandle& console, ProControllerContext& context, NewGameLanguage language, std::string_view screen){
    const VideoSnapshot frame = snapshot(console, context);
    const std::string file = intro_capture_file(language, screen);
    if (frame){
        frame->save(SCREENSHOTS_PATH() + file);
        console.log(std::format("AutoStory capture: {}.", file));
    }
}

std::optional<NewGameStart> read_start(ConsoleHandle& console, const ImageViewRGB32& frame){
    if (read_confirm_choices_cursor(frame)){
        return NewGameStart::CONFIRM_CHOICES;
    }
    if (read_language_list_cursor(frame)){
        return NewGameStart::LANGUAGE_LIST;
    }
    HomeMenuDetector home(console, COLOR_RED);
    if (home.detect_only(frame)){
        return NewGameStart::HOME;
    }
    return std::nullopt;
}

} // namespace


NewGameStart require_new_game_start(ConsoleHandle& console, ProControllerContext& context){
    const WallClock deadline = current_time() + 2000ms;
    while (true){
        const VideoSnapshot frame = snapshot(console, context);
        const std::optional<NewGameStart> start = frame ? read_start(console, frame) : std::nullopt;
        if (start){
            console.log(std::format("AutoStory intro: starting on screen {} (0 HOME, 1 language list, 2 Confirm Choices).", (int)*start));
            return *start;
        }
        if (current_time() >= deadline){
            throw UserSetupError(
                console,
                "AutoStory: 01.00 starts a new game (no save) from the Switch HOME (the game closed; PLZA's tile among "
                "the first 8), from the language list, or from \"Confirm Choices\". None of them is on screen; nothing was pressed."
            );
        }
        context.wait_for(250ms);
    }
}

void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    if (const std::optional<std::string> problem = new_game_problem(choices)){
        throw UserSetupError(console, *problem);
    }
    switch (require_new_game_start(console, context)){
    case NewGameStart::CONFIRM_CHOICES:
        console.log("AutoStory intro: already on Confirm Choices; its check reads the language and name.");
        return;
    case NewGameStart::HOME:
        close_game_from_home(console, context);
        launch_from_home(console, context);
        wait_for_language_list(console, context, /*recorder=*/nullptr, choices.language);
        break;
    case NewGameStart::LANGUAGE_LIST:
        break;
    }
    pick_language(console, context, choices.language);
    pick_look(console, context, choices.look);
    type_name(console, context, choices);
}


bool IntroFrameRecorder::see(Milliseconds at, std::optional<double> from_saved){
    if (m_last_saved && at - *m_last_saved < m_every){
        return false;
    }
    if (from_saved && *from_saved <= m_change){
        return false;
    }
    m_last_saved = at;
    m_saved++;
    return true;
}

std::string intro_capture_file(NewGameLanguage language, std::string_view screen){
    return std::format("{}_{}.png", new_game_language_info(language).tag, screen);
}

void capture_intro_screens(ConsoleHandle& console, ProControllerContext& context, size_t look){
    for (size_t index = 0; index < 10; index++){
        const NewGameLanguage language = (NewGameLanguage)index;
        const NewGameLanguageInfo& info = new_game_language_info(language);
        console.log(std::format("AutoStory capture: {}: closing the game and launching it from HOME.", info.tag));
        go_home(console, context);
        close_game_from_home(console, context);
        launch_from_home(console, context);
        IntroFrameRecorder recorder(CAPTURE_CHANGE, CAPTURE_EVERY);
        wait_for_language_list(console, context, &recorder, language);
        save_capture(console, context, language, "language-list");
        pick_language(console, context, language);
        if (!wait_for_frame(console, context, is_look_picker, 20000ms)){
            fail(console, "the look picker did not show within 20 s of the language's A.");
        }
        save_capture(console, context, language, "look-picker");
        pick_look(console, context, look);
        wait_for_keyboard(console, context);
        save_capture(console, context, language, "keyboard");
        console.log(std::format("AutoStory capture: {}: {} screens before the language list.", info.tag, recorder.saved()));
    }
    go_home(console, context);
    close_game_from_home(console, context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
