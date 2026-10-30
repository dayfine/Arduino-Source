/*  BDSP AutoStory - Segment 01 Shared
 *  Helpers used by more than one Segment 01 checkpoint.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"

#include "../Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "../Utils/PokemonBDSP_AutoStory_StoryBattle.h"

#include "PokemonBDSP_AutoStory_Segment_01_Shared.h"

using namespace std::chrono_literals;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;


[[noreturn]] void fail(VideoStream& stream, const std::string& message){
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, message, stream);
}

//  True if a dialogue opens within `timeout`: someone who stopped us as the
//  walk ended reads as BLOCKED, and their box comes up a moment later.
bool dialogue_opens(VideoStream& stream, ProControllerContext& context, Milliseconds timeout){
    context.wait_for_all_requests();
    ShortDialogWatcher dialog(COLOR_CYAN);
    return wait_until(stream, context, timeout, {{dialog}}) == 0;
}

//  For a dialogue that has opened: B through it. A trainer's ends in a
//  battle, which is fought with the best move (Potions when HP runs low),
//  followed by a Potion if HP is under half. Anyone else's ends back in the
//  overworld. Returns whether there was a battle. Between a trainer's last
//  line and the battle the overworld shows with no dialogue for a moment,
//  so "only talked" needs 4 s of it, and never comes when `battle_expected`.
//  `doubles` for tag and twin battles. A lost battle ends in the last
//  Pokemon Center; see in_pokemon_center().
bool talk_or_fight(VideoStream& stream, ProControllerContext& context, const std::string& label, bool battle_expected, bool doubles){
    ShortDialogDetector dialog;
    OverworldDetector overworld;
    BattleMenuDetector battle_menu(BattleType::TRAINER);
    BlackScreenDetector black;
    auto quiet_since = std::chrono::steady_clock::now();
    for (size_t check = 0; check < 120; check++){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        if (!frame){
            fail(stream, label + ": No video frame.");
        }
        if (battle_menu.detect(*frame.frame) || black.detect(*frame.frame)){
            route_status(stream, "Fighting " + label + ".");
            fight_story_battle(stream, context, label, doubles);
            if (!in_pokemon_center(stream)){
                use_potion_first_pokemon(stream, context, 0.5);
            }
            return true;
        }
        const bool talking = dialog.detect(*frame.frame);
        if (!talking && overworld.detect(*frame.frame)){
            if (!battle_expected && std::chrono::steady_clock::now() - quiet_since >= 4s){
                route_status(stream, label + " only talked.");
                return false;
            }
            pbf_wait(context, 300ms);
            continue;
        }
        quiet_since = std::chrono::steady_clock::now();
        if (talking){
            pbf_press_button(context, BUTTON_B, 80ms, 1100ms);
        }else{
            pbf_wait(context, 300ms);
        }
    }
    fail(stream, label + ": neither a battle nor the overworld after the dialogue.");
}

//  Holds the stick toward `dir` for `tiles` tiles, turning first.
void hold_tiles(VideoStream& stream, ProControllerContext& context, Direction dir, size_t tiles){
    route_turn(stream, context, dir);
    const double x = dir == Direction::EAST ? 1 : dir == Direction::WEST ? -1 : 0;
    const double y = dir == Direction::NORTH ? 1 : dir == Direction::SOUTH ? -1 : 0;
    pbf_move_left_joystick(context, {x, y}, route_open_hold(dir, tiles), 200ms);
    context.wait_for_all_requests();
}

WalkResult walk_dir(VideoStream& stream, ProControllerContext& context, Direction dir, Milliseconds limit, const std::string& label){
    const double x = dir == Direction::EAST ? 1 : dir == Direction::WEST ? -1 : 0;
    const double y = dir == Direction::NORTH ? 1 : dir == Direction::SOUTH ? -1 : 0;
    return walk_until(stream, context, x, y, limit, label);
}

//  A rough check that we stand in a Pokemon Center: its floor fills the lower
//  half of the screen with warm yellow, where the gym is grey and routes are
//  green. After a battle it tells a loss (a whiteout ends at the counter)
//  from a win.
bool in_pokemon_center(VideoStream& stream){
    const VideoSnapshot frame = stream.video().snapshot();
    if (!frame){
        return false;
    }
    const ImageRGB32 small = frame.frame->scale_to(192, 108);
    size_t warm = 0, total = 0;
    for (size_t y = 54; y < 108; y++){
        for (size_t x = 0; x < 192; x++){
            const uint32_t pixel = small.pixel(x, y);
            const int r = (pixel >> 16) & 0xff, g = (pixel >> 8) & 0xff, b = pixel & 0xff;
            total++;
            if (r > 200 && g > 150 && b < 150 && r - b > 70){
                warm++;
            }
        }
    }
    const bool inside = warm * 10 > total * 3;
    stream.log("[Center check] warm floor " + std::to_string(warm * 100 / total) + " %: " +
        (inside ? "in a Pokemon Center." : "not in a Pokemon Center."));
    return inside;
}

}
}
}
