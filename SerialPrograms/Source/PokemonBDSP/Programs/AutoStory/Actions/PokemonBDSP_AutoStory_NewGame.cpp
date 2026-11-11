/*  BDSP AutoStory - New Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/ScreenMachine/ScreenMachine.h"
#include "CommonTools/ScreenMachine/StepTrace.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_StartGameUserSelectDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_NameListDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_IntroMenuDetector.h"
#include "PokemonBDSP_AutoStory_IntroProbe.h"
#include "PokemonBDSP_AutoStory_IntroSteps.h"
#include "PokemonBDSP_AutoStory_NewGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace detail;


const std::vector<Language>& bdsp_language_menu(){
    static const std::vector<Language> menu{
        Language::English,  Language::Japanese,
        Language::Spanish,  Language::Japanese,
        Language::French,   Language::Korean,
        Language::German,   Language::ChineseSimplified,
        Language::Italian,  Language::ChineseTraditional,
    };
    return menu;
}

namespace{

std::optional<size_t> language_item(Language language){
    const std::vector<Language>& menu = bdsp_language_menu();
    const auto it = std::find(menu.begin(), menu.end(), language);
    if (it == menu.end()){
        return std::nullopt;
    }
    return (size_t)(it - menu.begin());
}

} // namespace

std::optional<std::vector<DpadPosition>> language_pick_moves(size_t cursor, const IntroPicks& picks){
    const std::optional<size_t> to = language_item(picks.game_language);
    if (!to){
        return std::nullopt;
    }
    return menu_cursor_moves(BDSP_LANGUAGE_GRID, cursor, *to);
}
std::optional<std::vector<DpadPosition>> look_pick_moves(size_t cursor, const IntroPicks& picks){
    return menu_cursor_moves(BDSP_LOOK_GRID, cursor, picks.look);
}

namespace detail{
IntroProgress intro_start(const IntroPicks& picks){
    IntroProgress progress;
    progress.language_pending = true;
    progress.look_pending = picks.look != 0;
    return progress;
}
} // namespace detail


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
    //  Before the dialogue box: its prompt sits in one.
    if (language_select_cursor(screen)){
        return "language-select";
    }
    if (look_select_cursor(screen)){
        return "look-select";
    }
    switch (NameListDetector().selected_row(screen)){
    case -1:
        break;
    case 0:
        return "rival-list-tell";
    default:
        return "rival-list-preset";
    }
    if (MenuDetector(COLOR_RED, /*skip_new_banner*/ true).detect(screen)){
        return "menu";
    }
    //  Includes the appearance grid, whose prompt sits in a dialogue box.
    if (ShortDialogDetector().detect(screen)){
        return "dialog";
    }
    //  The boot and fades. After the dialogue: Rowan's first lines show on black.
    if (BlackScreenDetector().detect(screen)){
        return "black";
    }
    //  Logos, the title, language, cutscenes, the bedroom with no menu open.
    return "other";
}

IntroStep decide_intro_step(std::string_view screen, const IntroProgress& progress){
    if (screen == "user-select"){
        return IntroStep::SELECT_USER;
    }
    if (screen == "home"){
        return IntroStep::FAIL_NOT_IN_GAME;
    }
    if (screen == "keyboard"){
        if (!progress.player_named && (progress.language_pending || progress.look_pending)){
            return IntroStep::FAIL_PICK_MISSED;
        }
        return progress.player_named ? IntroStep::CANCEL_RIVAL_KEYBOARD : IntroStep::ENTER_PLAYER_NAME;
    }
    if (screen == "language-select"){
        //  Once picked, A again only if the select still shows.
        return progress.language_pending ? IntroStep::PICK_LANGUAGE : IntroStep::ADVANCE;
    }
    if (screen == "look-select"){
        return progress.look_pending ? IntroStep::PICK_LOOK : IntroStep::ADVANCE;
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
    if (screen == "black"){
        return IntroStep::WAIT;
    }
    if (screen == "dialog"){
        return progress.rival_named ? IntroStep::ADVANCE_WITH_B : IntroStep::ADVANCE;
    }
    //  The bedroom comes only after the rival is named. Before that, every unknown
    //  screen (logos, the title, language, the appearance preview, cutscenes)
    //  takes A, but only for so long before a screen only BDSP shows: past that
    //  it is another game.
    if (progress.rival_named){
        return progress.b_presses < INTRO_B_BEFORE_X ? IntroStep::CLEAR_WITH_B : IntroStep::PROBE_FOR_MENU;
    }
    if (!progress.game_confirmed && progress.unconfirmed_for >= INTRO_TIME_BEFORE_BDSP){
        return IntroStep::FAIL_NOT_BDSP;
    }
    return IntroStep::ADVANCE;
}
bool intro_screen_confirms_bdsp(std::string_view screen){
    return screen == "dialog" || screen == "language-select" || screen == "look-select" || screen == "rival-list-tell" || screen == "rival-list-preset" || screen == "menu";
}

namespace detail{
void note_intro_screen(IntroProgress& progress, std::string_view screen, std::chrono::milliseconds since_start){
    if (intro_screen_confirms_bdsp(screen)){
        progress.game_confirmed = true;
    }
    if (screen == "black"){
        return;
    }
    if (!progress.first_shown){
        progress.first_shown = since_start;
    }
    if (!progress.game_confirmed){
        progress.unconfirmed_for = since_start - *progress.first_shown;
    }
}

bool intro_in_control_without_menu(const XProbe& probe){
    return probe.still_change <= INTRO_STILL_CHANGE && probe.x_change > INTRO_X_CHANGE && probe.b_change <= INTRO_B_RESTORE;
}

void record_intro_step(IntroProgress& progress, IntroStep step){
    switch (step){
    case IntroStep::ENTER_PLAYER_NAME:
        progress.player_named = true;
        return;
    case IntroStep::MOVE_TO_PRESET:
        //  The list again: the name was not confirmed.
        progress.rival_picked = false;
        progress.screens_after_pick = 0;
        return;
    case IntroStep::PICK_PRESET:
        progress.rival_picked = true;
        progress.screens_after_pick = 0;
        return;
    case IntroStep::ADVANCE:
        if (progress.rival_picked && ++progress.screens_after_pick >= INTRO_SCREENS_AFTER_RIVAL_PICK){
            progress.rival_named = true;
        }
        return;
    case IntroStep::CLEAR_WITH_B:
        progress.b_presses++;
        return;
    case IntroStep::PROBE_FOR_MENU:
        progress.b_presses = 0;
        return;
    default:
        return;
    }
}
} // namespace detail

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
    const IntroPicks& picks;
    const std::string& player_name;
    WallClock start;
    IntroProgress progress;
    size_t x_without_menu = 0;
    //  X opened something and B closed it, from a still screen.
    bool in_control = false;
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
    if (!screen_now || MenuDetector(COLOR_RED, /*skip_new_banner*/ true).detect(screen_now)){
        run.trace.fail("The main menu did not close.");
    }
}

//  The menu's area, which X changes when the menu opens.
const ImageFloatBox INTRO_MENU_AREA(0.12, 0.08, 0.76, 0.56);

double menu_area_change(const VideoSnapshot& before, const VideoSnapshot& after){
    if (!before || !after){
        return 1000;
    }
    return ImageMatch::pixel_RMSD(
        extract_box_reference(*before.frame, INTRO_MENU_AREA), extract_box_reference(*after.frame, INTRO_MENU_AREA)
    );
}

enum class XProbeResult{
    MENU,           //  the menu showed
    IN_CONTROL,     //  something X opened and B closed, from a still screen
    NOTHING,
};
//  X from a screen that held still; B after it if no menu showed.
XProbeResult x_from_still_screen(IntroRun& run){
    VideoStream& stream = run.env.console;
    run.context.wait_for_all_requests();
    const VideoSnapshot first = stream.video().snapshot();
    run.context.wait_for(INTRO_STILL_TIME);
    const VideoSnapshot before = stream.video().snapshot();
    press(run, BUTTON_X);
    run.context.wait_for_all_requests();
    MenuWatcher menu(COLOR_RED, /*skip_new_banner*/ true);
    if (wait_until(stream, run.context, 3s, {{menu}}) == 0){
        return XProbeResult::MENU;
    }
    const VideoSnapshot after_x = stream.video().snapshot();
    pbf_press_button(run.context, BUTTON_B, 80ms, 1000ms);
    run.context.wait_for_all_requests();
    const VideoSnapshot after_b = stream.video().snapshot();
    const XProbe probe{menu_area_change(first, before), menu_area_change(before, after_x), menu_area_change(before, after_b)};
    run.trace.action("X probe: still " + std::to_string(probe.still_change) + ", X " + std::to_string(probe.x_change) +
        ", after B " + std::to_string(probe.b_change) + ".");
    return intro_in_control_without_menu(probe) ? XProbeResult::IN_CONTROL : XProbeResult::NOTHING;
}

//  X, then the menu or B presses again: whatever X opened instead, B closes.
//  X opening something unrecognized from a still screen, which B closes
//  again, is the player in control too.
void probe_for_menu(IntroRun& run){
    const XProbeResult result = x_from_still_screen(run);
    run.in_control = result == XProbeResult::IN_CONTROL;
    if (result != XProbeResult::NOTHING){
        return;     //  The next read sees the menu, or the in-control state, and finishes.
    }
    run.trace.action("X opened no menu; B again before the next X.");
    if (++run.x_without_menu >= INTRO_X_WITHOUT_MENU){
        run.trace.fail("No menu after " + std::to_string(run.x_without_menu) + " X presses: the player never got control.");
    }
}

void advance(IntroRun& run){
    pbf_press_button(run.context, BUTTON_A, 60ms, 400ms);
}

//  One of the intro's two menus: its shape and its cursor reader.
struct IntroMenu{
    std::string_view what;
    MenuGrid grid;
    std::optional<size_t> (*read)(const ImageViewRGB32& screen);
};
const IntroMenu LANGUAGE_MENU{"language", BDSP_LANGUAGE_GRID, language_select_cursor};
const IntroMenu LOOK_MENU{"look", BDSP_LOOK_GRID, look_select_cursor};

//  Reads of a menu before its cursor must be on the pick.
const size_t INTRO_PICK_TRIES = 3;

//  The menu's cursor as read now; nullopt when the menu is not showing.
std::optional<size_t> read_cursor(IntroRun& run, const IntroMenu& menu){
    run.context.wait_for_all_requests();
    const VideoSnapshot screen = run.env.console.video().snapshot();
    if (!screen){
        return std::nullopt;
    }
    return menu.read(*screen.frame);
}

//  Reads the cursor, moves it to `target` and reads it there, up to
//  INTRO_PICK_TRIES times (a press dropped as the screen fades in), then
//  saves a screenshot and presses A. Whether it picked: false when the menu
//  went away, and the next read decides.
bool pick_on_grid(IntroRun& run, const IntroMenu& menu, size_t target){
    for (size_t attempt = 0; attempt < INTRO_PICK_TRIES; attempt++){
        const std::optional<size_t> cursor = read_cursor(run, menu);
        if (!cursor){
            return false;
        }
        run.trace.action(std::string("Picking the ") + std::string(menu.what) + ": cursor on item " + std::to_string(*cursor) +
            ", the pick is item " + std::to_string(target) + ".");
        if (*cursor == target){
            run.trace.screen(std::string("pick-") + std::string(menu.what));
            press(run, BUTTON_A);
            pbf_wait(run.context, 500ms);
            return true;
        }
        press_cursor_moves(run.context, *menu_cursor_moves(menu.grid, *cursor, target));
    }
    run.trace.fail(std::string("The ") + std::string(menu.what) + " menu's cursor did not reach its pick in " +
        std::to_string(INTRO_PICK_TRIES) + " tries.");
}

//  PICK_LANGUAGE or PICK_LOOK.
void pick_on_intro_menu(IntroRun& run, IntroStep step){
    if (step == IntroStep::PICK_LANGUAGE){
        if (pick_on_grid(run, LANGUAGE_MENU, *language_item(run.picks.game_language))){
            run.progress.language_pending = false;
        }
    }else if (pick_on_grid(run, LOOK_MENU, run.picks.look)){
        run.progress.look_pending = false;
    }
}

//  Notes `screen` in the run's progress, decides the step for it and counts it.
IntroStep next_intro_step(IntroRun& run, std::string_view screen){
    note_intro_screen(run.progress, screen,
        std::chrono::duration_cast<std::chrono::milliseconds>(current_time() - run.start));
    const IntroStep step = decide_intro_step(screen, run.progress);
    record_intro_step(run.progress, step);
    return step;
}

//  Stops the intro for one of the FAIL_ steps.
[[noreturn]] void fail_intro(const IntroRun& run, IntroStep step){
    if (step == IntroStep::FAIL_NOT_IN_GAME){
        run.trace.fail("The Switch HOME menu is showing: the game is not running in front.");
    }
    if (step == IntroStep::FAIL_NOT_BDSP){
        run.trace.fail("The game in front is not BDSP: no BDSP screen in " +
            std::to_string(run.progress.unconfirmed_for.count() / 1000) + " s of unknown screens.");
    }
    run.trace.fail(std::string("The player's name keyboard showed before the ") +
        (run.progress.language_pending ? "language select" : "appearance grid") +
        " was read.");
}

//  Carries out what decide_intro_step() chose for `screen`.
void act(IntroRun& run, std::string_view screen){
    const IntroStep step = next_intro_step(run, screen);
    switch (step){
    case IntroStep::SELECT_USER:
        //  The cursor starts on the user that launched the game.
        press(run, BUTTON_A);
        pbf_wait(run.context, 1000ms);
        return;
    case IntroStep::FAIL_NOT_IN_GAME:
    case IntroStep::FAIL_NOT_BDSP:
    case IntroStep::FAIL_PICK_MISSED:
        fail_intro(run, step);
    case IntroStep::PICK_LANGUAGE:
    case IntroStep::PICK_LOOK:
        pick_on_intro_menu(run, step);
        return;
    case IntroStep::ENTER_PLAYER_NAME:
        enter_player_name(run.env.console, run.context, run.trace, run.player_name);
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
        return;
    case IntroStep::CLOSE_MENU_AND_FINISH:
        close_menu_and_finish(run);
        return;
    case IntroStep::ADVANCE:
        advance(run);
        return;
    case IntroStep::ADVANCE_WITH_B:
    case IntroStep::CLEAR_WITH_B:
        pbf_press_button(run.context, BUTTON_B, 60ms, 400ms);
        return;
    case IntroStep::PROBE_FOR_MENU:
        probe_for_menu(run);
        return;
    case IntroStep::WAIT:
        pbf_wait(run.context, 500ms);
        return;
    }
}

//  After the restart BDSP boots on black for about 25 s (r3); presses then
//  would count against the wrong-game guard, so none until it ends.
void wait_out_boot(VideoStream& stream, ProControllerContext& context, StepTrace& trace){
    context.wait_for_all_requests();
    const WallClock deadline = current_time() + INTRO_BOOT_LIMIT;
    while (current_time() < deadline){
        VideoSnapshot screen = stream.video().snapshot();
        if (screen && !BlackScreenDetector().detect(screen)){
            trace.action("The boot's black screen ended.");
            return;
        }
        context.wait_for(500ms);
    }
    trace.fail("The screen stayed black for " + std::to_string(INTRO_BOOT_LIMIT.count() / 1000) + " s after the restart.");
}

//  The machine's states: the screens classify_intro_screen() tells apart, with
//  the keyboard split by whose name it asks for. Each state's step is act().
std::vector<ScreenState> intro_states(IntroRun& run){
    auto screen_is = [&run](const ImageViewRGB32& screen, const std::string& name){
        return classify_intro_screen(&run.env.console, screen) == name;
    };
    return {
        {"in-control",
            [&run](const ImageViewRGB32&){ return run.in_control; },
            [&run]{ run.trace.action("X opened and B closed the menu from a still bedroom: the player is in control."); },
            {}, 1, true},
        {"user-select",
            [=](const ImageViewRGB32& s){ return screen_is(s, "user-select"); },
            [&run]{ act(run, "user-select"); }, {}, 3},
        {"home",
            [=](const ImageViewRGB32& s){ return screen_is(s, "home"); },
            [&run]{ act(run, "home"); }, {}, 1},
        {"player-keyboard",
            [=, &run](const ImageViewRGB32& s){ return !run.progress.player_named && screen_is(s, "keyboard"); },
            [&run]{ act(run, "keyboard"); }, {"dialog", "black", "other"}, 1},
        {"rival-keyboard",
            [=, &run](const ImageViewRGB32& s){ return run.progress.player_named && screen_is(s, "keyboard"); },
            [&run]{ act(run, "keyboard"); }, {"rival-list-tell", "dialog", "black", "other"}, 3},
        {"language-select",
            [=](const ImageViewRGB32& s){ return screen_is(s, "language-select"); },
            [&run]{ act(run, "language-select"); }, {}, 10},
        {"look-select",
            [=](const ImageViewRGB32& s){ return screen_is(s, "look-select"); },
            [&run]{ act(run, "look-select"); }, {}, 10},
        {"rival-list-tell",
            [=](const ImageViewRGB32& s){ return screen_is(s, "rival-list-tell"); },
            [&run]{ act(run, "rival-list-tell"); }, {"rival-list-preset", "black", "other"}, 6},
        {"rival-list-preset",
            [=](const ImageViewRGB32& s){ return screen_is(s, "rival-list-preset"); },
            [&run]{ act(run, "rival-list-preset"); }, {"dialog", "black", "other"}, 6},
        {"menu",
            [=](const ImageViewRGB32& s){ return screen_is(s, "menu"); },
            [&run]{ act(run, "menu"); }, {}, 2, true},
        {"dialog",
            [=](const ImageViewRGB32& s){ return screen_is(s, "dialog"); },
            [&run]{ act(run, "dialog"); }, {}, 400},
        //  The boot is waited out before the machine starts; this is fades.
        {"black",
            [=](const ImageViewRGB32& s){ return screen_is(s, "black"); },
            [&run]{ act(run, "black"); }, {}, 300, false, 60},
        {"other",
            [](const ImageViewRGB32&){ return true; },
            [&run]{ act(run, "other"); }, {}, 300, false, 60},
    };
}

} // namespace



void play_new_game_intro(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const IntroPicks& picks,
    const std::string& player_name
){
    VideoStream& stream = env.console;
    StepTrace trace(stream, "Intro");

    if (!language_pick_moves(0, picks)){
        throw UserSetupError(env.console, "BDSP's language select lacks the game's language: pick one it lists.");
    }
    if (!look_pick_moves(0, picks)){
        throw UserSetupError(env.console, "The player's look must be one of BDSP's eight.");
    }

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

    wait_out_boot(stream, context, trace);
    IntroRun run{env, context, trace, picks, player_name, current_time(), intro_start(picks), 0};
    trace.action(std::string("Picks: language ") + language_data(picks.game_language).name +
        ", look " + std::to_string(picks.look + 1) + ".");
    const std::vector<ScreenState> states = intro_states(run);
    run_screen_machine(stream, context, states, std::chrono::minutes(15), trace);
    trace.action("Intro done.");
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
