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
<<<<<<< HEAD
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardCodeEntry.h"
#include "VideoGames/PokemonLZA/Inference/Map/PokemonLZA_MapDetector.h"
=======
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_SystemKeyboardTyping.h"
>>>>>>> plza/story-base
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

//  An intro screen with no detector yet (is_look_picker() is a TODO stub):
//  waited for until `is_screen` says it is up or `fixed_wait` passes, then
//  pressed blind. Either way its frame is saved to DebugDumps, so a run
//  records it for the detector's capture.
void wait_for_intro_screen(
    ConsoleHandle& console, ProControllerContext& context,
    std::string_view screen, bool (*is_screen)(const ImageViewRGB32&), Milliseconds fixed_wait
){
    const WallClock deadline = current_time() + fixed_wait;
    bool seen = false;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        if (frame && is_screen(frame)){
            seen = true;
            break;
        }
        context.wait_for(250ms);
    }
    context.wait_for_all_requests();
    const VideoSnapshot frame = console.video().snapshot();
    if (seen){
        console.log(std::format("AutoStory intro: {}: detected.", screen));
    }else{
        console.log(std::format(
            "AutoStory intro: {}: NOT CHECKED, its detector is a TODO stub; waited a fixed {} ms, pressing blind.",
            screen, fixed_wait.count()
        ));
    }
    if (frame){
        dump_debug_image(console, "PokemonLZA/AutoStory/Intro", std::string(screen), frame);
    }
}

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
    const std::optional<std::vector<IntroKey>> presses = look_picker_presses(look);
    if (!presses){
        throw UserSetupError(console, std::format("AutoStory: the player look must be 1 to {}.", LOOK_COUNT));
    }
    console.log(std::format("AutoStory intro: look {} of {}.", look, LOOK_COUNT));
    //  The language's A already waited 5 s for this screen.
    wait_for_intro_screen(console, context, "look-picker", is_look_picker, 0ms);
    press_intro_keys(console, context, *presses, 3000ms);
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
    press_intro_keys(console, context, {IntroKey::DOWN, IntroKey::DOWN, IntroKey::A}, 2000ms);
    dump_intro_frame(console, context, "confirm");
}

void talk_through_map_screen(ConsoleHandle& console, ProControllerContext& context){
    MapDetector map;
    const WallClock deadline = current_time() + 60s;
    while (true){
        context.wait_for_all_requests();
        if (map.detect(console.video().snapshot())){
            break;
        }
        if (current_time() > deadline){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "talk_through_map_screen(): the map did not show within 60 s.",
                console
            );
        }
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    }
    pbf_press_button(context, BUTTON_B, 160ms, 1500ms);
    context.wait_for_all_requests();
    mash_scene(console, context, SceneMashGoal::OVERWORLD, BUTTON_B, 90s);
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
