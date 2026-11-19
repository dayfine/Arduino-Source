/*  PokemonLZA AutoStory - Menu save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <optional>
#include <string_view>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryMenuReadings.h"
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

bool has_reading(const Pokemon::Observation<Pokemon::ZonePose>& look, std::string_view name){
    return look.readings.find(std::string(name)) != look.readings.end();
}

MenuSaveScreen menu_save_screen(const Pokemon::Observation<Pokemon::ZonePose>& look){
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
    if (look.events.has(ScreenEvent::PLAYER_CAN_MOVE)){
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

} // namespace


Pokemon::Outcome save_from_menu(PlzaBackend& backend){
    MenuSaveProgress progress;
    std::optional<WallClock> start;
    std::optional<MenuSaveScreen> last_screen;
    size_t screens = 0;
    size_t held = 0;
    while (true){
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return Pokemon::outcome_game_not_running();
        }
        if (!start){
            start = look.when;
        }
        const Milliseconds now = std::chrono::duration_cast<Milliseconds>(look.when - *start);
        const MenuSaveScreen screen = menu_save_screen(look);
        if (now > MENU_SAVE_LIMIT){
            return Pokemon::outcome_failed(
                "menu_save_timeout",
                std::format(
                    "not saved and back on the overworld in {} ms: on {}, R pressed {}, confirmed {}, saved {}",
                    MENU_SAVE_LIMIT.count(), screen_name(screen), progress.save_requested, progress.save_confirmed, progress.saved
                )
            );
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
            return Pokemon::outcome_ok();
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
