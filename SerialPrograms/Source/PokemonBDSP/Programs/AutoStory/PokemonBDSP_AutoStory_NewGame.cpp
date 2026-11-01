/*  BDSP AutoStory - New Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/ScreenMachine/ScreenMachine.h"
#include "CommonTools/ScreenMachine/StepTrace.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_StartGameUserSelectDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_NameListDetector.h"
#include "PokemonBDSP_AutoStory_NewGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  BDSP's dialogue box. ShortDialogDetector also wants something busy at the
//  box's right end, which the "more text" arrow gives; a speaker's last line
//  has no arrow and read as "other" (r2, 2026-11-01: Rowan's "Now, go on, and
//  leap into the world of Pokemon!"). Without the arrow: white at both ends
//  and along the bottom of the text area, and the box's edge on the left.
bool intro_dialog_showing(const ImageViewRGB32& screen){
    if (ShortDialogDetector().detect(screen)){
        return true;
    }
    const ImageStats left_white = image_stats(extract_box_reference(screen, ImageFloatBox(0.21, 0.835, 0.008, 0.12)));
    const ImageStats right_white = image_stats(extract_box_reference(screen, ImageFloatBox(0.785, 0.835, 0.008, 0.12)));
    const ImageStats bottom = image_stats(extract_box_reference(screen, ImageFloatBox(0.50, 0.91, 0.29, 0.05)));
    const ImageStats left_edge = image_stats(extract_box_reference(screen, ImageFloatBox(0.18, 0.835, 0.02, 0.12)));
    return is_white(left_white) && is_white(right_white) && is_white(bottom) && left_edge.stddev.sum() >= 30;
}

//  The part of the screen the probe compares before and after X: the
//  dialogue box's area. The intro's background sparkles everywhere above
//  it, which read as X having changed the screen (r2).
const ImageFloatBox PROBE_AREA(0.15, 0.75, 0.70, 0.25);

}


std::string classify_intro_screen(ConsoleHandle* console, const ImageViewRGB32& screen){
    //  The Switch's own screens come first: BDSP's dialogue detector also fires on them.
    if (console != nullptr && StartGameUserSelectDetector(*console).detect(screen)){
        return "user-select";
    }
    if (console != nullptr && HomeMenuDetector(*console, COLOR_RED).detect(screen)){
        return "home";
    }
    if (SystemKeyboardDetector().detect(screen)){
        return "keyboard";
    }
    switch (NameListDetector().selected_row(screen)){
    case -1:
        break;
    case 0:
        return "rival-list-tell";
    default:
        return "rival-list-preset";
    }
    if (MenuDetector().detect(screen)){
        return "menu";
    }
    //  Includes the appearance grid, whose prompt sits in a dialogue box.
    if (intro_dialog_showing(screen)){
        return "dialog";
    }
    //  Language, cutscenes, fades, the bedroom with no menu open.
    return "other";
}

IntroStep decide_intro_step(const std::string& screen, const IntroProgress& progress){
    if (screen == "user-select"){
        return IntroStep::SELECT_USER;
    }
    if (screen == "home"){
        return IntroStep::FAIL_NOT_IN_GAME;
    }
    if (screen == "keyboard"){
        return progress.player_named ? IntroStep::CANCEL_RIVAL_KEYBOARD : IntroStep::ENTER_PLAYER_NAME;
    }
    if (screen == "rival-list-tell"){
        return IntroStep::MOVE_TO_PRESET;
    }
    if (screen == "rival-list-preset"){
        return IntroStep::PICK_PRESET;
    }
    if (screen == "menu"){
        return IntroStep::CLOSE_MENU_AND_FINISH;
    }
    if (screen == "dialog"){
        return IntroStep::ADVANCE;
    }
    //  The bedroom comes only after the rival is named. Before that, every unknown
    //  screen (language, the appearance preview, cutscenes) takes A, but only so
    //  many before a screen only BDSP shows: past those it is another game.
    if (progress.rival_named){
        return IntroStep::PROBE_FOR_MENU;
    }
    if (!progress.game_confirmed && progress.unconfirmed_presses >= INTRO_PRESSES_BEFORE_BDSP){
        return IntroStep::FAIL_NOT_BDSP;
    }
    return IntroStep::ADVANCE;
}
bool intro_screen_confirms_bdsp(const std::string& screen){
    return screen == "dialog" || screen == "rival-list-tell" || screen == "rival-list-preset" || screen == "menu";
}

bool advance_after_probe(bool screen_changed){
    return !screen_changed;
}

size_t run_intro_screen_tests(Logger& logger, const std::string& folder){
    size_t passed = 0;
    size_t failed = 0;
    for (const Filesystem::Path& dir : Filesystem::list_directory(Filesystem::Path(folder))){
        if (!Filesystem::is_directory(dir)){
            continue;
        }
        const std::string expected = dir.filename().string();
        for (const Filesystem::Path& file : Filesystem::list_directory(dir)){
            if (file.extension().string() != ".png"){
                continue;
            }
            const ImageRGB32 image(file.string());
            //  1080p and 720p. At 960x540 the keyboard's and the name list's checks
            //  do not hold (blurred key text, an arrow below the object size limit).
            for (size_t width : {size_t(1920), size_t(1280)}){
                const std::string actual = classify_intro_screen(nullptr, image.scale_to(width, width * 9 / 16));
                if (actual == expected){
                    passed++;
                    continue;
                }
                failed++;
                logger.log("[Intro tests] FAIL " + expected + "/" + file.filename().string() +
                    " at " + std::to_string(width) + "px: read as " + actual, COLOR_RED);
            }
        }
    }
    logger.log("[Intro tests] " + std::to_string(passed) + " passed, " + std::to_string(failed) + " failed.",
        failed == 0 ? COLOR_BLUE : COLOR_RED);
    return failed;
}


namespace{

//  Waits until the keyboard is gone (or, with `gone` false, has appeared).
bool wait_for_keyboard(VideoStream& stream, ProControllerContext& context, bool gone, Milliseconds timeout){
    context.wait_for_all_requests();
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        VideoSnapshot screen = stream.video().snapshot();
        if (screen && SystemKeyboardDetector().detect(screen) != gone){
            return true;
        }
        context.wait_for(100ms);
    }
    return false;
}

void enter_player_name(VideoStream& stream, ProControllerContext& context, StepTrace& trace, const std::string& name){
    //  Let the keyboard finish sliding in: inputs during that are dropped.
    pbf_wait(context, 800ms);
    context.wait_for_all_requests();
    //  B deletes a character, and closes the keyboard on an empty field, so delete only
    //  while there is text (e.g. after a retry).
    for (size_t c = 0; c < 14; c++){
        VideoSnapshot screen = stream.video().snapshot();
        if (!screen || !system_keyboard_field_has_text(screen)){
            break;
        }
        trace.action("Name field holds text; deleting a character.");
        pbf_press_button(context, BUTTON_B, 60ms, 250ms);
        context.wait_for_all_requests();
    }
    trace.action("Typing the player's name: " + name);
    type_on_system_keyboard(stream, context, name);
    pbf_press_button(context, BUTTON_PLUS, 80ms, 0ms);
    if (!wait_for_keyboard(stream, context, true, 5s)){
        trace.fail("The keyboard stayed open after PLUS (OK).");
    }
    trace.action("Name confirmed.");
}

//  A run of the intro: what its steps act on and how far it has got.
struct IntroRun{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    StepTrace& trace;
    const std::string& player_name;
    IntroProgress progress;
    size_t x_without_menu = 0;
};

void press(IntroRun& run, Button button){
    pbf_press_button(run.context, button, 80ms, 0ms);
}

void close_menu_and_finish(IntroRun& run){
    run.trace.action("Main menu is open: the player is in control in the bedroom. Closing it.");
    press(run, BUTTON_B);
    pbf_wait(run.context, 1000ms);
    run.context.wait_for_all_requests();
    VideoSnapshot screen_now = run.env.console.video().snapshot();
    if (!screen_now || MenuDetector().detect(screen_now)){
        run.trace.fail("The main menu did not close.");
    }
}

void probe_for_menu(IntroRun& run){
    VideoStream& stream = run.env.console;
    run.context.wait_for_all_requests();
    VideoSnapshot before = stream.video().snapshot();
    press(run, BUTTON_X);
    run.context.wait_for_all_requests();
    MenuWatcher menu;
    if (wait_until(stream, run.context, 3s, {{menu}}) == 0){
        return;     //  The next read sees the menu and finishes.
    }
    VideoSnapshot after = stream.video().snapshot();
    const bool changed = !before || !after || ImageMatch::pixel_RMSD(
        extract_box_reference(*before.frame, PROBE_AREA), extract_box_reference(*after.frame, PROBE_AREA)
    ) > 10;
    if (!advance_after_probe(changed)){
        run.trace.action("X changed the screen but no known menu opened; not pressing A.");
        if (++run.x_without_menu >= 10){
            run.trace.fail("X keeps changing the screen without a known menu opening.");
        }
        return;
    }
    run.x_without_menu = 0;
    pbf_press_button(run.context, BUTTON_A, 60ms, 400ms);
}

//  Carries out what decide_intro_step() chose for `screen`.
void act(IntroRun& run, const char* screen){
    if (intro_screen_confirms_bdsp(screen)){
        run.progress.game_confirmed = true;
    }
    switch (decide_intro_step(screen, run.progress)){
    case IntroStep::SELECT_USER:
        //  The cursor starts on the user that launched the game.
        press(run, BUTTON_A);
        pbf_wait(run.context, 1000ms);
        return;
    case IntroStep::FAIL_NOT_IN_GAME:
        run.trace.fail("The Switch HOME menu is showing: the game is not running in front.");
    case IntroStep::FAIL_NOT_BDSP:
        run.trace.fail("The game in front is not BDSP: no BDSP screen after " +
            std::to_string(run.progress.unconfirmed_presses) + " presses on unknown screens.");
    case IntroStep::ENTER_PLAYER_NAME:
        enter_player_name(run.env.console, run.context, run.trace, run.player_name);
        run.progress.player_named = true;
        return;
    case IntroStep::CANCEL_RIVAL_KEYBOARD:
        //  Opened from "I'll tell you..."; inputs during the slide-in are dropped.
        pbf_wait(run.context, 800ms);
        press(run, BUTTON_X);
        return;
    case IntroStep::MOVE_TO_PRESET:
        pbf_press_dpad(run.context, DPAD_DOWN, 100ms, 300ms);
        return;
    case IntroStep::PICK_PRESET:
        press(run, BUTTON_A);
        pbf_wait(run.context, 500ms);
        run.progress.rival_named = true;
        return;
    case IntroStep::CLOSE_MENU_AND_FINISH:
        close_menu_and_finish(run);
        return;
    case IntroStep::ADVANCE:
        if (!run.progress.game_confirmed){
            run.progress.unconfirmed_presses++;
        }
        pbf_press_button(run.context, BUTTON_A, 60ms, 400ms);
        return;
    case IntroStep::PROBE_FOR_MENU:
        probe_for_menu(run);
        return;
    }
}

//  The machine's states: the screens classify_intro_screen() tells apart, with
//  the keyboard split by whose name it asks for. Each state's step is act().
std::vector<ScreenState> intro_states(IntroRun& run){
    auto screen_is = [&run](const ImageViewRGB32& screen, const char* name){
        return classify_intro_screen(&run.env.console, screen) == name;
    };
    return {
        {"user-select",
            [=](const ImageViewRGB32& s){ return screen_is(s, "user-select"); },
            [&run]{ act(run, "user-select"); }, {}, 3},
        {"home",
            [=](const ImageViewRGB32& s){ return screen_is(s, "home"); },
            [&run]{ act(run, "home"); }, {}, 1},
        {"player-keyboard",
            [=, &run](const ImageViewRGB32& s){ return !run.progress.player_named && screen_is(s, "keyboard"); },
            [&run]{ act(run, "keyboard"); }, {"dialog", "other"}, 1},
        {"rival-keyboard",
            [=, &run](const ImageViewRGB32& s){ return run.progress.player_named && screen_is(s, "keyboard"); },
            [&run]{ act(run, "keyboard"); }, {"rival-list-tell", "dialog", "other"}, 3},
        {"rival-list-tell",
            [=](const ImageViewRGB32& s){ return screen_is(s, "rival-list-tell"); },
            [&run]{ act(run, "rival-list-tell"); }, {"rival-list-preset", "other"}, 6},
        {"rival-list-preset",
            [=](const ImageViewRGB32& s){ return screen_is(s, "rival-list-preset"); },
            [&run]{ act(run, "rival-list-preset"); }, {"dialog", "other"}, 3},
        {"menu",
            [=](const ImageViewRGB32& s){ return screen_is(s, "menu"); },
            [&run]{ act(run, "menu"); }, {}, 2, true},
        {"dialog",
            [=](const ImageViewRGB32& s){ return screen_is(s, "dialog"); },
            [&run]{ act(run, "dialog"); }, {}, 400},
        {"other",
            [](const ImageViewRGB32&){ return true; },
            [&run]{ act(run, "other"); }, {}, 300, false, 60},
    };
}

}



void play_new_game_intro(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const std::string& player_name
){
    VideoStream& stream = env.console;
    StepTrace trace(stream, "Intro");

    //  The restart below reopens the game HOME's cursor is on, which is the
    //  game running in front only while it runs: with none running (r1,
    //  2026-11-01), closing it fails, HOME is pressed again, the cursor goes
    //  to the first tile, and another game was launched. So BDSP must be
    //  running in front when the intro starts.
    context.wait_for_all_requests();
    VideoSnapshot start = stream.video().snapshot();
    if (start && HomeMenuDetector(env.console, COLOR_RED).detect(start)){
        throw UserSetupError(env.console,
            "Start a new game with BDSP running in front, not from the Switch's HOME menu: "
            "the intro restarts the game in front.");
    }

    //  Only close and reopen the game: the states below take it from the user select
    //  on. reset_game_from_home() waits for a loaded save's black screen to end, and a
    //  new game's first screen is a dialog on black, so it would sit there until its
    //  timeout.
    trace.action("Restarting the game.");
    go_home(env.console, context);
    from_home_close_and_reopen_game(env.console, context, true);

    IntroRun run{env, context, trace, player_name, {}, 0};
    const std::vector<ScreenState> states = intro_states(run);
    run_screen_machine(stream, context, trace, states, std::chrono::minutes(15));
    trace.action("Intro done.");
}



}
}
}
