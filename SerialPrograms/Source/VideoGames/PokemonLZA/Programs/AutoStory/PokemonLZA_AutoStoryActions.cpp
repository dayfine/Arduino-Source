/*  PokemonLZA AutoStory - Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <numbers>
#include <utility>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_RunFromBattleDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_FastTravelNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "PokemonLZA_AutoStoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


StoryActionNotImplemented::StoryActionNotImplemented(std::string message)
    : OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, std::move(message))
{}

void throw_not_implemented(ConsoleHandle& console, std::string_view action, std::string_view todo){
    const std::string message = std::format("PLZA AutoStory: {} is not implemented yet. TODO: {}", action, todo);
    console.log(message, COLOR_RED);
    throw StoryActionNotImplemented(message);
}


void mash_to_overworld(ConsoleHandle& console, ProControllerContext& context, Button button, Milliseconds timeout){
    //  The party list shows between lines of a scene too; a second of it is
    //  the overworld.
    OverworldPartySelectionWatcher overworld(COLOR_WHITE, &console.overlay(), 1000ms);
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [button, timeout](ProControllerContext& context){
            pbf_mash_button(context, button, timeout);
        },
        {overworld}
    );
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("mash_to_overworld(): no overworld after {} ms.", timeout.count()),
            console
        );
    }
}

void talk_to_facing(ConsoleHandle& console, ProControllerContext& context){
    FlatWhiteDialogWatcher white(COLOR_RED, &console.overlay());
    BlueDialogWatcher blue(COLOR_BLUE, &console.overlay());
    TealDialogWatcher teal(COLOR_GREEN, &console.overlay());
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_press_button(context, BUTTON_A, 160ms, 5000ms);
        },
        {white, blue, teal}
    );
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "talk_to_facing(): A opened no dialogue.",
            console
        );
    }
}


const ImageFloatBox& dialog_choice_box(){
    static const ImageFloatBox box{0.654308, 0.481553, 0.295529, 0.312621};
    return box;
}

namespace{

//  The selection arrow's top among the choices, once it has held still;
//  none if it does not show within `timeout`.
std::optional<double> wait_for_choice_arrow(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout){
    SelectionArrowWatcher arrow(COLOR_YELLOW, &console.overlay(), SelectionArrowType::RIGHT, dialog_choice_box());
    context.wait_for_all_requests();
    if (wait_until(console, context, timeout, {arrow}) < 0){
        return std::nullopt;
    }
    return arrow.last_detected().y;
}

} // namespace

void choose_dialog_option(ConsoleHandle& console, ProControllerContext& context, size_t index){
    std::optional<double> row = wait_for_choice_arrow(console, context, 10s);
    if (!row){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "choose_dialog_option(): no choices on screen.",
            console
        );
    }
    for (size_t moved = 0; moved < index; moved++){
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 240ms);
        const std::optional<double> next = wait_for_choice_arrow(console, context, 2s);
        if (!next || *next <= *row){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::format("choose_dialog_option(): the arrow did not move down to choice {}.", moved + 1),
                console
            );
        }
        row = next;
    }
    pbf_press_button(context, BUTTON_A, 160ms, 240ms);
    context.wait_for_all_requests();
}


double read_heading(ConsoleHandle& console, ProControllerContext& context){
    return get_facing_direction(console, context);
}

namespace{

//  Recenters the camera behind the player (L), then taps the stick
//  `degrees` clockwise of straight ahead, which turns the player that way
//  (and steps it a little).
void turn_by(ProControllerContext& context, double degrees){
    const double radians = degrees * std::numbers::pi / 180.0;
    pbf_press_button(context, BUTTON_L, 100ms, 400ms);
    pbf_move_left_joystick(context, {std::sin(radians), std::cos(radians)}, 100ms, 300ms);
}

} // namespace

double face_heading(ConsoleHandle& console, ProControllerContext& context, double heading, double tolerance){
    double now = get_facing_direction(console, context);
    for (size_t attempt = 0; attempt < 4 && get_angle_between_facing_directions(now, heading) > tolerance; attempt++){
        turn_by(context, heading - now);
        context.wait_for_all_requests();
        now = get_facing_direction(console, context);
    }
    if (get_angle_between_facing_directions(now, heading) > tolerance){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("face_heading(): facing {:.0f} deg, wanted {:.0f} +/- {:.0f}.", now, heading, tolerance),
            console
        );
    }
    return now;
}

void run_story_move(ConsoleHandle& console, ProControllerContext& context, const StoryMove& move){
    if (move.heading){
        face_heading(console, context, *move.heading, 10.0);
    }
    context.wait_for_all_requests();
    if (run_a_straight_path_in_overworld(console, context, 0, 1, move.duration) == 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "run_story_move(): a day/night change cut the run short.",
            console
        );
    }
}


void fly_to(ConsoleHandle& console, ProControllerContext& context, Language language, Location location){
    const FastTravelState state = open_map_and_fly_to(console, context, language, location);
    if (state != FastTravelState::SUCCESS){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format(
                "fly_to(): could not fly to {} (state {}).",
                get_location_item_from_enum(location).slug, static_cast<int>(state)
            ),
            console
        );
    }
}


void fight_battle(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai
){
    RunFromBattleWatcher battle_menu(COLOR_GREEN, &console.overlay(), 10ms);
    OverworldPartySelectionWatcher overworld(COLOR_WHITE, &console.overlay(), 100ms);
    TrainerBattleState battle_state(battle_ai);
    //  The battle's text first: the overworld only ends it once it has
    //  begun, and it must begin within a minute.
    const WallClock start_deadline = current_time() + 60s;
    bool started = false;
    while (true){
        context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& context){
                pbf_mash_button(context, BUTTON_B, 120s);
            },
            {battle_menu, overworld}
        );
        if (ret == 0){
            started = true;
            if (!battle_state.attempt_one_attack(env, console, context)){
                pbf_press_button(context, BUTTON_UP, 500ms, 1000ms);
            }
            continue;
        }
        if (ret == 1 && started){
            return;
        }
        if (ret < 0 || current_time() > start_deadline){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "fight_battle(): no battle began within a minute, or its menu was gone for two.",
                console
            );
        }
        //  The overworld before the battle: its text has not started it yet.
        context.wait_for(500ms);
    }
}


void heal_at_pokemon_center(ConsoleHandle& console, ProControllerContext&){
    throw_not_implemented(
        console, "heal_at_pokemon_center",
        "talk to the nurse: no LZA program heals at a Pokemon Center yet."
    );
}

void buy_items(ConsoleHandle& console, ProControllerContext&, const std::vector<std::string>& items){
    throw_not_implemented(
        console, std::format("buy_items ({} items)", items.size()),
        "the Center counter's shop menu. StallBuyer's purchase loop is for stalls and lives in that program."
    );
}


void save_story_game(ConsoleHandle& console, ProControllerContext& context){
    if (!save_game_to_menu(console, context)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "save_story_game(): the game did not save.",
            console
        );
    }
    mash_to_overworld(console, context, BUTTON_B, 10s);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
