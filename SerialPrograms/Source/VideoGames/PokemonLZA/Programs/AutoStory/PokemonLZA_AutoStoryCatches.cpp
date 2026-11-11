/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_MoveEffectivenessSymbol.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  What advance_text_and_cards() pressed A through.
struct ScreensPassed{
    bool card = false;
    bool text = false;
};

//  A through text and tutorial cards until neither has shown for 3 s
//  (30 s at most).
ScreensPassed advance_text_and_cards(ConsoleHandle& console, ProControllerContext& context){
    FlatWhiteDialogDetector white_dialog;
    BlueDialogDetector blue_dialog;
    TealDialogDetector teal_dialog;
    ScreensPassed passed;
    WallClock last_busy = current_time();
    const WallClock deadline = last_busy + 30s;
    while (current_time() < deadline && current_time() - last_busy < 3s){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        const bool card_now = screen && tutorial_card_shows(screen);
        if (card_now || white_dialog.detect(screen) || blue_dialog.detect(screen) || teal_dialog.detect(screen)){
            passed.card = passed.card || card_now;
            passed.text = passed.text || !card_now;
            last_busy = current_time();
            pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        }else{
            context.wait_for(250ms);
        }
    }
    return passed;
}

//  ZL+ZR held 1.5 s throws a ball at the locked target (0.5 s only aims),
//  then the ball's flight, shakes and any loading screen.
void throw_ball(ProControllerContext& context){
    pbf_press_button(context, BUTTON_ZL, 500ms, 100ms);
    pbf_press_button(context, BUTTON_ZL | BUTTON_ZR, 1500ms, 6000ms);
    context.wait_for_all_requests();
}

//  Turns the camera in place (10 x 1 s), mashing ZL, until the move
//  symbols of a lock-on show (InPlaceCatcher's scan). Returns whether
//  something locked on within one full turn.
bool scan_for_lock_on(ConsoleHandle& console, ProControllerContext& context){
    MoveEffectivenessSymbolWatcher lock_on(COLOR_RED, &console.overlay(), 100ms);
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            for (int c = 0; c < 10; c++){
                ssf_press_right_joystick(context, {-0.5, 0}, 0ms, 1000ms, 0ms);
                pbf_mash_button(context, BUTTON_ZL, 1000ms);
            }
        },
        {lock_on}
    );
    return ret == 0;
}

//  Turns the player to `heading` (minimap, clockwise from north): L
//  recenters the camera behind the player, a 150 ms stick tap turns, L
//  again; up to 4 times, within 20 degrees.
void face_compass(ConsoleHandle& console, ProControllerContext& context, double heading){
    for (int attempt = 0; attempt < 4; attempt++){
        const double now = get_facing_direction(console, context);
        if (get_angle_between_facing_directions(now, heading) <= 20){
            return;
        }
        const double turn = (heading - now) * 3.14159265358979 / 180;
        pbf_press_button(context, BUTTON_L, 160ms, 400ms);
        pbf_move_left_joystick(context, {std::sin(turn), std::cos(turn)}, 150ms, 400ms);
        pbf_press_button(context, BUTTON_L, 160ms, 400ms);
        context.wait_for_all_requests();
    }
    console.log(std::format("catch_in_zone(): heading {:.0f} not reached.", heading));
}

//  Lock-ons and throws so far, and how many brought text or a card after
//  the throw (a catch).
struct ThrowCount{
    int throws = 0;
    int caught = 0;
};

//  Up to 3 lock-ons and throws from where the player stands, while the
//  total stays under `max_throws`.
ThrowCount throw_at_what_locks(ConsoleHandle& console, ProControllerContext& context, ThrowCount count, int max_throws){
    for (int i = 0; i < 3 && count.throws < max_throws; i++){
        if (!scan_for_lock_on(console, context)){
            break;
        }
        throw_ball(context);
        count.throws++;
        const ScreensPassed passed = advance_text_and_cards(console, context);
        if (passed.card || passed.text){
            count.caught++;
            console.log(std::format("catch_in_zone(): throw {}: text after it, a catch.", count.throws));
        }
        pbf_press_button(context, BUTTON_L, 160ms, 400ms);
    }
    return count;
}

} // namespace


void catch_in_zone(ConsoleHandle& console, ProControllerContext& context, int catches_wanted, int max_throws){
    //  Spokes out from the start and back, relative to its heading: ahead,
    //  then alternating left and right (the zig-zag).
    static const double SPOKES[] = {0, -60, 60, -120, 120, 180};
    pbf_press_button(context, BUTTON_L, 160ms, 400ms);
    context.wait_for_all_requests();
    const double start_heading = get_facing_direction(console, context);
    //  Nothing spawns at 03.07's start (console B, 2026-11-10): out into
    //  the zone first.
    pbf_move_left_joystick(context, {0, 1}, 5000ms, 300ms);
    ThrowCount count = throw_at_what_locks(console, context, ThrowCount(), max_throws);
    for (double spoke : SPOKES){
        if (count.caught >= catches_wanted || count.throws >= max_throws){
            break;
        }
        face_compass(console, context, std::fmod(start_heading + spoke + 360, 360));
        pbf_move_left_joystick(context, {0, 1}, 3000ms, 300ms);
        count = throw_at_what_locks(console, context, count, max_throws);
        //  Back to the start: face the way out, then walk toward the camera.
        face_compass(console, context, std::fmod(start_heading + spoke + 360, 360));
        pbf_move_left_joystick(context, {0, -1}, 3000ms, 300ms);
    }
    face_compass(console, context, start_heading);
    pbf_move_left_joystick(context, {0, -1}, 5000ms, 300ms);
    face_compass(console, context, start_heading);
    console.log(std::format("catch_in_zone(): {} catches in {} throws.", count.caught, count.throws));
    if (count.caught < catches_wanted){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("catch_in_zone(): {} catches of {} in {} throws.", count.caught, catches_wanted, count.throws),
            console
        );
    }
}

void catch_tutorial(ConsoleHandle& console, ProControllerContext& context, int max_throws){
    for (int i = 0; i < max_throws; i++){
        throw_ball(context);
        if (advance_text_and_cards(console, context).card){
            console.log(std::format("catch_tutorial(): caught on throw {}.", i + 1));
            return;
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("catch_tutorial(): no catch in {} throws.", max_throws),
        console
    );
}

void battle_then_catch(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int max_rounds
){
    //  D-pad up is ignored while Bunnelby's "weaken it" banner shows.
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    advance_text_and_cards(console, context);
    pbf_press_dpad(context, DPAD_UP, 100ms, 2000ms);
    TrainerBattleState battle_state(battle_ai);
    for (int round = 0; round < max_rounds; round++){
        int landed = 0;
        int misses = 0;
        int sends = 0;
        for (int tries = 0; tries < 12 && landed < 3; tries++){
            if (battle_state.attempt_one_attack(env, console, context)){
                landed++;
                misses = 0;
            }else if (++misses == 2 && sends < 2){
                //  No move symbols twice: the partner may not be out.
                console.log("battle_then_catch(): no lock-on twice: D-pad up to send the partner out.");
                pbf_press_dpad(context, DPAD_UP, 100ms, 2000ms);
                misses = 0;
                sends++;
            }
        }
        if (landed == 0 && round > 0){
            console.log(std::format("battle_then_catch(): nothing to lock on in round {}: caught.", round + 1));
            return;
        }
        advance_text_and_cards(console, context);
        throw_ball(context);
        advance_text_and_cards(console, context);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("battle_then_catch(): still a target after {} rounds.", max_rounds),
        console
    );
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
