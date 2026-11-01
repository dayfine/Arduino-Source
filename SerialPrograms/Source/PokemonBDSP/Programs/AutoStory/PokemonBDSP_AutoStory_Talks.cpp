/*  BDSP AutoStory - Talks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Talking to people and fighting the trainers among them, and the looks
 *  at the screen the story's events decide by.
 *
 */

#include <chrono>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Bag.h"
#include "PokemonBDSP_AutoStory_Battle.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_Talks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


[[noreturn]] void fail_step(VideoStream& stream, const std::string& message){
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, message, stream);
}

//  True if a dialogue opens within `timeout`: someone who stopped us as the
//  walk ended reads as BLOCKED, and their box comes up a moment later.
bool dialogue_opens(VideoStream& stream, ProControllerContext& context, Milliseconds timeout){
    context.wait_for_all_requests();
    ShortDialogWatcher dialog(COLOR_CYAN);
    return wait_until(stream, context, timeout, {{dialog}}) == 0;
}

bool talk_or_fight(VideoStream& stream, ProControllerContext& context, const std::string& label, TalkExpect expect, BattleFormat format){
    ShortDialogDetector dialog;
    OverworldDetector overworld;
    BattleMenuDetector battle_menu(BattleType::TRAINER);
    //  Roark's battle menu read as STANDARD, not TRAINER, and nothing fought
    //  it (r210d): either counts here, as only a trainer's battle can start.
    BattleMenuDetector battle_menu_standard(BattleType::STANDARD);
    BlackScreenDetector black;
    auto quiet_since = std::chrono::steady_clock::now();
    for (size_t check = 0; check < 120; check++){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        if (!frame){
            fail_step(stream, label + ": No video frame.");
        }
        if (battle_menu.detect(*frame.frame) || battle_menu_standard.detect(*frame.frame) || black.detect(*frame.frame)){
            route_status(stream, "Fighting " + label + ".");
            fight_story_battle(stream, context, label, format);
            // Lines after the battle (Roark's badge, r211d) keep the menu
            // from opening: finish them before the Potion.
            if (dialogue_opens(stream, context, 3000ms)){
                finish_dialogue(stream, context, BUTTON_B, label + " after the battle");
            }
            if (!in_pokemon_center(stream)){
                use_potion_first_pokemon(stream, context, 0.5);
            }
            return true;
        }
        const bool talking = dialog.detect(*frame.frame);
        if (!talking && overworld.detect(*frame.frame)){
            if (expect != TalkExpect::BATTLE && std::chrono::steady_clock::now() - quiet_since >= 4s){
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
    fail_step(stream, label + ": neither a battle nor the overworld after the dialogue.");
}

//  A rough check that we stand in a Pokemon Center: its floor fills the lower
//  half of the screen with warm yellow, where the gym is grey and routes are
//  green. After a battle it tells a loss (a whiteout ends at the counter)
//  from a win.
bool center_floor_on_screen(const ImageViewRGB32& screen, size_t* percent){
    const ImageRGB32 small = screen.scale_to(192, 108);
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
    if (percent){
        *percent = warm * 100 / total;
    }
    return warm * 10 > total * 3;
}

bool in_pokemon_center(VideoStream& stream){
    const VideoSnapshot frame = stream.video().snapshot();
    if (!frame){
        return false;
    }
    size_t percent = 0;
    const bool inside = center_floor_on_screen(*frame.frame, &percent);
    stream.log("[Center check] warm floor " + std::to_string(percent) + " %: " +
        (inside ? "in a Pokemon Center." : "not in a Pokemon Center."));
    return inside;
}

//  The Oreburgh Mine's hall and corridors are dim: mean brightness 45 to 64
//  on the console, against 99 to 116 for the city and its yard (2026-10-31).
bool in_dim_cave(VideoStream& stream, uint64_t below){
    const VideoSnapshot frame = stream.video().snapshot();
    if (!frame){
        return false;
    }
    const ImageRGB32 small = frame.frame->scale_to(192, 108);
    uint64_t sum = 0;
    for (size_t y = 0; y < 108; y++){
        for (size_t x = 0; x < 192; x++){
            const uint32_t pixel = small.pixel(x, y);
            sum += ((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff);
        }
    }
    const uint64_t mean = sum / (3 * 192 * 108);
    stream.log("[Cave check] mean brightness " + std::to_string(mean) + ".");
    return mean < below;
}

//  Mean of the three channels over a small copy of the frame, 0 to 255.
uint64_t mean_brightness(const ImageViewRGB32& screen){
    const ImageRGB32 small = screen.scale_to(192, 108);
    uint64_t sum = 0;
    for (size_t y = 0; y < 108; y++){
        for (size_t x = 0; x < 192; x++){
            const uint32_t pixel = small.pixel(x, y);
            sum += ((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff);
        }
    }
    return sum / (3 * 192 * 108);
}

// Fights a trainer whose dialogue has opened, with whatever starter we
// have. Someone who only talks leaves no battle to fight.
void fight_route_trainer(VideoStream& stream, ProControllerContext& context, const std::string& name){
    pbf_mash_button(context, BUTTON_B, 5000ms);
    context.wait_for_all_requests();
    BattleMenuWatcher battle_menu(BattleType::TRAINER, COLOR_YELLOW);
    BattleDialogWatcher battle_dialog(COLOR_YELLOW);
    if (wait_until(stream, context, 8s, {{battle_menu}, {battle_dialog}}) < 0){
        stream.log("[Route] " + name + ": no battle followed the talk.");
        pbf_mash_button(context, BUTTON_B, 2000ms);
        context.wait_for_all_requests();
        return;
    }
    fight_trainer(stream, context, name);
    context.wait_for_all_requests();
    // The next trainer may be a few tiles on: check HP now. With no
    // Potion in the Bag this only looks.
    use_potion_first_pokemon(stream, context, 0.5);
}

bool sign_text_matches(const std::string& text, const std::string& key){
    std::string lower;
    for (char ch : text){
        lower += (char)std::tolower((unsigned char)ch);
    }
    return lower.find(key) != std::string::npos;
}

namespace{

//  Where a sign's box shows: the bottom of the screen, as a dialogue's. The
//  Lab's is grey, Route 201's green; opening either changes the area far
//  more than the overworld behind it moves standing still.
const ImageFloatBox SIGN_BOX(0.19, 0.80, 0.64, 0.18);
//  Its text, inside the margins.
const ImageFloatBox SIGN_TEXT(0.22, 0.82, 0.58, 0.15);
constexpr double SIGN_BOX_OPENED = 20.0;

bool dialogue_showing(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot screen = stream.video().snapshot();
    return screen && ShortDialogDetector().detect(*screen.frame);
}

//  After a talk opened: reads the first box, then presses `next` once
//  per line, 1.5 s apart, and only while a box shows: a press after it
//  has closed would start the talk over.
void finish_talk(VideoStream& stream, ProControllerContext& context, DialogueRead& read, Button next, const std::string& label){
    //  The first box's text, inside its white margins.
    //  For the log only; a talk is checked by its box opening and closing.
    VideoSnapshot screen = stream.video().snapshot();
    if (screen && story_reads_text()){
        read.text = OCR::ocr_read(story_language(), extract_box_reference(*screen.frame, SIGN_TEXT), OCR::PageSegMode::SINGLE_BLOCK);
    }
    stream.log("[Talk] " + label + ": the dialogue reads \"" + read.text + "\".");
    size_t quiet = 0;
    //  Barry's scene, to the Town Map, took more than 20 (r150a).
    for (size_t presses = 0; presses < 80 && quiet < 2; presses++){
        ShortDialogWatcher dialog(COLOR_YELLOW);
        if (wait_until(stream, context, 1500ms, {{dialog}}) == 0){
            quiet = 0;
            pbf_press_button(context, next, 80ms, 0ms);
        }else{
            quiet++;
        }
    }
    read.closed = !dialogue_showing(stream, context);
    stream.log("[Talk] " + label + (read.closed ? ": closed." : ": still open."));
}

}

DialogueRead talk_facing(VideoStream& stream, ProControllerContext& context, Direction direction, Button next, const std::string& label){
    route_turn(stream, context, direction);
    return talk_ahead(stream, context, next, label);
}

DialogueRead talk_ahead(VideoStream& stream, ProControllerContext& context, Button next, const std::string& label){
    DialogueRead read;
    pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    context.wait_for_all_requests();
    ShortDialogWatcher opened(COLOR_CYAN);
    if (wait_until(stream, context, 2000ms, {{opened}}) != 0){
        stream.log("[Talk] " + label + ": no dialogue opened.");
        return read;
    }
    read.opened = true;
    finish_talk(stream, context, read, next, label);
    return read;
}

DialogueRead talk_walking(VideoStream& stream, ProControllerContext& context, Direction direction, size_t tiles, const std::string& label){
    DialogueRead read;
    const auto [x, y] = dpad_of(direction);
    const bool across = direction == Direction::EAST || direction == Direction::WEST;
    const Milliseconds limit = (int64_t)tiles * (across ? ROUTE_MS_PER_TILE_ACROSS : ROUTE_MS_PER_TILE_UP_DOWN) + 800ms;
    const WalkResult walked = walk_to_talk(stream, context, run_from_wild_battle, {x, y}, limit, label);
    if (walked.stop != WalkStop::DIALOG){
        stream.log("[Talk] " + label + ": nobody answered.");
        return read;
    }
    read.opened = true;
    finish_talk(stream, context, read, BUTTON_A, label);
    return read;
}

DialogueRead read_sign(VideoStream& stream, ProControllerContext& context, Direction facing, const std::string& label){
    DialogueRead read;
    route_turn(stream, context, facing);
    //  Let the camera settle after the turn: its drift read as a box
    //  opening (r132a).
    pbf_wait(context, 800ms);
    context.wait_for_all_requests();
    VideoSnapshot before = stream.video().snapshot();
    //  The text types out over about a second; B before it has finished
    //  only finishes it (r131a).
    pbf_press_button(context, BUTTON_A, 80ms, 1500ms);
    context.wait_for_all_requests();
    VideoSnapshot shown = stream.video().snapshot();
    if (!before || !shown){
        return read;
    }
    const double opened = walk_screen_change(extract_box_reference(*before.frame, SIGN_BOX), extract_box_reference(*shown.frame, SIGN_BOX));
    read.opened = opened > SIGN_BOX_OPENED;
    if (!read.opened){
        stream.log("[Sign] " + label + ": no box opened (change " + std::to_string(opened) + ").");
        return read;
    }
    //  Tesseract read the Lab's white-on-grey text as "e R O e IO" (r131a).
    //  Only to say which sign, when there is a reader for the game's
    //  language; the box opening and closing is the check.
    if (story_reads_text()){
        read.text = OCR::ocr_read(story_language(), extract_box_reference(*shown.frame, SIGN_TEXT), OCR::PageSegMode::SINGLE_BLOCK);
    }
    stream.log("[Sign] " + label + ": reads \"" + read.text + "\" (change " + std::to_string(opened) + ").");
    for (size_t presses = 0; presses < 3 && !read.closed; presses++){
        pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
        context.wait_for_all_requests();
        VideoSnapshot after = stream.video().snapshot();
        read.closed = after && walk_screen_change(extract_box_reference(*before.frame, SIGN_BOX), extract_box_reference(*after.frame, SIGN_BOX)) < SIGN_BOX_OPENED;
    }
    stream.log("[Sign] " + label + (read.closed ? ": closed." : ": still open."));
    return read;
}



}
}
}
