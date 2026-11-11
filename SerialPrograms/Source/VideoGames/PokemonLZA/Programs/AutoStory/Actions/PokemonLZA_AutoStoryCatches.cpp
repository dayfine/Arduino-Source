/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_MoveEffectivenessSymbol.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryAutosaveDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryPartyRowCount.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using namespace Kernels::Waterfill;


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
//  then the ball's flight, shakes and any loading screen. Returns whether
//  the autosave badge showed meanwhile: the game autosaves on a catch.
bool throw_ball(ConsoleHandle& console, ProControllerContext& context){
    DetectorToFinder<AutosaveDetector> autosave("AutosaveWatcher", 100ms);
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_press_button(context, BUTTON_ZL, 500ms, 100ms);
            pbf_press_button(context, BUTTON_ZL | BUTTON_ZR, 1500ms, 6000ms);
        },
        {autosave}
    );
    if (ret == 0){
        context.wait_for(3000ms);
    }
    context.wait_for_all_requests();
    return ret == 0;
}

//  The party row's count now.
size_t party_row_now(ConsoleHandle& console){
    const VideoSnapshot frame = console.video().snapshot();
    return frame ? party_row_count(frame) : 0;
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

//  Lock-ons and throws so far, and how many brought text or a card after
//  the throw (a catch).
struct ThrowCount{
    int throws = 0;
    int caught = 0;
};

//  ZL held a moment at what is in front: whether it locks on.
bool lock_on_ahead(ConsoleHandle& console, ProControllerContext& context){
    MoveEffectivenessSymbolWatcher lock_on(COLOR_RED, &console.overlay(), 100ms);
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_ZL, 2000ms);
        },
        {lock_on}
    );
    return ret == 0;
}

//  Up to 3 throws at what locks on in front, else at what a full camera
//  turn finds, while the total stays under `max_throws`.
ThrowCount throw_here(ConsoleHandle& console, ProControllerContext& context, ThrowCount count, int max_throws){
    for (int i = 0; i < 3 && count.throws < max_throws; i++){
        if (!lock_on_ahead(console, context) && !scan_for_lock_on(console, context)){
            break;
        }
        //  A catch shows no text (Scatterbug, console B 2026-11-10): it
        //  autosaves, and the party row gains a Pokemon until it is full.
        const size_t party_before = party_row_now(console);
        const bool saved = throw_ball(console, context);
        count.throws++;
        advance_text_and_cards(console, context);
        const size_t party_after = party_row_now(console);
        pbf_press_button(context, BUTTON_L, 160ms, 400ms);
        console.log(std::format(
            "catch_in_zone(): throw {}: autosave {}, party row {} -> {}.",
            count.throws, saved ? "seen" : "not seen", party_before, party_after
        ));
        if (saved || party_after > party_before){
            count.caught++;
        }
    }
    return count;
}

} // namespace


std::optional<ObjectiveMarker> read_objective_marker(Logger& logger, const ImageViewRGB32& screen){
    //  The hexagon's orange, about (245, 175, 40) on console B: the largest
    //  hexagon-sized blob outside the minimap (its marker is there too).
    const double scale = screen.height() / 1080.0;
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(screen, 220, 255, 140, 215, 0, 90);
    std::unique_ptr<WaterfillSession> session = make_WaterfillSession(matrix);
    std::unique_ptr<WaterfillIterator> iter = session->make_iterator((size_t)(500 * scale * scale));
    std::optional<WaterfillObject> best;
    WaterfillObject object;
    while (iter->find_next(object, false)){
        const double x = object.center_of_gravity_x() / screen.width();
        const double y = object.center_of_gravity_y() / screen.height();
        const bool in_minimap = x < 0.20 && y < 0.45;
        const bool in_party_bar = y > 0.70;
        const double aspect = object.aspect_ratio();
        if (in_minimap || in_party_bar || object.area > 4000 * scale * scale || aspect < 0.6 || aspect > 1.4){
            continue;
        }
        if (!best || object.area > best->area){
            best = object;
        }
    }
    if (!best){
        return std::nullopt;
    }
    //  The distance: white digits just above the hexagon, up to its width
    //  out each side, about half its height tall.
    const double w = (double)best->width();
    const double h = (double)best->height();
    const ImagePixelBox digits(
        (size_t)std::max(0.0, best->min_x - w), (size_t)std::max(0.0, best->min_y - 0.95 * h),
        (size_t)std::min((double)screen.width(), best->max_x + w), (size_t)std::max(1.0, best->min_y - 0.10 * h)
    );
    ObjectiveMarker marker;
    marker.x = (best->min_x + best->max_x) / 2.0 / screen.width();
    marker.distance = OCR::read_number_waterfill(logger, extract_box_reference(screen, digits), 0xffd0d0d0, 0xffffffff);
    return marker;
}

std::optional<int> walk_to_objective(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit){
    PlzaScreenObserver observer;
    const WallClock deadline = current_time() + limit;
    int unseen = 0;
    std::optional<int> last;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        if (observer.observe(frame, current_time()).has(Pokemon::ScreenEvent::INTERACT_PROMPT) && stop_at <= 0){
            return last;
        }
        const std::optional<ObjectiveMarker> marker = read_objective_marker(console, frame);
        if (!marker){
            //  Behind us: turn the camera until it shows.
            if (++unseen > 12){
                return std::nullopt;
            }
            pbf_move_right_joystick(context, {0.5, 0}, 500ms, 200ms);
            continue;
        }
        unseen = 0;
        if (marker->distance > 0){
            if (last != marker->distance){
                console.log(std::format("walk_to_objective(): {} to go, marker at x = {:.2f}.", marker->distance, marker->x));
            }
            last = marker->distance;
            if (marker->distance <= stop_at){
                console.log(std::format("walk_to_objective(): {} to the objective.", marker->distance));
                return last;
            }
        }
        const double off = marker->x - 0.5;
        if (off > 0.06 || off < -0.06){
            pbf_move_right_joystick(context, {off > 0 ? 0.4 : -0.4, 0}, Milliseconds((int)(std::min(1.0, std::abs(off) * 4) * 400)) + 80ms, 150ms);
        }
        pbf_move_left_joystick(context, {0, 1}, 600ms, 0ms);
    }
    console.log(std::format("walk_to_objective(): not within {} in time (last read {}).", stop_at, last.value_or(-1)));
    return last;
}

void catch_in_zone(ConsoleHandle& console, ProControllerContext& context, int catches_wanted, int max_throws){
    //  Toward the objective (the zone's far gate), stopping at 100 and 60
    //  to throw at what is in front: 03.07 starts at 127 (console B,
    //  2026-11-10), with nothing to catch at the start.
    //  Taunie's two rocks stand between the start and the marker: a step
    //  right first, around them, from the start (a catch's autosave can
    //  leave the save further on).
    context.wait_for_all_requests();
    const std::optional<ObjectiveMarker> start = read_objective_marker(console, console.video().snapshot());
    if (start && start->distance >= 115){
        pbf_move_left_joystick(context, {1, 0}, 1000ms, 300ms);
    }
    ThrowCount count;
    for (int stop_at : {100, 60}){
        if (count.caught >= catches_wanted){
            break;
        }
        walk_to_objective(console, context, stop_at, 30s);
        count = throw_here(console, context, count, max_throws);
    }
    console.log(std::format("catch_in_zone(): {} catches in {} throws.", count.caught, count.throws));
    if (count.caught < catches_wanted){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("catch_in_zone(): {} catches of {} in {} throws.", count.caught, catches_wanted, count.throws),
            console
        );
    }
}

void walk_to_zone_gate(ConsoleHandle& console, ProControllerContext& context){
    walk_to_objective(console, context, 0, 60s);
    PlzaScreenObserver observer;
    context.wait_for_all_requests();
    if (!observer.observe(console.video().snapshot(), current_time()).has(Pokemon::ScreenEvent::INTERACT_PROMPT)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_to_zone_gate(): no A prompt at the objective.",
            console
        );
    }
}

void catch_tutorial(ConsoleHandle& console, ProControllerContext& context, int max_throws){
    for (int i = 0; i < max_throws; i++){
        throw_ball(console, context);
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
        throw_ball(console, context);
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
