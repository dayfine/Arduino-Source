/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardCodeEntry.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  When an OVERWORLD mash is over: the party bar held a second, or (with a
//  quiet time) nothing busy on screen for that long.
class OverworldEnd{
public:
    explicit OverworldEnd(std::optional<Milliseconds> quiet)
        : m_quiet(quiet)
        , m_last_busy(current_time())
    {}
    bool reached(ConsoleHandle& console, const Pokemon::ScreenEvents& events, SceneMashAction action, WallClock now){
        if (events.any_of({
            Pokemon::ScreenEvent::DIALOG_BOX, Pokemon::ScreenEvent::CHOICE_BOX,
            Pokemon::ScreenEvent::BATTLE_STARTED, Pokemon::ScreenEvent::FADE_BLACK,
            Pokemon::ScreenEvent::FADE_WHITE,
        })){
            m_last_busy = now;
        }
        if (action != SceneMashAction::DONE){
            m_can_move_since.reset();
        }else if (!m_can_move_since){
            m_can_move_since = now;
        }
        if (m_can_move_since && now - *m_can_move_since >= 1000ms){
            return true;
        }
        if (m_quiet && now - m_last_busy >= *m_quiet){
            console.log(std::format("mash_scene(): quiet for {} ms, taking it as the overworld.", m_quiet->count()));
            return true;
        }
        return false;
    }

private:
    std::optional<Milliseconds> m_quiet;
    WallClock m_last_busy;
    std::optional<WallClock> m_can_move_since;
};

void press_for(ProControllerContext& context, SceneMashAction action, Button text_button){
    switch (action){
    case SceneMashAction::PRESS_TEXT:
        pbf_press_button(context, text_button, 160ms, 240ms);
        return;
    case SceneMashAction::PRESS_A:
        pbf_press_button(context, BUTTON_A, 160ms, 240ms);
        return;
    case SceneMashAction::PRESS_B:
        pbf_press_button(context, BUTTON_B, 160ms, 240ms);
        return;
    case SceneMashAction::WAIT:
    case SceneMashAction::DONE:
        context.wait_for(250ms);
        return;
    }
}

} // namespace

void mash_scene(
    ConsoleHandle& console, ProControllerContext& context,
    SceneMashGoal goal, Button text_button, Milliseconds timeout,
    std::optional<Milliseconds> quiet
){
    PlzaScreenObserver observer;
    OverworldEnd overworld(quiet);
    const WallClock start = current_time();
    while (current_time() - start < timeout){
        context.wait_for_all_requests();
        const WallClock now = current_time();
        const Pokemon::ScreenEvents events = observer.observe(console.video().snapshot(), now);
        const SceneMashAction action = scene_mash_action(events, goal, text_button == BUTTON_B);
        if (goal == SceneMashGoal::OVERWORLD){
            if (overworld.reached(console, events, action, now)){
                return;
            }
        }else if (action == SceneMashAction::DONE){
            return;
        }
        press_for(context, action, text_button);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("mash_scene(): the goal did not show within {} ms.", timeout.count()),
        console
    );
}


void new_game_language(ConsoleHandle&, ProControllerContext& context){
    //  TODO: a title screen and a language list detector; the waits are guesses.
    pbf_press_button(context, BUTTON_A, 160ms, 5000ms);
    pbf_press_button(context, BUTTON_A, 160ms, 5000ms);
    context.wait_for_all_requests();
}

void pick_first_look(ConsoleHandle&, ProControllerContext& context){
    //  TODO: the shared MenuCursor (BDSP #305) once it reaches this branch,
    //  to read the highlight instead of relying on the edges clamping.
    pbf_press_dpad(context, DPAD_UP, 160ms, 400ms);
    for (int i = 0; i < 3; i++){
        pbf_press_dpad(context, DPAD_LEFT, 160ms, 400ms);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 3000ms);
    context.wait_for_all_requests();
}

void type_player_name(ConsoleHandle& console, ProControllerContext& context, const std::string& name){
    //  TODO: a keyboard detector instead of the wait.
    context.wait_for(3000ms);
    //  The system keyboard types lowercase. Its Shift (stick click) caps
    //  the next letter only, so it goes right before the name ("K" and "k"
    //  are the same key to keyboard_enter_code).
    pbf_press_button(context, BUTTON_LCLICK, 160ms, 400ms);
    context.wait_for_all_requests();
    FastCodeEntry::keyboard_enter_code(console, context, true, KeyboardLayout::QWERTY, name, true);
    context.wait_for(1000ms);
    //  PLUS is the keyboard's OK. "Confirm Choices" opens on "Change hair
    //  color"; "Ready!" is two rows down (console B, 2026-11-10).
    pbf_press_button(context, BUTTON_PLUS, 160ms, 2500ms);
    pbf_press_dpad(context, DPAD_DOWN, 160ms, 400ms);
    pbf_press_dpad(context, DPAD_DOWN, 160ms, 400ms);
    pbf_press_button(context, BUTTON_A, 160ms, 2000ms);
    context.wait_for_all_requests();
}

void climb_ladder(ConsoleHandle&, ProControllerContext& context){
    pbf_move_left_joystick(context, {1, 0}, 600ms, 300ms);
    pbf_move_left_joystick(context, {0, 1}, 150ms, 300ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    pbf_move_left_joystick(context, {0, 1}, 5000ms, 500ms);
    context.wait_for_all_requests();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
