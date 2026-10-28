/*  BDSP AutoStory - Pokemon Center
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <optional>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_PokemonCenter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


namespace{

//  How many tiles the 1080p view spans across: a tile is about 125 px.
constexpr double SCREEN_WIDTH_TILES = 1920.0 / 125;

//  Where a building's lit doorway is across the screen, 0 to 1: the warm
//  white columns in the band where doors stand, taking the run of them
//  nearest the middle (a second building's door may show further off).
//  Nothing if no doorway shows.
std::optional<double> lit_doorway_x(const ImageViewRGB32& screen){
    const ImageRGB32 small = screen.scale_to(480, 270);
    const size_t width = small.width();
    std::vector<size_t> counts(width, 0);
    for (size_t y = 81; y < 157; y++){
        for (size_t x = 0; x < width; x++){
            const uint32_t pixel = small.pixel(x, y);
            const int r = (pixel >> 16) & 0xff, g = (pixel >> 8) & 0xff, b = pixel & 0xff;
            if (r > 235 && g > 215 && b > 120 && b < 215 && r - b > 40){
                counts[x]++;
            }
        }
    }
    std::optional<double> best;
    double best_distance = 0;
    for (size_t x = 0; x < width; ){
        if (counts[x] < 4){
            x++;
            continue;
        }
        double mass = 0, moment = 0;
        size_t gap = 0;
        for (; x < width && gap <= 3; x++){
            if (counts[x] >= 4){
                mass += (double)counts[x];
                moment += (double)counts[x] * (double)x;
                gap = 0;
            }else{
                gap++;
            }
        }
        const double center = moment / mass / (double)width;
        const double distance = std::abs(center - 0.5);
        if (distance < 0.35 && (!best || distance < best_distance)){
            best = center;
            best_distance = distance;
        }
    }
    return best;
}

}


void heal_at_pokemon_center(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
){
    //  Movement is not locked to tiles, and the door takes us in only when we
    //  are lined up with it. Its doorway is lit: find it on screen, where the
    //  camera keeps the player in the middle, move across by its offset, and
    //  walk in once it is straight ahead.
    route_status(stream, label + ": lining up with the door.");
    bool inside = false;
    for (size_t tries = 0; tries < 12 && !inside; tries++){
        context.wait_for_all_requests();
        VideoSnapshot outside = stream.video().snapshot();
        if (!outside){
            continue;
        }
        const std::optional<double> doorway = lit_doorway_x(*outside.frame);
        if (!doorway){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                label + ": no lit doorway on screen.", stream);
        }
        const double tiles = (*doorway - 0.5) * SCREEN_WIDTH_TILES;
        stream.log("[Route] " + label + ": doorway " + std::to_string(tiles) + " tiles across.");
        if (std::abs(tiles) > 0.25){
            const GridDirection side = tiles > 0 ? GridDirection::EAST : GridDirection::WEST;
            route_turn(stream, context, side);
            const Milliseconds hold = std::max<Milliseconds>(Milliseconds(40),
                Milliseconds((int64_t)(std::abs(tiles) * ROUTE_MS_PER_TILE_ACROSS.count())));
            pbf_move_left_joystick(context, {tiles > 0 ? 1.0 : -1.0, 0}, hold, 300ms);
            continue;
        }
        inside = walk_until(stream, context, 0, 1, 1500ms, label + ": into the door").stop == WalkStop::TRANSITION;
        context.wait_for_all_requests();
        VideoSnapshot now = stream.video().snapshot();
        //  The door's fade is not always black enough to read as a transition;
        //  a whole new scene says we are in just as well.
        if (!inside && now && walk_screen_change(*outside.frame, *now.frame) > WALK_SCENE_CHANGE){
            stream.log("[Route] " + label + ": the scene changed; inside.");
            inside = true;
        }
    }
    if (!inside){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": door not found.", stream);
    }
    pbf_wait(context, 1500ms);

    //  The nurse's counter is straight ahead of the door.
    route_status(stream, label + ": talking to the nurse.");
    if (walk_to_talk(stream, context, 0, 1, 6000ms, label + ": to the nurse").stop != WalkStop::DIALOG){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": nurse not reached.", stream);
    }
    //  A through her lines; the cursor starts on "Yes". Stop once no dialogue
    //  has shown for a while, so A does not start the talk over.
    size_t quiet = 0;
    for (size_t presses = 0; presses < 30 && quiet < 2; presses++){
        ShortDialogWatcher dialog(COLOR_YELLOW);
        if (wait_until(stream, context, 1500ms, {{dialog}}) == 0){
            quiet = 0;
            pbf_press_button(context, BUTTON_A, 80ms, 700ms);
        }else{
            quiet++;
        }
    }
    context.wait_for_all_requests();

    route_status(stream, label + ": healed; walking out.");
    //  Her last line may still be up, and it would stop the walk.
    pbf_mash_button(context, BUTTON_B, 2000ms);
    context.wait_for_all_requests();
    VideoSnapshot counter = stream.video().snapshot();
    bool faded_out = false;
    bool scene_changed = false;
    for (size_t tries = 0; tries < 3 && !faded_out && !scene_changed; tries++){
        const WalkResult walk = walk_until(stream, context, 0, -1, 6000ms, label + ": out the door");
        faded_out = walk.stop == WalkStop::TRANSITION;
        if (walk.stop == WalkStop::DIALOG){
            pbf_mash_button(context, BUTTON_B, 2000ms);
        }
        context.wait_for_all_requests();
        VideoSnapshot now = stream.video().snapshot();
        scene_changed = counter && now && walk_screen_change(*counter.frame, *now.frame) > WALK_SCENE_CHANGE;
    }
    if (!faded_out && !scene_changed){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": did not get back out.", stream);
    }
    pbf_wait(context, 1500ms);
    context.wait_for_all_requests();
}



}
}
}
