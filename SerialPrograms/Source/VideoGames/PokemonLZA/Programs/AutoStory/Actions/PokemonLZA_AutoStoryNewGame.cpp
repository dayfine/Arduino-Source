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
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryConfirmChoicesReader.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryIntroScreens.h"
#include "PokemonLZA_AutoStoryNameKeys.h"
#include "PokemonLZA_AutoStoryNewGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using FrameCheck = std::function<bool(const ImageViewRGB32& frame)>;

constexpr std::string_view DUMP_FOLDER = "PokemonLZA/AutoStory/Intro";

//  From the launch to the language list (the boot, logos, prompts): no
//  press for LAUNCH_QUIET, then A at most LAUNCH_A_PRESSES times,
//  LAUNCH_A_EVERY apart, on any screen that is not the list, until
//  LAUNCH_LIMIT. With no save the game goes straight to the list (the
//  console, 2026-11-09); the A presses are for a prompt in between.
constexpr Milliseconds LAUNCH_QUIET = 30000ms;
constexpr Milliseconds LAUNCH_A_EVERY = 6000ms;
constexpr size_t LAUNCH_A_PRESSES = 5;
constexpr Milliseconds LAUNCH_LIMIT = 90000ms;

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

void press(ConsoleHandle& console, ProControllerContext& context, IntroKey key, Milliseconds release){
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
    case IntroKey::SHIFT:
        pbf_press_button(context, BUTTON_LCLICK, /*hold=*/80ms, release);
        break;
    case IntroKey::PLUS:
        console.log("AutoStory intro: keyboard: PLUS (OK).");
        pbf_press_button(context, BUTTON_PLUS, /*hold=*/80ms, release);
        break;
    }
}

//  The look picker's presses are left stick tilts (it says "Use the Left
//  Stick"; the D-pad does not move it).
void tilt(ProControllerContext& context, IntroKey key){
    switch (key){
    case IntroKey::UP:
        pbf_move_left_joystick(context, /*position=*/{0, +1}, /*hold=*/200ms, /*release=*/500ms);
        break;
    case IntroKey::DOWN:
        pbf_move_left_joystick(context, /*position=*/{0, -1}, /*hold=*/200ms, /*release=*/500ms);
        break;
    case IntroKey::LEFT:
        pbf_move_left_joystick(context, /*position=*/{-1, 0}, /*hold=*/200ms, /*release=*/500ms);
        break;
    case IntroKey::RIGHT:
        pbf_move_left_joystick(context, /*position=*/{+1, 0}, /*hold=*/200ms, /*release=*/500ms);
        break;
    default:
        break;
    }
}

//  From HOME with the game closed: A on the selected tile, through the
//  user select (start_game_from_home(), slot 0: the cursor does not move),
//  until the game's black screen.
void launch_from_home(ConsoleHandle& console, ProControllerContext& context){
    console.log("AutoStory intro: launching the selected HOME tile.");
    start_game_from_home(console, context, /*tolerate_update_menu=*/true, /*game_slot=*/0, /*user_slot=*/0);
}

//  After the launch, until the language list (tolerant: see LAUNCH_QUIET).
//  `recorder` and `language`: capture mode, save each new screen on the way.
void wait_for_language_list(
    ConsoleHandle& console, ProControllerContext& context,
    IntroFrameRecorder* recorder, NewGameLanguage language
){
    const WallClock start = current_time();
    size_t a_presses = 0;
    WallClock next_a = start + LAUNCH_QUIET;
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
        if (current_time() >= next_a && a_presses < LAUNCH_A_PRESSES){
            a_presses++;
            console.log(std::format("AutoStory intro: no language list yet; A ({} of {}).", a_presses, LAUNCH_A_PRESSES));
            pbf_press_button(context, BUTTON_A, /*hold=*/80ms, /*release=*/500ms);
            next_a = current_time() + LAUNCH_A_EVERY;
        }
        context.wait_for(250ms);
    }
    fail(console, std::format("the language list did not show within {} s of the launch.", LAUNCH_LIMIT.count() / 1000));
}

//  On the language list: one press at a time to `language`, reading the
//  highlight after each, then A.
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
            press(console, context, IntroKey::A, 500ms);
            return;
        }
        const std::optional<std::vector<IntroKey>> moves = language_list_moves(*cursor, info.list_entry);
        if (!moves || moves->empty()){
            break;
        }
        press(console, context, moves->front(), 400ms);
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

//  On the look picker (it opens with no portrait highlighted; a tilt
//  brings the highlight up): tilts toward `look`, reading the highlight
//  after each, then A.
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
            tilt(context, IntroKey::UP);
            continue;
        }
        if (*cursor == look){
            console.log(std::format("AutoStory intro: look {}: A.", look));
            press(console, context, IntroKey::A, 500ms);
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

//  On the keyboard: checks its counter, then types the name and OK.
void type_name(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    wait_for_keyboard(console, context);
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
    const std::optional<std::vector<IntroKey>> presses = name_key_presses(choices.name, choices.language);
    if (!presses){
        throw UserSetupError(console, std::format("AutoStory: the player name \"{}\" cannot be typed.", choices.name));
    }
    console.log(std::format("AutoStory intro: typing \"{}\" ({} presses).", choices.name, presses->size()));
    for (const IntroKey key : *presses){
        press(console, context, key, key == IntroKey::A ? 400ms : 250ms);
    }
    context.wait_for_all_requests();
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
                "AutoStory: 01.00 starts a new game (no save) from the Switch HOME with PLZA's tile selected and the game "
                "closed, from the language list, or from \"Confirm Choices\". None of them is on screen; nothing was pressed."
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
    if (!wait_for_frame(console, context, [](const ImageViewRGB32& frame){ return read_confirm_choices_cursor(frame).has_value(); }, 15000ms)){
        fail(console, "Confirm Choices did not show within 15 s of the name's OK.");
    }
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
