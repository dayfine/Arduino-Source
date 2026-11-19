/*  PokemonLZA AutoStory - Menu save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <optional>
#include <string_view>
#include <utility>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryMenuReadings.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryMenuSave.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

constexpr Milliseconds MENU_SAVE_POLL = 250ms;
//  Between repeated X or R presses: the menu takes a moment to show, and a
//  second X closes it.
constexpr Milliseconds MENU_OPEN_WAIT = 2000ms;
constexpr Milliseconds MENU_SAVE_LIMIT = 30000ms;
constexpr size_t MENU_SAVE_HELD_LOOKS = 2;

enum class MenuSaveScreen{
    UNKNOWN,
    OVERWORLD,
    TUTORIAL_CARD,
    CHOICE,
    TEXT,
    SAVE_BUTTON,
    MAIN_MENU,
};

std::string_view screen_name(MenuSaveScreen screen){
    switch (screen){
    case MenuSaveScreen::OVERWORLD:
        return "overworld";
    case MenuSaveScreen::TUTORIAL_CARD:
        return "tutorial card";
    case MenuSaveScreen::CHOICE:
        return "choice";
    case MenuSaveScreen::TEXT:
        return "text box";
    case MenuSaveScreen::SAVE_BUTTON:
        return "save selected";
    case MenuSaveScreen::MAIN_MENU:
        return "main menu";
    default:
        return "nothing read";
    }
}

bool has_reading(const ScreenObservation& look, std::string_view name){
    return look.readings.find(std::string(name)) != look.readings.end();
}

MenuSaveScreen menu_save_screen(const ScreenObservation& look){
    if (has_reading(look, TUTORIAL_CARD_READING)){
        return MenuSaveScreen::TUTORIAL_CARD;
    }
    if (look.events.has(ScreenEvent::CHOICE_BOX)){
        return MenuSaveScreen::CHOICE;
    }
    if (look.events.has(ScreenEvent::DIALOG_BOX)){
        return MenuSaveScreen::TEXT;
    }
    if (has_reading(look, MENU_SAVE_BUTTON_READING)){
        return MenuSaveScreen::SAVE_BUTTON;
    }
    if (has_reading(look, MAIN_MENU_READING)){
        return MenuSaveScreen::MAIN_MENU;
    }
    //  01.01's platform shows no party bar.
    if (look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || has_reading(look, PLATFORM_READING)){
        return MenuSaveScreen::OVERWORLD;
    }
    return MenuSaveScreen::UNKNOWN;
}

struct MenuSaveProgress{
    bool save_requested = false;
    bool save_confirmed = false;
    //  The save's text closed, or the main menu back, after the confirmation.
    bool saved = false;
    std::optional<Milliseconds> last_x = std::nullopt;
    std::optional<Milliseconds> last_r = std::nullopt;
};

bool too_soon(const std::optional<Milliseconds>& last, Milliseconds now){
    return last && now - *last < MENU_OPEN_WAIT;
}

//  nullopt: wait.
std::optional<Button> menu_save_press(MenuSaveScreen screen, const MenuSaveProgress& progress, Milliseconds now){
    switch (screen){
    case MenuSaveScreen::OVERWORLD:
        if (too_soon(progress.last_x, now)){
            return std::nullopt;
        }
        return BUTTON_X;
    case MenuSaveScreen::TUTORIAL_CARD:
    case MenuSaveScreen::CHOICE:
        return BUTTON_A;
    case MenuSaveScreen::TEXT:
        return progress.save_confirmed ? BUTTON_B : BUTTON_A;
    case MenuSaveScreen::SAVE_BUTTON:
        return progress.saved ? BUTTON_B : BUTTON_A;
    case MenuSaveScreen::MAIN_MENU:
        if (progress.saved || progress.save_confirmed){
            return BUTTON_B;
        }
        if (too_soon(progress.last_r, now)){
            return std::nullopt;
        }
        return BUTTON_R;
    default:
        return std::nullopt;
    }
}

void note_press(MenuSaveScreen screen, Button button, Milliseconds now, MenuSaveProgress& progress){
    if (button == BUTTON_X){
        progress.last_x = now;
    }
    if (button == BUTTON_R){
        progress.last_r = now;
        progress.save_requested = true;
    }
    const bool confirm_screen = screen == MenuSaveScreen::SAVE_BUTTON || screen == MenuSaveScreen::CHOICE;
    if (confirm_screen && button == BUTTON_A && progress.save_requested){
        progress.save_confirmed = true;
    }
    if ((screen == MenuSaveScreen::TEXT || screen == MenuSaveScreen::MAIN_MENU) && button == BUTTON_B){
        progress.saved = true;
    }
}

//  From the main menu to the options' text speed, set to Fast; each with
//  its release.
constexpr std::pair<Button, Milliseconds> TEXT_SPEED_PRESSES[] = {
    {BUTTON_L, 600ms},
    {BUTTON_UP, 300ms},
    {BUTTON_A, 1100ms},
    {BUTTON_DOWN, 300ms},
    {BUTTON_RIGHT, 300ms},
};

//  Before the rows: X on the overworld, A on the tutorial card. After: B
//  until the overworld. nullopt: wait (or, before the rows on the main
//  menu, send them).
std::optional<Button> text_speed_press(MenuSaveScreen screen, bool rows_sent, const std::optional<Milliseconds>& last_x, Milliseconds now){
    if (rows_sent){
        return screen == MenuSaveScreen::OVERWORLD ? std::nullopt : std::optional<Button>(BUTTON_B);
    }
    if (screen == MenuSaveScreen::TUTORIAL_CARD){
        return BUTTON_A;
    }
    if (screen == MenuSaveScreen::OVERWORLD && !too_soon(last_x, now)){
        return BUTTON_X;
    }
    return std::nullopt;
}

void press_text_speed_rows(PlzaBackend& backend){
    for (const auto& [button, release] : TEXT_SPEED_PRESSES){
        backend.press(button, /*hold=*/100ms, release);
    }
}

//  The looks also bound the step: a script's time can stand still.
constexpr size_t TEXT_SPEED_MAX_LOOKS = MENU_SAVE_LIMIT / MENU_SAVE_POLL + 1;

//  The screen of the latest looks and how many looks in a row it showed.
struct HeldScreen{
    std::optional<MenuSaveScreen> screen = std::nullopt;
    size_t looks = 0;
};

//  `held` after one more look at `screen`; logs a new screen.
HeldScreen held_after(PlzaBackend& backend, const HeldScreen& held, MenuSaveScreen screen, Milliseconds now){
    if (screen == held.screen){
        return {.screen = screen, .looks = held.looks + 1};
    }
    backend.log_detail(std::format("[PLZA set_text_speed_fast] {} at {:.1f}s", screen_name(screen), now.count() / 1000.0));
    return {.screen = screen, .looks = 1};
}

} // namespace


std::optional<Pokemon::StoryFailure> set_text_speed_fast(PlzaBackend& backend){
    bool rows_sent = false;
    std::optional<Milliseconds> last_x;
    std::optional<WallClock> start;
    HeldScreen held;
    for (size_t looks = 1; ; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return Pokemon::game_not_running_failure();
        }
        start = start.value_or(look.when);
        const Milliseconds now = std::chrono::duration_cast<Milliseconds>(look.when - *start);
        const MenuSaveScreen screen = menu_save_screen(look);
        if (now > MENU_SAVE_LIMIT || looks > TEXT_SPEED_MAX_LOOKS){
            return Pokemon::StoryFailure{.code = "text_speed_timeout", .detail = std::format(
                "not back on the overworld in {} ms: on {}, text speed rows sent {}", MENU_SAVE_LIMIT.count(), screen_name(screen), rows_sent
            )};
        }
        held = held_after(backend, held, screen, now);
        if (held.looks < MENU_SAVE_HELD_LOOKS){
            backend.wait(MENU_SAVE_POLL);
            continue;
        }
        if (rows_sent && screen == MenuSaveScreen::OVERWORLD){
            backend.log(std::format("[PLZA set_text_speed_fast] text speed set, back on the overworld at {:.1f}s", now.count() / 1000.0));
            return std::nullopt;
        }
        if (!rows_sent && screen == MenuSaveScreen::MAIN_MENU){
            press_text_speed_rows(backend);
            rows_sent = true;
            held.looks = 0;
            continue;
        }
        const std::optional<Button> button = text_speed_press(screen, rows_sent, last_x, now);
        if (!button){
            backend.wait(MENU_SAVE_POLL);
            continue;
        }
        backend.press(*button, /*hold=*/160ms, /*release=*/240ms);
        if (*button == BUTTON_X){
            last_x = now;
        }
        held.looks = 0;
    }
}


std::optional<Pokemon::StoryFailure> save_from_menu(PlzaBackend& backend){
    MenuSaveProgress progress;
    std::optional<WallClock> start;
    std::optional<MenuSaveScreen> last_screen;
    size_t screens = 0;
    size_t held = 0;
    while (true){
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return Pokemon::game_not_running_failure();
        }
        if (!start){
            start = look.when;
        }
        const Milliseconds now = std::chrono::duration_cast<Milliseconds>(look.when - *start);
        const MenuSaveScreen screen = menu_save_screen(look);
        if (now > MENU_SAVE_LIMIT){
            return Pokemon::StoryFailure{.code = "menu_save_timeout", .detail =
                std::format(
                    "not saved and back on the overworld in {} ms: on {}, R pressed {}, confirmed {}, saved {}",
                    MENU_SAVE_LIMIT.count(), screen_name(screen), progress.save_requested, progress.save_confirmed, progress.saved
                )
            };
        }
        if (screen != last_screen){
            screens++;
            backend.log_detail(std::format(
                "[PLZA save_from_menu] screen {}: {} at {:.1f}s", screens, screen_name(screen), now.count() / 1000.0
            ));
            last_screen = screen;
            held = 0;
        }
        held++;
        if (screen == MenuSaveScreen::OVERWORLD && progress.saved){
            backend.log(std::format("[PLZA save_from_menu] saved, back on the overworld at {:.1f}s", now.count() / 1000.0));
            return std::nullopt;
        }
        const std::optional<Button> button = held < MENU_SAVE_HELD_LOOKS
            ? std::nullopt
            : menu_save_press(screen, progress, now);
        if (!button){
            backend.wait(MENU_SAVE_POLL);
            continue;
        }
        backend.press(*button, /*hold=*/160ms, /*release=*/240ms);
        note_press(screen, *button, now, progress);
        held = 0;
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
