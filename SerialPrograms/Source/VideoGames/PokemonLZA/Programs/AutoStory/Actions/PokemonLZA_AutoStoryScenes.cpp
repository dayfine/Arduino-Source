/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <format>
#include <optional>
#include <string_view>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_SystemKeyboardTyping.h"
#include "PokemonLZA_AutoStoryIntro.h"
#include "PokemonLZA_AutoStoryIntroScreens.h"
#include "PokemonLZA_AutoStoryLanguageList.h"
#include "PokemonLZA_AutoStoryLaunch.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  When an OVERWORLD mash is over: the party bar held a second, or (with a
//  quiet time) nothing busy on screen for that long once the scene's
//  minimum length has passed.
class OverworldEnd{
public:
    OverworldEnd(std::optional<Milliseconds> quiet, Milliseconds min_length)
        : m_quiet(quiet)
        , m_quiet_from(current_time() + min_length)
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
        if (m_quiet && now >= m_quiet_from && now - m_last_busy >= *m_quiet){
            console.log(std::format("mash_scene(): quiet for {} ms, taking it as the overworld.", m_quiet->count()));
            return true;
        }
        return false;
    }

private:
    std::optional<Milliseconds> m_quiet;
    WallClock m_quiet_from;
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

//  Logs the frame's events whenever they change (the still-screen flag left
//  out), so a run's log shows each scene's fades, boxes and party bar.
void log_event_change(ConsoleHandle& console, const Pokemon::ScreenEvents& events, Pokemon::ScreenEvents& last){
    Pokemon::ScreenEvents shown = events;
    shown.set(Pokemon::ScreenEvent::SCREEN_STILL, false);
    if (shown == last){
        return;
    }
    last = shown;
    console.log(std::format("mash_scene(): screen now: {}", Pokemon::describe(shown)));
}

} // namespace

//  A tutorial card (Map & Missions, Main Missions, Pokedex, ...): a white
//  panel in the middle over a dark, blurred, even screen. Only A turns its
//  pages (B turns back), so a B mash never closes it. Measured on 5 cards
//  and 9 other screens (map, Pokedex, dialogs, overworld, night) on
//  console B, 2026-11-10, at full resolution: cards' panel 241-246 bright
//  (its text spreads it to 40-48) and edges 61-76 with spread under 7; the
//  others' panel at most 185, or edges brighter or busy.
bool tutorial_card_shows(const ImageViewRGB32& frame){
    const ImageStats panel = image_stats(extract_box_reference(frame, ImageFloatBox(0.32, 0.62, 0.36, 0.20)));
    if (panel.average.sum() / 3 < 225){
        return false;
    }
    for (const ImageFloatBox& side : {ImageFloatBox(0.03, 0.30, 0.15, 0.40), ImageFloatBox(0.82, 0.30, 0.15, 0.40)}){
        const ImageStats edge = image_stats(extract_box_reference(frame, side));
        if (edge.average.sum() / 3 > 100 || edge.stddev.sum() / 3 > 12){
            return false;
        }
    }
    return true;
}

void mash_scene(
    ConsoleHandle& console, ProControllerContext& context,
    SceneMashGoal goal, Button text_button, Milliseconds timeout,
    std::optional<Milliseconds> quiet, Milliseconds min_length
){
    PlzaScreenObserver observer;
    OverworldEnd overworld(quiet, min_length);
    Pokemon::ScreenEvents last_events;
    const WallClock start = current_time();
    while (current_time() - start < timeout){
        context.wait_for_all_requests();
        const WallClock now = current_time();
        const VideoSnapshot frame = console.video().snapshot();
        if (frame && tutorial_card_shows(frame)){
            console.log("mash_scene(): a tutorial card: A.");
            pbf_press_button(context, BUTTON_A, 160ms, 840ms);
            continue;
        }
        const Pokemon::ScreenEvents events = observer.observe(frame, now);
        log_event_change(console, events, last_events);
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


namespace{

//  Saves the current frame to DebugDumps as intro screen `screen`.
void dump_intro_frame(ConsoleHandle& console, ProControllerContext& context, std::string_view screen){
    context.wait_for_all_requests();
    const VideoSnapshot frame = console.video().snapshot();
    if (frame){
        dump_debug_image(console, "PokemonLZA/AutoStory/Intro", std::string(screen), frame);
    }
}

void press_intro_keys(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<IntroKey>& presses, Milliseconds after_last
){
    for (size_t i = 0; i < presses.size(); i++){
        const IntroKey key = presses[i];
        const Milliseconds release = i + 1 == presses.size() ? after_last : 400ms;
        switch (key){
        case IntroKey::UP:
            console.log("AutoStory intro: UP.");
            pbf_press_dpad(context, DPAD_UP, 160ms, release);
            break;
        case IntroKey::DOWN:
            console.log("AutoStory intro: DOWN.");
            pbf_press_dpad(context, DPAD_DOWN, 160ms, release);
            break;
        case IntroKey::LEFT:
            console.log("AutoStory intro: LEFT.");
            pbf_press_dpad(context, DPAD_LEFT, 160ms, release);
            break;
        case IntroKey::RIGHT:
            console.log("AutoStory intro: RIGHT.");
            pbf_press_dpad(context, DPAD_RIGHT, 160ms, release);
            break;
        case IntroKey::A:
            console.log("AutoStory intro: A.");
            pbf_press_button(context, BUTTON_A, 160ms, release);
            break;
        }
    }
    context.wait_for_all_requests();
}

//  Until the language list shows, or `timeout`. Saves its first frame to
//  DebugDumps. Throws OperationFailedException with `failure` if it does
//  not show.
void wait_for_language_list(
    ConsoleHandle& console, ProControllerContext& context,
    Milliseconds timeout, std::string_view failure
){
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        if (frame && is_language_list(frame)){
            console.log("AutoStory intro: language-list: detected.");
            dump_debug_image(console, "PokemonLZA/AutoStory/Intro", "language-list", frame);
            return;
        }
        context.wait_for(250ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("AutoStory intro: {} (waited {} ms)", failure, timeout.count()),
        console
    );
}

//  On the language list: one press at a time towards entry `target`,
//  reading the highlight after each, until it is on `target`. A dropped
//  press or a frame caught mid-move is read again on the next turn.
void move_language_cursor(ConsoleHandle& console, ProControllerContext& context, size_t target){
    for (size_t attempt = 0; attempt < 20; attempt++){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        const std::optional<size_t> cursor = frame ? read_language_list_cursor(frame) : std::nullopt;
        if (!cursor){
            console.log("AutoStory intro: language-list: no highlight read; reading again.");
            context.wait_for(250ms);
            continue;
        }
        if (*cursor == target){
            console.log(std::format("AutoStory intro: language-list: the highlight is on entry {}.", target));
            return;
        }
        const std::optional<std::vector<IntroKey>> moves = language_list_moves(*cursor, target);
        if (!moves || moves->empty()){
            break;
        }
        console.log(std::format("AutoStory intro: language-list: the highlight is on entry {}.", *cursor));
        press_intro_keys(console, context, {moves->front()}, 400ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("AutoStory intro: the language list's highlight did not reach entry {}.", target),
        console
    );
}

//  Until the system keyboard shows (or, `gone`, has closed), or `timeout`.
//  Throws OperationFailedException if it does not.
void wait_for_system_keyboard(ConsoleHandle& console, ProControllerContext& context, bool gone, Milliseconds timeout){
    SystemKeyboardDetector keyboard;
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        if (frame && keyboard.detect(frame) != gone){
            console.log(std::format("AutoStory intro: keyboard: {}.", gone ? "closed" : "detected"));
            return;
        }
        context.wait_for(250ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("AutoStory intro: the name keyboard {} within {} ms.", gone ? "did not close" : "did not show", timeout.count()),
        console
    );
}

//  B deletes a character, and closes the keyboard on an empty field, so it
//  deletes only while the field holds text (a name left from a retry).
void clear_system_keyboard_field(ConsoleHandle& console, ProControllerContext& context){
    for (size_t c = 0; c < 14; c++){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        if (!frame || !system_keyboard_field_has_text(frame)){
            return;
        }
        console.log("AutoStory intro: keyboard: the field holds text; B.");
        pbf_press_button(context, BUTTON_B, 60ms, 250ms);
    }
}

//  What type_on_system_keyboard() types for `plan`: it clicks Shift before
//  each capital, so the keys in lower case, the first a capital when Shift
//  goes first.
std::string system_keyboard_text(const NameKeys& plan){
    std::string text;
    for (const char key : plan.keys){
        text += (char)std::tolower((unsigned char)key);
    }
    if (plan.shift_first && !text.empty()){
        text[0] = (char)std::toupper((unsigned char)text[0]);
    }
    return text;
}

//  One look picker press: a short tilt of the left stick (the screen says
//  "Use the Left Stick"), then a pause for the highlight to move.
void press_look_picker_key(ConsoleHandle& console, ProControllerContext& context, IntroKey key){
    switch (key){
    case IntroKey::UP:
        console.log("AutoStory intro: look-picker: stick UP.");
        pbf_move_left_joystick(context, {0, +1}, 200ms, 500ms);
        break;
    case IntroKey::DOWN:
        console.log("AutoStory intro: look-picker: stick DOWN.");
        pbf_move_left_joystick(context, {0, -1}, 200ms, 500ms);
        break;
    case IntroKey::LEFT:
        console.log("AutoStory intro: look-picker: stick LEFT.");
        pbf_move_left_joystick(context, {-1, 0}, 200ms, 500ms);
        break;
    case IntroKey::RIGHT:
        console.log("AutoStory intro: look-picker: stick RIGHT.");
        pbf_move_left_joystick(context, {+1, 0}, 200ms, 500ms);
        break;
    case IntroKey::A:
        console.log("AutoStory intro: look-picker: A.");
        pbf_press_button(context, BUTTON_A, 160ms, 500ms);
        break;
    }
    context.wait_for_all_requests();
}

//  Until the look picker shows, or `timeout`. Saves its first frame to
//  DebugDumps. Throws OperationFailedException if it does not show.
void wait_for_look_picker(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout){
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        if (frame && is_look_picker(frame)){
            console.log("AutoStory intro: look-picker: detected.");
            dump_debug_image(console, "PokemonLZA/AutoStory/Intro", "look-picker", frame);
            return;
        }
        context.wait_for(250ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("AutoStory intro: the look picker did not show within {} ms.", timeout.count()),
        console
    );
}

//  The look picker's highlight once it has settled: frames 150 ms apart
//  until two reads in a row agree on a look, over at most 2 s. The card
//  grows and moves for a moment after a press, so one read can catch it
//  between looks. Nullopt (the last frame saved to DebugDumps) if no two
//  reads agree.
std::optional<size_t> read_settled_look(ConsoleHandle& console, ProControllerContext& context){
    std::optional<size_t> previous;
    VideoSnapshot frame;
    for (size_t read = 0; read < 14; read++){
        context.wait_for_all_requests();
        frame = console.video().snapshot();
        const std::optional<size_t> cursor = frame ? read_look_picker_cursor(frame) : std::nullopt;
        if (cursor && cursor == previous){
            return cursor;
        }
        previous = cursor;
        context.wait_for(150ms);
    }
    if (frame){
        dump_debug_image(console, "PokemonLZA/AutoStory/Intro", "look-picker-unread", frame);
    }
    return std::nullopt;
}

//  On the look picker: one press at a time towards look `target`, reading
//  the settled highlight (read_settled_look()) after each, until it is on
//  `target`. The picker opens with
//  no face highlighted, so while none is, UP (harmless on any face of the
//  top row) to bring the highlight up. Throws OperationFailedException if the highlight is
//  not read or does not reach `target`.
void move_look_cursor(ConsoleHandle& console, ProControllerContext& context, size_t target){
    size_t unread = 0;
    for (size_t attempt = 0; attempt < 24; attempt++){
        const std::optional<size_t> cursor = read_settled_look(console, context);
        if (!cursor){
            unread++;
            if (unread > 6){
                break;
            }
            console.log("AutoStory intro: look-picker: no highlight read.");
            press_look_picker_key(console, context, IntroKey::UP);
            continue;
        }
        unread = 0;
        if (*cursor == target){
            return;
        }
        const std::optional<std::vector<IntroKey>> moves = look_picker_moves(*cursor, target);
        if (!moves || moves->empty()){
            break;
        }
        console.log(std::format("AutoStory intro: look-picker: the highlight is on look {}.", *cursor));
        press_look_picker_key(console, context, moves->front());
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("AutoStory intro: the look picker's highlight was not read on look {}.", target),
        console
    );
}

} // namespace


void launch_game_from_home(ConsoleHandle& console, ProControllerContext& context){
    launch_selected_game_from_home(
        console, context, "PLZA", "language list",
        [](const ImageViewRGB32& frame){ return is_language_list(frame); },
        90s, "PokemonLZA/AutoStory/Intro"
    );
}

void new_game_language(ConsoleHandle& console, ProControllerContext& context, Language language){
    const std::optional<size_t> target = language_list_index(language);
    if (!target){
        throw UserSetupError(console, "AutoStory: the new game's language list has no such language.");
    }
    console.log(std::format("AutoStory intro: language {}: entry {} of the list.", language_data(language).code, *target));
    wait_for_language_list(console, context, 5s,
        "the language list is not on screen: the launch_game_from_home step brings it up."
    );
    move_language_cursor(console, context, *target);
    press_intro_keys(console, context, {IntroKey::A}, 5000ms);
}

void pick_player_look(ConsoleHandle& console, ProControllerContext& context, size_t look){
    if (look < 1 || look > LOOK_COUNT){
        throw UserSetupError(console, std::format("AutoStory: the player look must be 1 to {}.", LOOK_COUNT));
    }
    console.log(std::format("AutoStory intro: look {} of {}.", look, LOOK_COUNT));
    wait_for_look_picker(console, context, 10s);
    move_look_cursor(console, context, look);
    console.log(std::format("AutoStory intro: look-picker: the highlight is on look {}; A.", look));
    press_intro_keys(console, context, {IntroKey::A}, 3000ms);
}

void type_player_name(
    ConsoleHandle& console, ProControllerContext& context,
    const std::string& name, bool first_letter_auto_caps
){
    const std::optional<NameKeys> plan = name_keys(name, first_letter_auto_caps);
    if (!plan){
        throw UserSetupError(console, std::format("AutoStory: the player name \"{}\" cannot be typed.", name));
    }
    wait_for_system_keyboard(console, context, false, 20s);
    dump_intro_frame(console, context, "keyboard");
    //  Let the keyboard finish sliding in: inputs during that are dropped.
    context.wait_for(800ms);
    clear_system_keyboard_field(console, context);
    const std::string typed = system_keyboard_text(*plan);
    console.log(std::format(
        "AutoStory intro: typing \"{}\" as \"{}\" (keyboard capitalizes the first letter: {}).",
        name, typed, first_letter_auto_caps
    ));
    FastCodeEntry::type_on_system_keyboard(console, context, typed);
    console.log("AutoStory intro: PLUS (the keyboard's OK).");
    pbf_press_button(context, BUTTON_PLUS, 160ms, 0ms);
    wait_for_system_keyboard(console, context, true, 5s);
    //  "Confirm Choices" opens on "Change hair color"; "Ready!" is two rows
    //  down (console B, 2026-11-10).
    context.wait_for(2500ms);
    dump_intro_frame(console, context, "confirm-choices");
    console.log("AutoStory intro: Confirm Choices: DOWN x2 to \"Ready!\", then A.");
    press_intro_keys(console, context, *confirm_choices_moves(0), 2000ms);
    dump_intro_frame(console, context, "confirm");
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
