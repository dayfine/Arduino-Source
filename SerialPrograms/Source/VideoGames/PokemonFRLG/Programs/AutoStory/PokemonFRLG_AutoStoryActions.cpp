/*  AutoStory Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_NamingScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_SelectionArrowDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_StartMenuNavigation.h"
#include "PokemonFRLG_AutoStoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

constexpr Milliseconds POLL = 300ms;
//  Text without an arrow or a prompt this long: press anyway, for a last
//  line that waits without an arrow (Oak's "Here, come with me!").
constexpr Milliseconds TEXT_STILL = 4000ms;
//  In the intro: A this often on a message box; how long with no box
//  before checking for the bedroom; and before pressing A on a screen
//  nothing reads.
constexpr Milliseconds INTRO_A_EVERY = 1500ms;
constexpr Milliseconds INTRO_QUIET = 3000ms;
constexpr Milliseconds INTRO_STUCK = 15000ms;
constexpr Milliseconds INTRO_TIMEOUT = 600000ms;
//  "Wait about 8 s" after YES at the nurse (console B, 2026-11-01): 1.5 s
//  between presses did not heal.
constexpr Milliseconds HEAL_TIME = 8000ms;
//  FRLG takes a tap this short as a turn, without a step.
constexpr Milliseconds TURN_TAP = 40ms;
constexpr Milliseconds TURN_SETTLE = 300ms;

DpadPosition dpad(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return DPAD_UP;
    case Direction::EAST:
        return DPAD_RIGHT;
    case Direction::SOUTH:
        return DPAD_DOWN;
    case Direction::WEST:
        return DPAD_LEFT;
    }
    return DPAD_NONE;
}

void press(ProControllerContext& context, Button button, Milliseconds after = 340ms){
    pbf_press_button(context, button, 160ms, after);
}

void tap(ProControllerContext& context, DpadPosition position, Milliseconds after = 400ms){
    pbf_press_dpad(context, position, 160ms, after);
}



bool wait_for_yes_no(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout){
    SelectionArrowWatcher yes(COLOR_RED, &console.overlay(), SelectionArrowPositionConfirmationMenu::YES);
    context.wait_for_all_requests();
    return wait_until(console, context, timeout, {yes}) >= 0;
}

//  The START menu's cursor onto `row`, read from the arrow. As
//  move_cursor_to_position(), for a menu of any length.
bool move_start_cursor(ConsoleHandle& console, ProControllerContext& context, int rows, int row){
    SelectionArrowWatcher arrow(COLOR_RED, &console.overlay(), MENU_ARROW_BOX);
    context.wait_for_all_requests();
    if (wait_until(console, context, 2000ms, {arrow}) < 0){
        return false;
    }
    const double arrow_y = arrow.last_detected().y + arrow.last_detected().height / 2;
    int current = 0;
    double closest = std::numeric_limits<double>::max();
    for (int c = 0; c < rows; c++){
        const ImageFloatBox box = SelectionArrowDetector::arrow_box_for_position(static_cast<SelectionArrowPositionStartMenu>(c));
        const double distance = std::abs(arrow_y - (box.y + box.height / 2));
        if (distance < closest){
            closest = distance;
            current = c;
        }
    }
    console.log(std::format("START menu: cursor on row {}, moving to row {}.", current + 1, row + 1));
    const int down = (row - current + rows) % rows;
    const int up = (current - row + rows) % rows;
    for (int c = 0; c < std::min(down, up); c++){
        tap(context, down <= up ? DPAD_DOWN : DPAD_UP);
    }
    context.wait_for_all_requests();
    return true;
}

void open_start_menu_row(ConsoleHandle& console, ProControllerContext& context, StartMenu menu, int row){
    open_start_menu(console, context);
    if (!move_start_cursor(console, context, start_menu_rows(menu), row)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT, "START menu: no cursor.", console
        );
    }
    press(context, BUTTON_A, 1500ms);
}


//  The new game's intro, from the title screen. Everything but the keyboard
//  is advanced with A, which never hurts there (BOY is the default, YES to
//  GREEN); B is never pressed outside the keyboard, since in the intro it
//  goes back to the controls pages (console B, 2026-11-01). The controls
//  pages read as a message box without an arrow, and the arrow at the end
//  of Oak's lines may be missed, so a message box gets A every 1.5 s
//  whatever it shows. On the keyboard, B deletes whatever a stray A typed
//  and START jumps to OK, which confirms the empty name as the default.
class NewGameIntro{
public:
    NewGameIntro(ConsoleHandle& console, ProControllerContext& context)
        : m_console(console)
        , m_context(context)
    {}

    void run(){
        press_a_until_dialog();
        const WallClock deadline = current_time() + INTRO_TIMEOUT;
        m_last_dialog = current_time();
        m_last_press = m_last_dialog;
        while (current_time() < deadline){
            if (step()){
                m_console.log("New game: the bedroom.");
                return;
            }
        }
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT, "play_new_game_intro(): still in the intro after 10 minutes.", m_console
        );
    }

private:
    //  The title, then the controls pages, which A turns, up to a message box.
    void press_a_until_dialog(){
        WhiteDialogWatcher dialog(COLOR_RED);
        m_context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            m_console, m_context,
            [](ProControllerContext& context){
                for (int c = 0; c < 80; c++){
                    pbf_press_button(context, BUTTON_A, 160ms, 1340ms);
                }
            },
            {dialog}
        );
        if (ret < 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT, "play_new_game_intro(): no message after the title screen.", m_console
            );
        }
    }

    //  True once the bedroom shows.
    bool step(){
        m_context.wait_for_all_requests();
        const WallClock now = current_time();
        VideoSnapshot screen = m_console.video().snapshot();
        if (!screen){
            pbf_wait(m_context, POLL);
            return false;
        }
        if (NamingScreenDetector(COLOR_RED).detect(screen)){
            enter_empty_name();
            m_last_dialog = current_time();
            return false;
        }
        if (yes_no_cursor(screen) || WhiteDialogDetector(COLOR_RED).detect(screen)){
            m_dialog_seen = true;
            m_last_dialog = now;
            if (now - m_last_press >= INTRO_A_EVERY){
                press(m_context, BUTTON_A, 200ms);
                m_last_press = now;
            }else{
                pbf_wait(m_context, POLL);
            }
            return false;
        }
        return quiet(screen, now);
    }

    //  No message box and no keyboard: a fade, a pause in the speech, or
    //  the bedroom.
    bool quiet(const ImageViewRGB32& screen, WallClock now){
        const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
        if (is_black(game) || !m_dialog_seen || now - m_last_dialog < INTRO_QUIET){
            pbf_wait(m_context, POLL);
            return false;
        }
        //  START does nothing in the intro.
        if (overworld_shows(m_console, m_context)){
            return true;
        }
        //  A screen still waiting for A that none of the detectors knows.
        if (now - m_last_press > INTRO_STUCK){
            m_console.log("New game: nothing known on screen, pressing A.");
            press(m_context, BUTTON_A, 200ms);
            m_last_press = current_time();
        }
        return false;
    }

    void enter_empty_name(){
        m_console.log("New game: the keyboard, confirming an empty name.");
        for (int c = 0; c < 8; c++){
            press(m_context, BUTTON_B, 340ms);
        }
        press(m_context, BUTTON_PLUS, 500ms);
        press(m_context, BUTTON_A, 2000ms);
        m_context.wait_for_all_requests();
    }

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    WallClock m_last_dialog;
    WallClock m_last_press;
    bool m_dialog_seen = false;
};

}


int start_menu_rows(StartMenu menu){
    switch (menu){
    case StartMenu::BEFORE_POKEMON:
        return 5;
    case StartMenu::BEFORE_POKEDEX:
        return 6;
    case StartMenu::FULL:
        return 7;
    }
    return 7;
}
int start_menu_save_row(StartMenu menu){
    return start_menu_rows(menu) - 3;
}
int start_menu_option_row(StartMenu menu){
    return start_menu_rows(menu) - 2;
}


std::optional<bool> yes_no_cursor(const ImageViewRGB32& screen){
    if (SelectionArrowDetector(COLOR_RED, nullptr, SelectionArrowPositionConfirmationMenu::YES).detect(screen)){
        return true;
    }
    if (SelectionArrowDetector(COLOR_RED, nullptr, SelectionArrowPositionConfirmationMenu::NO).detect(screen)){
        return false;
    }
    return std::nullopt;
}

bool overworld_shows(ConsoleHandle& console, ProControllerContext& context){
    press(context, BUTTON_PLUS, 1200ms);
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    if (!screen || !StartMenuDetector(COLOR_RED).detect(screen)){
        return false;
    }
    press(context, BUTTON_B, 600ms);
    context.wait_for_all_requests();
    return true;
}

TextEnd advance_text(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout, bool stop_at_battle){
    const WallClock deadline = current_time() + timeout;
    //  A YES/NO here is a move to learn: "Delete a move to make room?" NO,
    //  then, right after, "Stop learning?" YES. B to "Stop learning?" goes
    //  back to the first, for good (console B, 2026-11-01).
    WallClock last_no = WallClock::min();
    while (current_time() < deadline){
        BattleScreenWatcher battle(COLOR_RED);
        BattleMenuWatcher battle_menu(COLOR_RED);
        SelectionArrowWatcher yes(COLOR_RED, &console.overlay(), SelectionArrowPositionConfirmationMenu::YES);
        SelectionArrowWatcher no(COLOR_RED, &console.overlay(), SelectionArrowPositionConfirmationMenu::NO);
        BattleLearnDialogWatcher learn(COLOR_RED);
        context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& context){
                for (int c = 0; c < 5; c++){
                    pbf_press_button(context, BUTTON_B, 160ms, 340ms);
                }
            },
            {battle, battle_menu, yes, no, learn}
        );
        if (ret >= 2){
            const bool stop_learning = current_time() - last_no < 10000ms;
            console.log(stop_learning ? "Text: YES/NO, YES (stop learning)." : "Text: YES/NO, NO.");
            press(context, stop_learning ? BUTTON_A : BUTTON_B, 1500ms);
            last_no = stop_learning ? WallClock::min() : current_time();
            continue;
        }
        if (ret >= 0 && stop_at_battle){
            return TextEnd::BATTLE;
        }
        if (ret < 0 && overworld_shows(console, context)){
            return TextEnd::OVERWORLD;
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("advance_text(): the overworld didn't come back within {} s.", timeout.count() / 1000),
        console
    );
}

void mash_until_overworld(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout){
    advance_text(console, context, timeout, false);
}

void answer_yes_no(ConsoleHandle& console, ProControllerContext& context, bool yes, Button advance, Milliseconds timeout){
    const WallClock deadline = current_time() + timeout;
    WallClock last_press = current_time();
    while (current_time() < deadline){
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        const std::optional<bool> on_yes = screen ? yes_no_cursor(screen) : std::nullopt;
        if (on_yes){
            console.log(yes ? "Answering YES." : "Answering NO.");
            if (yes && !*on_yes){
                tap(context, DPAD_UP);
            }
            press(context, yes ? BUTTON_A : BUTTON_B, 800ms);
            context.wait_for_all_requests();
            return;
        }
        const WallClock now = current_time();
        if ((screen && AdvanceWhiteDialogDetector(COLOR_RED).detect(screen)) || now - last_press > TEXT_STILL){
            press(context, advance, 300ms);
            last_press = now;
        }
        pbf_wait(context, POLL);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT, "answer_yes_no(): no YES/NO.", console
    );
}

void step_once(ProControllerContext& context, Direction direction, std::optional<Direction> facing){
    if (facing != direction){
        pbf_press_dpad(context, dpad(direction), TURN_TAP, TURN_SETTLE);
    }
    //  A step, once started, finishes on its own.
    pbf_press_dpad(context, dpad(direction), 120ms, 500ms);
    context.wait_for_all_requests();
}

void face_wall(ProControllerContext& context, Direction direction){
    pbf_press_dpad(context, dpad(direction), TURN_TAP, TURN_SETTLE);
    context.wait_for_all_requests();
}

void save_game(ConsoleHandle& console, ProControllerContext& context, StartMenu menu){
    console.log("Saving the game.");
    open_start_menu_row(console, context, menu, start_menu_save_row(menu));
    //  The save card and "Would you like to save the game?"
    if (!wait_for_yes_no(console, context, 5000ms)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT, "save_game(): no YES/NO to save.", console
        );
    }
    press(context, BUTTON_A, 1500ms);
    //  "There is already a saved file. Is it okay to overwrite it?", except
    //  for the first save.
    if (wait_for_yes_no(console, context, 3000ms)){
        press(context, BUTTON_A, 1500ms);
    }
    pbf_wait(context, 4000ms);
    mash_until_overworld(console, context, 30000ms);
    console.log("Saved.");
}

void set_options(ConsoleHandle& console, ProControllerContext& context, StartMenu menu){
    console.log("Options: text speed FAST, battle style SET.");
    open_start_menu_row(console, context, menu, start_menu_option_row(menu));
    //  TEXT SPEED (MID), BATTLE SCENE, BATTLE STYLE (SHIFT), ...
    tap(context, DPAD_RIGHT);
    tap(context, DPAD_DOWN);
    tap(context, DPAD_DOWN);
    tap(context, DPAD_RIGHT);
    press(context, BUTTON_B, 1500ms);
    mash_until_overworld(console, context, 20000ms);
}

void heal_at_center(ConsoleHandle& console, ProControllerContext& context){
    console.log("Healing at the Pokemon Center.");
    //  The nurse's YES/NO isn't seen by the selection detector (console B,
    //  2026-11-01); YES is the default, so A after her lines is YES.
    //  "Welcome ... We heal your POKEMON ... Shall we heal?" is 3 lines.
    press(context, BUTTON_A, 1500ms);
    press(context, BUTTON_A, 1500ms);
    press(context, BUTTON_A, 2500ms);
    press(context, BUTTON_A, 1000ms);
    pbf_wait(context, HEAL_TIME);
    mash_until_overworld(console, context, 30000ms);
}

void buy_items(ConsoleHandle& console, ProControllerContext& context, const std::vector<MartOrder>& orders){
    //  "May I help you?" with BUY on top, then the list, cursor on its top.
    press(context, BUTTON_A, 2000ms);
    press(context, BUTTON_A, 2000ms);
    for (const MartOrder& order : orders){
        console.log(std::format("Mart: buying {} of row {}.", order.quantity, order.row + 1));
        for (int c = 0; c < order.row; c++){
            tap(context, DPAD_DOWN, 300ms);
        }
        //  "How many?" drops presses while its text prints.
        press(context, BUTTON_A, 2500ms);
        for (int c = 1; c < order.quantity; c++){
            tap(context, DPAD_UP, 340ms);
        }
        //  "That will be..." YES/NO, YES, "Here you are!"; the cursor
        //  goes back to the top.
        press(context, BUTTON_A, 3500ms);
        press(context, BUTTON_A, 2000ms);
        press(context, BUTTON_A, 1500ms);
    }
    press(context, BUTTON_B, 1500ms);
    press(context, BUTTON_B, 1500ms);
    mash_until_overworld(console, context, 20000ms);
}

void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context){
    console.log("New game: from the title screen.");
    NewGameIntro(console, context).run();
}



}
}
}
