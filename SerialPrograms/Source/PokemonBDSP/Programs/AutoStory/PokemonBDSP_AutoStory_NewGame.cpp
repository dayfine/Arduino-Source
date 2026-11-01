/*  BDSP AutoStory - New Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
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
    if (ShortDialogDetector().detect(screen)){
        return "dialog";
    }
    //  Language, cutscenes, fades, the bedroom with no menu open.
    return "other";
}

IntroStep decide_intro_step(const std::string& screen, const IntroProgress& progress){
    if (screen == "user-select")        return IntroStep::SELECT_USER;
    if (screen == "home")               return IntroStep::FAIL_NOT_IN_GAME;
    if (screen == "keyboard"){
        return progress.player_named ? IntroStep::CANCEL_RIVAL_KEYBOARD : IntroStep::ENTER_PLAYER_NAME;
    }
    if (screen == "rival-list-tell")    return IntroStep::MOVE_TO_PRESET;
    if (screen == "rival-list-preset")  return IntroStep::PICK_PRESET;
    if (screen == "menu")               return IntroStep::CLOSE_MENU_AND_FINISH;
    if (screen == "dialog")             return IntroStep::ADVANCE;
    //  The bedroom comes only after the rival is named. Before that, every unknown
    //  screen (language, the appearance preview, cutscenes) takes A.
    return progress.rival_named ? IntroStep::PROBE_FOR_MENU : IntroStep::ADVANCE;
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

}



void play_new_game_intro(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const std::string& player_name
){
    VideoStream& stream = env.console;
    StepTrace trace(stream, "Intro");

    //  Only close and reopen the game: the states below take it from the user select
    //  on. reset_game_from_home() waits for a loaded save's black screen to end, and a
    //  new game's first screen is a dialog on black, so it would sit there until its
    //  timeout.
    trace.action("Restarting the game.");
    go_home(env.console, context);
    from_home_close_and_reopen_game(env.console, context, true);

    IntroProgress progress;
    size_t x_without_menu = 0;
    auto press = [&](Button button){
        pbf_press_button(context, button, 80ms, 0ms);
    };

    //  Carries out what decide_intro_step() chose for `screen`.
    auto act = [&](const char* screen){
        switch (decide_intro_step(screen, progress)){
        case IntroStep::SELECT_USER:
            //  The cursor starts on the user that launched the game.
            press(BUTTON_A);
            pbf_wait(context, 1000ms);
            return;
        case IntroStep::FAIL_NOT_IN_GAME:
            trace.fail("The Switch HOME menu is showing: the game is not running in front.");
        case IntroStep::ENTER_PLAYER_NAME:
            enter_player_name(stream, context, trace, player_name);
            progress.player_named = true;
            return;
        case IntroStep::CANCEL_RIVAL_KEYBOARD:
            //  Opened from "I'll tell you..."; inputs during the slide-in are dropped.
            pbf_wait(context, 800ms);
            press(BUTTON_X);
            return;
        case IntroStep::MOVE_TO_PRESET:
            pbf_press_dpad(context, DPAD_DOWN, 100ms, 300ms);
            return;
        case IntroStep::PICK_PRESET:
            press(BUTTON_A);
            pbf_wait(context, 500ms);
            progress.rival_named = true;
            return;
        case IntroStep::CLOSE_MENU_AND_FINISH:{
            trace.action("Main menu is open: the player is in control in the bedroom. Closing it.");
            press(BUTTON_B);
            pbf_wait(context, 1000ms);
            context.wait_for_all_requests();
            VideoSnapshot screen_now = stream.video().snapshot();
            if (!screen_now || MenuDetector().detect(screen_now)){
                trace.fail("The main menu did not close.");
            }
            return;
        }
        case IntroStep::ADVANCE:
            pbf_press_button(context, BUTTON_A, 60ms, 400ms);
            return;
        case IntroStep::PROBE_FOR_MENU:{
            context.wait_for_all_requests();
            VideoSnapshot before = stream.video().snapshot();
            press(BUTTON_X);
            context.wait_for_all_requests();
            MenuWatcher menu;
            if (wait_until(stream, context, 3s, {{menu}}) == 0){
                return;     //  The next read sees the menu and finishes.
            }
            VideoSnapshot after = stream.video().snapshot();
            const bool changed = !before || !after || ImageMatch::pixel_RMSD(before, after) > 10;
            if (!advance_after_probe(changed)){
                trace.action("X changed the screen but no known menu opened; not pressing A.");
                if (++x_without_menu >= 10){
                    trace.fail("X keeps changing the screen without a known menu opening.");
                }
                return;
            }
            x_without_menu = 0;
            pbf_press_button(context, BUTTON_A, 60ms, 400ms);
            return;
        }
        }
    };

    auto screen_is = [&](const ImageViewRGB32& screen, const char* name){
        return classify_intro_screen(&env.console, screen) == name;
    };
    //  The machine's states: the screens classify_intro_screen() tells apart, with the
    //  keyboard split by whose name it asks for. Each state's step is act().
    const std::vector<ScreenState> states{
        {"user-select",
            [&](const ImageViewRGB32& s){ return screen_is(s, "user-select"); },
            [&]{ act("user-select"); }, {}, 3},
        {"home",
            [&](const ImageViewRGB32& s){ return screen_is(s, "home"); },
            [&]{ act("home"); }, {}, 1},
        {"player-keyboard",
            [&](const ImageViewRGB32& s){ return !progress.player_named && screen_is(s, "keyboard"); },
            [&]{ act("keyboard"); }, {"dialog", "other"}, 1},
        {"rival-keyboard",
            [&](const ImageViewRGB32& s){ return progress.player_named && screen_is(s, "keyboard"); },
            [&]{ act("keyboard"); }, {"rival-list-tell", "dialog", "other"}, 3},
        {"rival-list-tell",
            [&](const ImageViewRGB32& s){ return screen_is(s, "rival-list-tell"); },
            [&]{ act("rival-list-tell"); }, {"rival-list-preset", "other"}, 6},
        {"rival-list-preset",
            [&](const ImageViewRGB32& s){ return screen_is(s, "rival-list-preset"); },
            [&]{ act("rival-list-preset"); }, {"dialog", "other"}, 3},
        {"menu",
            [&](const ImageViewRGB32& s){ return screen_is(s, "menu"); },
            [&]{ act("menu"); }, {}, 2, true},
        {"dialog",
            [&](const ImageViewRGB32& s){ return screen_is(s, "dialog"); },
            [&]{ act("dialog"); }, {}, 400},
        {"other",
            [](const ImageViewRGB32&){ return true; },
            [&]{ act("other"); }, {}, 300, false, 60},
    };
    run_screen_machine(stream, context, trace, states, std::chrono::minutes(15));
    trace.action("Intro done.");
}



}
}
}
