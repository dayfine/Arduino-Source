/*  BDSP AutoStory - Events: Route 204 to Floaroma (01.09, 01.11)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Ravaged Path's Rock Smash, and segment 01's walks to the old couple
 *  in Floaroma's flower field, as they ran on the console (2026-10-31).
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_SelectionArrow.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  "This rock seems breakable with a hidden move. Would you like to use Rock
//  Smash?" is open, from an A at a smashable rock. Its Yes/No box comes only
//  once the text is advanced again (by hand, 2026-10-31: the text complete
//  1.2 s after the first A, the box 1.2 s after a second), and r237a's timed
//  A, A, B put its B on that box: No. So press A until the box's cursor
//  shows, then once more for Yes, and wait out the move. False if no box
//  shows: whatever is open is not this prompt.
bool answer_rock_smash_prompt(VideoStream& stream, ProControllerContext& context, const std::string& label){
    static const ImageFloatBox YES_NO_ARROW_BOX{0.700, 0.620, 0.070, 0.120};
    SelectionArrowFinder yes_no(stream.overlay(), YES_NO_ARROW_BOX, COLOR_GREEN);
    for (size_t press = 0; press < 5; press++){
        context.wait_for(1200ms);
        context.wait_for_all_requests();
        const VideoSnapshot screen = stream.video().snapshot();
        if (screen && yes_no.detect(*screen.frame)){
            stream.log("[Route] " + label + ": Rock Smash offered; Yes.");
            pbf_press_button(context, BUTTON_A, 80ms, 0ms);
            context.wait_for(5000ms);
            if (dialogue_opens(stream, context, 500ms)){
                finish_dialogue(stream, context, BUTTON_B, label + ": after Rock Smash");
            }
            return true;
        }
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    }
    stream.log("[Route] " + label + ": no Yes/No box for Rock Smash.", COLOR_RED);
    return false;
}

//  01.09: Rock Smash on the rock east of us, from the tile the route ends on
//  facing it. Only the prompt's Yes/No box tells the rock from someone in
//  the way (r237a: the walker took one for a person and said No).
void smash_the_rock(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, Direction::EAST);
    if (!talk_with_nudges(stream, context, run_from_wild_battle, Direction::EAST, "the rock east of us", 2000ms)){
        fail_step(stream, "Ravaged Path: nothing answered facing the rock east of us.");
    }
    if (!answer_rock_smash_prompt(stream, context, "the rock east of us")){
        fail_step(stream, "Ravaged Path: no Rock Smash offer facing the rock east of us.");
    }
}

//  01.11: west 8 along the flower field's bottom row, north until the old woman stops us.
void west_8_north_to_the_old_woman(VideoStream& stream, ProControllerContext& context){
    {
        const WalkResult west = walk_tiles_dpad(stream, context, run_from_wild_battle, {-1, 0}, 8, "west 8 to the flower field");
        if (west.stop != WalkStop::LIMIT || west.moved_chunks < 7){
            fail_step(stream, "Floaroma: west 8 to the flower field moved " + std::to_string(west.moved_chunks) + " tiles.");
        }
        //  Blocked, or out of taps with the last ones bumping: either way
        //  she is in front of us (r250a: 3 steps, 2 bumps, "limit").
        const WalkResult north = walk_tiles_dpad(stream, context, run_from_wild_battle, {0, 1}, 5, "north until the old woman");
        if (north.stop != WalkStop::BLOCKED && north.moved_chunks >= 5){
            fail_step(stream, "Floaroma: nothing stopped the walk north to the old woman.");
        }
    }
}

//  01.11: one tile west, then a tap north: the old man stands there, so the tap only turns.
void below_the_old_man(VideoStream& stream, ProControllerContext& context){
    const WalkResult west = walk_tiles_dpad(stream, context, run_from_wild_battle, {-1, 0}, 1, "west 1 below the old man");
    if (west.moved_chunks < 1){
        fail_step(stream, "Floaroma: the step west below the old man did not move.");
    }
    // He stands there, so one tap only turns us to him; the talk after fails
    // if nobody answers. No counting: Floaroma's cloud shadows read as moves
    // (r84: blocked taps counted, view change 20).
    pbf_press_dpad(context, DPAD_UP, 160ms, 500ms);
    context.wait_for_all_requests();
}

}


void add_route204_events(StoryEvents& events){
    events.emplace("smash_the_rock", console_event(smash_the_rock));
    events.emplace("west_8_north_to_the_old_woman", console_event(west_8_north_to_the_old_woman));
    events.emplace("below_the_old_man", console_event(below_the_old_man));
}



}
}
}
