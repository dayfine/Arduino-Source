/*  BDSP AutoStory - New Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_StartGameUserSelectDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameEntry.h"
#include "PokemonBDSP_AutoStory_Trace.h"
#include "PokemonBDSP_AutoStory_NewGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  The intro screens the routine tells apart. Everything else (the appearance grid,
//  cutscenes) is "other" and is advanced with A. The Switch's own screens come first:
//  BDSP's dialogue detector also fires on them.
enum class IntroScreen{
    USER_SELECT,    //  the Switch's "Who is using the software?" when the game starts
    HOME,           //  the Switch HOME menu: the game is not in front
    KEYBOARD,   //  the Switch keyboard: the player's name, or the rival's after "I'll tell you..."
    MENU,       //  the main menu: only reachable in the bedroom, so the intro is over
    DIALOG,     //  a dialogue box, including Yes/No prompts whose default is Yes
    OTHER,
};
const char* to_string(IntroScreen screen){
    switch (screen){
    case IntroScreen::USER_SELECT: return "user-select";
    case IntroScreen::HOME:     return "home";
    case IntroScreen::KEYBOARD: return "keyboard";
    case IntroScreen::MENU:     return "menu";
    case IntroScreen::DIALOG:   return "dialog";
    default:                    return "other";
    }
}
IntroScreen read_screen(ConsoleHandle& console){
    VideoSnapshot screen = console.video().snapshot();
    if (!screen){
        return IntroScreen::OTHER;
    }
    if (StartGameUserSelectDetector(console).detect(screen)){
        return IntroScreen::USER_SELECT;
    }
    if (HomeMenuDetector(console, COLOR_RED).detect(screen)){
        return IntroScreen::HOME;
    }
    if (SystemKeyboardDetector().detect(screen)){
        return IntroScreen::KEYBOARD;
    }
    if (MenuDetector().detect(screen)){
        return IntroScreen::MENU;
    }
    if (ShortDialogDetector().detect(screen)){
        return IntroScreen::DIALOG;
    }
    return IntroScreen::OTHER;
}

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

void enter_player_name(VideoStream& stream, ProControllerContext& context, AutoStoryTrace& trace, const std::string& name){
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

void pick_rival_default_name(VideoStream& stream, ProControllerContext& context, AutoStoryTrace& trace){
    //  This keyboard opened from "I'll tell you..." in the rival's name list. X (Cancel)
    //  goes back to that list with the cursor still on it; Down then A picks "Barry".
    //  The list takes a moment to appear and drops inputs until then, so if the keyboard
    //  comes back, cancel again and wait longer before Down.
    for (size_t attempt = 0; attempt < 3; attempt++){
        trace.action("Rival keyboard: cancelling it to pick the default name from the list (attempt " +
            std::to_string(attempt + 1) + "/3).");
        bool closed = false;
        for (size_t c = 0; c < 3 && !closed; c++){
            pbf_wait(context, 800ms);
            pbf_press_button(context, BUTTON_X, 80ms, 0ms);
            closed = wait_for_keyboard(stream, context, true, 2s);
        }
        if (!closed){
            trace.fail("X (Cancel) did not close the rival's keyboard.");
        }
        pbf_wait(context, Milliseconds(1500 + 1000 * attempt));
        pbf_press_dpad(context, DPAD_DOWN, 100ms, 500ms);
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        if (!wait_for_keyboard(stream, context, false, 2500ms)){
            trace.action("Picked the rival's default name.");
            return;
        }
        trace.action("The keyboard opened again: Down was dropped before the list was ready.");
    }
    trace.fail("Could not move the rival's name list off \"I'll tell you...\" in 3 attempts.");
}

}



void play_new_game_intro(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const std::string& player_name
){
    VideoStream& stream = env.console;
    AutoStoryTrace trace(stream, "Intro");

    //  No save exists yet, so a restart always lands on the title screen.
    trace.action("Restarting the game.");
    if (!reset_game_from_home(env, env.console, context, true, 1000ms)){
        trace.action("The restart did not reach the expected screen; continuing from what is shown.");
    }

    bool player_named = false;
    const WallClock deadline = current_time() + std::chrono::minutes(15);
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const IntroScreen screen = read_screen(env.console);
        trace.screen(to_string(screen));
        switch (screen){
        case IntroScreen::USER_SELECT:
            //  The cursor starts on the user that launched the game.
            pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
            continue;
        case IntroScreen::HOME:
            trace.fail("The Switch HOME menu is showing: the game is not running in front.");
        case IntroScreen::KEYBOARD:
            if (!player_named){
                enter_player_name(stream, context, trace, player_name);
                player_named = true;
            }else{
                pick_rival_default_name(stream, context, trace);
            }
            continue;
        case IntroScreen::MENU:
            trace.action("Main menu is open: the player is in control in the bedroom. Closing it.");
            pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
            context.wait_for_all_requests();
            if (read_screen(env.console) == IntroScreen::MENU){
                trace.fail("The main menu did not close.");
            }
            return;
        case IntroScreen::DIALOG:
            //  Text, or a Yes/No prompt whose default (Yes) is what the intro wants.
            pbf_press_button(context, BUTTON_A, 60ms, 300ms);
            continue;
        case IntroScreen::OTHER:
            break;
        }

        //  Not a known screen: either an intro screen whose default choice is right
        //  (language, appearance), a cutscene, or the bedroom with the player in control.
        //  Only the bedroom opens the menu, so try X first; then advance with A.
        pbf_press_button(context, BUTTON_X, 80ms, 0ms);
        context.wait_for_all_requests();
        MenuWatcher menu;
        if (wait_until(stream, context, 3s, {{menu}}) == 0){
            continue;   //  The next pass sees the menu and finishes.
        }
        if (read_screen(env.console) == IntroScreen::KEYBOARD){
            continue;
        }
        pbf_press_button(context, BUTTON_A, 60ms, 400ms);
    }
    trace.fail("The intro did not reach the bedroom within 15 minutes.");
}



}
}
}
