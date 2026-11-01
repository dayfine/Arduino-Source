/*  Naming Screen
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_NamingScreenDetector.h"
#include "PokemonFRLG_NamingScreen.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The game reads new presses once per frame and starts repeating a held direction after
//  about 40 frames, so a short hold with a release of several frames is one press each.
constexpr Milliseconds PRESS_HOLD = 80ms;
constexpr Milliseconds MOVE_RELEASE = 120ms;
constexpr Milliseconds KEY_RELEASE = 200ms;
//  The page swap animation runs 32 frames (about 530 ms) with input disabled.
constexpr Milliseconds PAGE_SWAP_RELEASE = 900ms;
//  After the last character fits, the cursor animates to OK with input disabled.
constexpr Milliseconds START_RELEASE = 500ms;

void press(ProControllerContext& context, NamingAction action){
    switch (action){
    case NamingAction::UP:
        pbf_press_dpad(context, DPAD_UP, PRESS_HOLD, MOVE_RELEASE);
        return;
    case NamingAction::DOWN:
        pbf_press_dpad(context, DPAD_DOWN, PRESS_HOLD, MOVE_RELEASE);
        return;
    case NamingAction::LEFT:
        pbf_press_dpad(context, DPAD_LEFT, PRESS_HOLD, MOVE_RELEASE);
        return;
    case NamingAction::RIGHT:
        pbf_press_dpad(context, DPAD_RIGHT, PRESS_HOLD, MOVE_RELEASE);
        return;
    case NamingAction::A:
        pbf_press_button(context, BUTTON_A, PRESS_HOLD, KEY_RELEASE);
        return;
    case NamingAction::SELECT:
        pbf_press_button(context, BUTTON_MINUS, PRESS_HOLD, PAGE_SWAP_RELEASE);
        return;
    case NamingAction::START:
        pbf_press_button(context, BUTTON_PLUS, PRESS_HOLD, START_RELEASE);
        return;
    }
}

}


bool type_name(
    ConsoleHandle& console, ProControllerContext& context,
    const std::string& text, NamingScreenKind kind
){
    const std::string error = naming_text_error(text, kind);
    if (!error.empty()){
        throw UserSetupError(console, error);
    }
    const std::vector<NamingAction> actions = plan_naming_input(text, kind);

    NamingScreenWatcher naming_screen(COLOR_RED);
    context.wait_for_all_requests();
    if (wait_until(console, context, std::chrono::seconds(5), {naming_screen}) < 0){
        console.log("Naming screen not found. Not typing \"" + text + "\".", COLOR_RED);
        return false;
    }

    console.log("Typing " + std::string(naming_screen_kind_name(kind)) + " name \"" + text + "\": " + to_string(actions));
    for (NamingAction action : actions){
        press(context, action);
    }
    context.wait_for_all_requests();
    return true;
}


}
}
}
